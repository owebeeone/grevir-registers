#pragma once

#include <grevir/registers/access.hpp>

namespace setl {

// For FindRegisterForField.
namespace nfp {
template <typename w_BitField, typename...w_Registers>
struct FindRegisterForFieldHelper;

template <typename w_BitField>
struct FindRegisterForFieldHelper<w_BitField> {
  using type = void;
  static constexpr bool value = false;
};

template <typename w_BitField, typename w_Register, typename...w_Registers>
struct FindRegisterForFieldHelper<w_BitField, w_Register, w_Registers...> {
private:
  static constexpr bool selected = w_Register::FormatType::template contains<w_BitField>;
public:
  using type = typename std::conditional<selected,
    w_Register,
    typename FindRegisterForFieldHelper<w_BitField, w_Registers...>::type>::type;
  static constexpr bool value = std::conditional<selected,
    std::true_type,
    FindRegisterForFieldHelper<w_BitField, w_Registers...>>::type::value;
};
}  // namespace nfp

/**
 * Find the register in the given tuple that supports the given bit field.
 * value is true if the supporting register is found.
 */
template <typename w_BitField, typename w_RegistersTuple>
struct FindRegisterForField;

template <typename w_BitField, typename...w_Registers>
struct FindRegisterForField<w_BitField, std::tuple<w_Registers...>> {
private:
  using HelperType = nfp::FindRegisterForFieldHelper<w_BitField, w_Registers...>;
public:
  // True if the corresponding register for bitfield was found.
  static constexpr bool value = HelperType::value;
  /// Register that supports the bitfield.
  using type = typename HelperType::type;
};

template <typename...Rs>
struct RegisterSelectorHelper;

template <>
struct RegisterSelectorHelper<> {

};

template <typename R, typename...Rs>
struct RegisterSelectorHelper<R, Rs...> {
  // ?? Collect all the bitfields for R.
};

namespace nfp {

template <bool do_not_assert, typename Bs, typename Rs>
struct BitsToRegistersChecker;

template <bool do_not_assert, typename...Rs>
struct BitsToRegistersChecker<do_not_assert, std::tuple<>, std::tuple<Rs...>> {
  static constexpr bool value = true;
};

template <bool do_not_assert, typename B, typename...Bs, typename...Rs>
struct BitsToRegistersChecker<do_not_assert, std::tuple<B, Bs...>, std::tuple<Rs...>> {
  using MapperForRest = BitsToRegistersChecker<do_not_assert, std::tuple<Bs...>, std::tuple<Rs...>>;
  using FinderResult = FindRegisterForField<B, std::tuple<Rs...>>;
  static constexpr bool value = MapperForRest::value && FinderResult::value;

  static_assert(
    do_not_assert || FinderResult::value,
    "Bitfield was not contained in any of the registers provided.");
};

template <typename R, typename...Bs>
struct RegisterToBitsFinder;

template <typename R>
struct RegisterToBitsFinder<R> : std::false_type {};

template <typename R, typename B, typename...Bs>
struct RegisterToBitsFinder<R, B, Bs...> {
  using type = bool;
  static constexpr type value = R::FormatType::template contains<B>
      || RegisterToBitsFinder<R, Bs...>::value;
};

} // Namespace nfp

/**
 * Support bitfield operations across a number of registers, automatically
 * choosing the correct register for each associated bitfield.
 */
template <typename Tuple>
struct RegisterSelector;

template <>
struct RegisterSelector<std::tuple<>> {
  using Registers = std::tuple<>;

  template <typename...BitsTypes>
  static void ReadModifyWrite(BitsTypes...) {
    static_assert(sizeof...(BitsTypes) == 0,
      "Bitfield was not contained in any of the registers provided.");
  }

  template <typename...BitsTypes>
  static void Read(BitsTypes&...) {
    static_assert(sizeof...(BitsTypes) == 0,
      "Bitfield was not contained in any of the registers provided.");
  }

  template <typename BitsType>
  static typename BitsType::type Read() {
    BitsType value;
    Read(value);
    return value.value;
  }
};

namespace nfp { // For internal use only. This API can change.

// Helper class to find and select operations for registers provided.
template <typename Rt, typename Bt, bool selected>
struct RegisterSelectorHelper;

template <typename R0, typename...Bs>
struct RegisterSelectorHelper<std::tuple<R0>, std::tuple<Bs...>, false> {
  // Last register's RMW function must exist.
  static void ReadModifyWrite(Bs...bitsValues) {
  }

  // Last register's assign function must exist.
  static void Read(Bs&...bitsValues) {
  }
};

template <typename R0, typename...Bs>
struct RegisterSelectorHelper<std::tuple<R0>, std::tuple<Bs...>, true> {
  static void ReadModifyWrite(Bs...bitsValues) {
    R0::template ReadModifyWriteEx<true>(bitsValues...);
  }

  static void Read(Bs&...bitsValues) {
    Assigner<Bs...>(bitsValues...).AssignSparse(R0::Read());
  }
};

// If a register does not participate with the given bitfields, then
// no fuctions to read or or write are provided.
template <typename R0, typename R1, typename...Rs, typename...Bs>
struct RegisterSelectorHelper<std::tuple<R0, R1, Rs...>, std::tuple<Bs...>, false>
  : RegisterSelectorHelper<std::tuple<R1, Rs...>,
                           std::tuple<Bs...>,
                           RegisterToBitsFinder<R1, Bs...>::value> {
};

template <typename R0, typename R1, typename...Rs, typename...Bs>
struct RegisterSelectorHelper<std::tuple<R0, R1, Rs...>, std::tuple<Bs...>, true>
  : RegisterSelectorHelper<std::tuple<R1, Rs...>,
                           std::tuple<Bs...>,
                           RegisterToBitsFinder<R1, Bs...>::value> {

  using super = RegisterSelectorHelper<std::tuple<R1, Rs...>,
    std::tuple<Bs...>,
    RegisterToBitsFinder<R1, Bs...>::value>;

  static void ReadModifyWrite(Bs...bitsValues) {
    R0::template ReadModifyWriteEx<true>(bitsValues...);
    super::ReadModifyWrite(bitsValues...);
  }

  static void Read(Bs&...bitsRefs) {
    // Assign the bit fields for this register.
    Assigner<Bs...>(bitsRefs...).AssignSparse(R0::Read());
    // Assign the bit fields for the next available register.
    super::Read(bitsRefs...);
  }
};

}  // namespace nfp

/**
 * Provides multi-register operations. Sometimes bitfields are defined in
 * different registers for semantically the same bitfields. This allows
 * client code to specify all the possible destination registers and the
 * required bitfield values to operate on. Only the registers whose bitfields
 * are present will have their values changed.
 */
template <typename R, typename...Rs>
struct RegisterSelector<std::tuple<R, Rs...>> {
 private:
  template <typename...BitsTypes>
  static void ReadModifyWriteImpl(BitsTypes...bitsValues) {
    static constexpr bool contained = nfp::RegisterToBitsFinder<R, BitsTypes...>::value;
    using RSHelper = nfp::RegisterSelectorHelper<
        std::tuple<R, Rs...>, std::tuple<BitsTypes...>, contained>;

    // RSHelper::ReadModifyWrite functions are only provided for registers whose
    // bitfields are present in the parameters.
    RSHelper::ReadModifyWrite(bitsValues...);
  }
 public:
  using Registers = std::tuple<R, Rs...>;

  /**
   * Perform a write operation with the given bit field values by first reading
   * the register (if any bits will survive the write) and then writing the new
   * values in a single write.
   */
  template <typename...BitsTypes>
  static void ReadModifyWrite(BitsTypes...bitsValues) {
    // Make sure all bits are used.
    static_assert(
      nfp::BitsToRegistersChecker<
          false, std::tuple<BitsTypes...>, std::tuple<R, Rs...>>::value,
      "A bitfield was not contained in any of the registers provided.");
    ReadModifyWriteImpl(bitsValues...);
  }

  /**
   * Read the passed in bit field values from the selected registers.
   * Only the registers whose associated bitfields will be read and only once.
   */
  template <typename...BitsTypes>
  static void Read(BitsTypes&...bitsRefs) {
    // Make sure all bit types can be read.
    static_assert(
      nfp::BitsToRegistersChecker<
      false, std::tuple<BitsTypes...>, std::tuple<R, Rs...>>::value,
      "Bitfield was not contained in any of the registers provided.");

    static constexpr bool contained = nfp::RegisterToBitsFinder<R, BitsTypes...>::value;
    using RSHelper = nfp::RegisterSelectorHelper<
      std::tuple<R, Rs...>, std::tuple<BitsTypes...>, contained>;
    RSHelper::Read(bitsRefs...);
  }

  template <typename BitsType>
  static typename BitsType::type Read() {
    // Make sure all bit types can be read.
    BitsType bits_value;
    Read(bits_value);
    return bits_value.value;
  }
};

/**
 * Provides a utility to read a bitfield from a register selection..
 */
template <typename w_BitField>
struct ApplierReader {
  using BitField = w_BitField;

  template <typename Register>
  static BitField read() {
    BitField value;
    Register::Read(value);
    return value;
  }
};

// Trap any attempts to read from a void bitfield.
template <>
struct ApplierReader<void> {
  using BitField = void;

  template <typename Register>
  static void read() {
  }
};

} // namespace setl
