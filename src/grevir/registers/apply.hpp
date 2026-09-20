#pragma once

#include <grevir/registers/selection.hpp>

namespace setl {

/**
 * Defines a bitfield and a corrsponding value.
 */
template <typename w_BitField, typename w_BitField::type w_value>
struct ApplierValue {
  template <typename...w_Appliers>
  friend struct Appliers;
  template <typename v_BitField, typename v_BitField::type v_value, typename v_Register>
  friend struct Applier;

  using BitField = w_BitField;
  static constexpr typename BitField::type value = w_value;

 private:
  template <typename Register>
  static void apply() {
    Register::ReadModifyWrite(BitField{ w_value });
  }
};

// Tools for value applier.
namespace nfp {

// Convert a list of ApplierValue types to a list of bitfields.
template <typename...AVs>
struct ToBitFields;

template <>
struct ToBitFields<> {
  using type = std::tuple<>;
};

template <typename AV, typename...AVs>
struct ToBitFields<AV, AVs...> {
  using Rest = ToBitFields<AVs...>;
  using type = setl::tuple_concat_t<
      std::tuple<typename AV::BitField>, typename Rest::type>;
};

// Collects all the ApplierValue types specific to register R.
template <typename AVT, typename R>
struct ApplierValuesRegHelper;

template <typename R>
struct ApplierValuesRegHelper<std::tuple<>, R> {
  using AVsForRegister = std::tuple<>;
};

template <typename AV, typename...AVs, typename R>
struct ApplierValuesRegHelper<std::tuple<AV, AVs...>, R> {
  using Rest = ApplierValuesRegHelper<std::tuple<AVs...>, R>;
  static constexpr bool contains = R::FormatType::template contains<typename AV::BitField>;
  using AVsForRegister = std::conditional_t<
      contains,
      setl::tuple_concat_t<std::tuple<AV>, typename Rest::AVsForRegister>,
      typename Rest::AVsForRegister>;
};


template <typename w_unsigned_type, typename w_FormatType, typename...AVs>
struct ApplierValueEvaluator;

template <typename w_unsigned_type, typename w_FormatType>
struct ApplierValueEvaluator<w_unsigned_type, w_FormatType> {
  using unsigned_type = w_unsigned_type;
  static constexpr unsigned_type value = unsigned_type{0};
};


template <typename w_unsigned_type, typename w_FormatType, typename AV, typename...AVs>
struct ApplierValueEvaluator<w_unsigned_type, w_FormatType, AV, AVs...> {
  using unsigned_type = w_unsigned_type;
  using FormatType = w_FormatType;
  using Rest = ApplierValueEvaluator<unsigned_type, FormatType, AVs...>;
  using Bitfields = typename ToBitFields<AV, AVs...>::type;
  using Evaluator = BitTypesEvaluator<false, FormatType, Bitfields>;
  using traits = typename Evaluator::traits;
  using inv_applier = typename traits::appliers::inv_applier;

  static constexpr unsigned_type this_value = inv_applier::convert(
    static_cast<unsigned_type>(AV::value));

  static constexpr unsigned_type value = this_value | Rest::value;
};

template <typename AVT, typename R, typename w_Base>
struct ApplierValuesForRegister;

template <typename R, typename w_Base>
struct ApplierValuesForRegister<std::tuple<>, R, w_Base> {
  using ApplierT = w_Base;
};

enum class AVsApplierType {
  partial_write,
  full_mask,
  null_op
};

// Default case for non zero and non all set bits mask.
template<
    typename w_Register,
    typename w_Base,
    typename w_register_type,
    w_register_type w_new_bits,
    w_register_type w_in_mask,
    AVsApplierType w_applier_type>
struct AVsApplier : w_Base {
  using Register = w_Register;
  using Base = w_Base;
  using register_type = w_register_type;
  static constexpr w_register_type new_bits = w_new_bits;
  static constexpr w_register_type in_mask = w_in_mask;
  static void apply() {
    Base::apply();
    // Uses the read/modify write API on the register.
    Register::ioregister::set_mask(
      static_cast<register_type>(new_bits), static_cast<register_type>(in_mask));
  }
};

// All bits being written, no point in doing a read.
template<
    typename w_Register,
    typename w_Base,
    typename w_register_type,
    w_register_type w_new_bits,
    w_register_type w_in_mask>
struct AVsApplier<
    w_Register,
    w_Base,
    w_register_type,
    w_new_bits,
    w_in_mask,
    AVsApplierType::full_mask> : w_Base {
  using Register = w_Register;
  using Base = w_Base;
  using register_type = w_register_type;
  static constexpr w_register_type new_bits = w_new_bits;
  static constexpr w_register_type in_mask = w_in_mask;
  static void apply() {
    Base::apply();
    Register::ioregister::set(static_cast<register_type>(new_bits));
  }
};

// Register is not being accessed hence no apply() function.
template<
    typename w_Register,
    typename w_Base,
    typename w_register_type,
    w_register_type w_new_bits,
    w_register_type w_in_mask>
struct AVsApplier<
    w_Register,
    w_Base,
    w_register_type,
    w_new_bits,
    w_in_mask,
    AVsApplierType::null_op> : w_Base {
  using Register = w_Register;
  using Base = w_Base;
  using register_type = w_register_type;
  static constexpr w_register_type new_bits = w_new_bits;
  static constexpr w_register_type in_mask = w_in_mask;
};


template <typename AV, typename...AVs, typename R, typename w_Base>
struct ApplierValuesForRegister<std::tuple<AV, AVs...>, R, w_Base> {
  using Base = w_Base;
  using Bitfields = typename ToBitFields<AV, AVs...>::type;
  using Register = R;
  using FormatType = typename Register::FormatType;
  using Evaluator = BitTypesEvaluator<false, FormatType, Bitfields>;
  using unsigned_type = typename Evaluator::unsigned_type;
  using ApplierEvaluator = ApplierValueEvaluator<
      unsigned_type, FormatType, AV, AVs...>;
  static constexpr unsigned_type new_bits = ApplierEvaluator::value;
  static constexpr unsigned_type in_mask = Evaluator::traits::in_mask;
  using register_type = typename Register::type;

  static constexpr AVsApplierType op =
    (in_mask == register_type{0}) ? AVsApplierType::null_op
    : (in_mask == static_cast<register_type>(~register_type{0})) ? AVsApplierType::full_mask
    : AVsApplierType::partial_write;


  using ApplierT = AVsApplier<Register, Base, register_type, new_bits, in_mask, op>;
};

// Appliers chain via inheritance and only those Appliers containing
// values to apply implement the apply() method. This means the compiler
// only needs to generate apply() functions for registers that have
// mutating values.
struct BaseApplier {
  // The applier that does nothing.
  struct ApplierT {
    static void apply() {
    }
  };
};

template <typename AVT, typename RT>
struct ApplierValuesRegApplier;

template <typename...AVs>
struct ApplierValuesRegApplier<std::tuple<AVs...>, std::tuple<>> {

  using ApplierT = BaseApplier::ApplierT;
};


template <typename...AVs, typename R, typename...Rs>
struct ApplierValuesRegApplier<std::tuple<AVs...>, std::tuple<R, Rs...>> {
  using RegAvs = ApplierValuesRegHelper<std::tuple<AVs...>, R>;
  using RegAvsTuple = typename RegAvs::AVsForRegister;
  //using Bitfields = typename ToBitFields<AVs...>::type;
  //static_assert(
  //  nfp::BitsToRegistersChecker<false, BitFields, std::tuple<R, Rs...>>::value,
  //  "A bitfield was not contained in any of the registers provided.");

  using Rest = ApplierValuesRegApplier<std::tuple<AVs...>, std::tuple<Rs...>>;
  using BaseApplier = typename Rest::ApplierT;

  using ApplierT = typename ApplierValuesForRegister<
      RegAvsTuple, R, BaseApplier>::ApplierT;
};

}  // namespace nfp

/**
 * Appliers provides a template class that may be used to perform a number
 * bitfield operations.
 */
template <typename...w_ApplierValues>
struct ApplierValues {
  using ApplierValueTypes = std::tuple<w_ApplierValues...>;

  template <typename w_RegisterSelector>
  static void apply() {
    using Registers = typename w_RegisterSelector::Registers;
    using AVApplier = nfp::ApplierValuesRegApplier<ApplierValueTypes, Registers>;
    using BitFields = typename nfp::ToBitFields<w_ApplierValues...>::type;
    static_assert(
      nfp::BitsToRegistersChecker<false, BitFields, Registers>::value,
      "A bitfield was not contained in any of the registers provided.");
    AVApplier::ApplierT::apply();
  }
};

/**
 * Appliers provides a template class that may be used to perform a number
 * bitfield operations.
 */
template <typename...w_Appliers>
struct Appliers;

/**
 * Applied the given bit field in a ReadModifyWrite.
 */
template <typename w_BitField, typename w_BitField::type w_value, typename w_Register>
struct Applier {
  template <typename...w_Appliers>
  friend struct Appliers;

  using ApplierValueType = ApplierValue<w_BitField, w_value>;
  using Register = w_Register;

 private:
  static void apply() {
    ApplierValueType::template apply<Register>();
  }
};

/**
 * Applier taking a set of registers in a tuple. The register selected
 * is the register that supports the given field.
 */
template <typename w_BitField, typename w_BitField::type w_value, typename...w_Registers>
struct Applier<w_BitField, w_value, std::tuple<w_Registers...>> {
  template <typename...w_Appliers>
  friend struct Appliers;

  using Finder = setl::FindRegisterForField<w_BitField, std::tuple<w_Registers...>>;
  static_assert(Finder::value, "Given bitfield is not supported in the given registers.");
  using Register = typename Finder::type;

 private:
  static void apply() {
    Register::ReadModifyWrite(w_BitField{ w_value });
  }
};

template <>
struct Appliers<> {
  template <typename...v_Appliers>
  friend struct Appliers;

  using types = std::tuple<>;
  static void apply() {
  }
};

template <typename w_Applier, typename...w_Appliers>
struct Appliers<w_Applier, w_Appliers...> {
  template <typename...v_Appliers>
  friend struct Appliers;
  friend struct ApplierRunner;

  using types = std::tuple<w_Applier, w_Appliers...>;

private:
  static void apply() {
    w_Applier::apply();
    Appliers<w_Appliers...>::apply();
  }
};

/**
 * Provides "apply" frunctions that execute the given bit Appliers. One of the
 * functions performs a synchronization (memory barrier) as may be needed on some
 * microcontrollers.
 */
struct ApplierRunner {
protected:
  /// Applies the given "Appliers" within a caller-supplied RAII barrier scope.
  /// The barrier policy owns platform synchronization; there is no default.
  template <typename w_Appliers, typename MemoryBarrier>
  static void applySync() {
    MemoryBarrier barrier;
    w_Appliers::apply();
  }

  /// Applies the given "Appliers".
  template <typename w_Appliers>
  static void applyNoSync() {
    w_Appliers::apply();
  }
};

} // namespace setl
