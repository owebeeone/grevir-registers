#pragma once

#include <grevir/registers/bit_mapping.hpp>

namespace setl {

/**
 * The value type for a given Format. This can represent the value
 * of an MCU register containing potentially many "bit field" values.
 */
template <typename w_FormatType>
struct BitValue {
  using FormatType = w_FormatType;
  using type = typename FormatType::type;

  template <typename w_BitType>
  static constexpr bool contains = FormatType::template contains<w_BitType>;

  type value;
};

/**
 * Describes the allowable operations on a bit field.
 */
enum class BitOps : unsigned {
  ReadOnly,
  WriteOnly,
  ReadWrite
};

/**
 * Provides a distinguising type for Bits types.
 */
template <std::uint64_t w_distinguisher_code, typename T>
struct SemanticType {
  using type = T;
  static constexpr ::uint64_t distinguisher_code = w_distinguisher_code;
};

/**
 * Provides a constant evaluator for a hash code from a string.
 */
inline constexpr std::uint32_t hash(const char* s, unsigned shift = 1) {
  return s[0] ? std::uint32_t(s[0]) ^ shift * hash(s + 1, shift + 1) : 0;
}

template <typename T>
struct GetTypeOfHelper {
  using type = T;
};

template <std::uint64_t w_distinguisher_code, typename T>
struct GetTypeOfHelper<SemanticType<w_distinguisher_code, T>> {
  using type = T;
};

template <typename T>
using GetTypeOf = typename GetTypeOfHelper<T>::type;

template <BitOps w_ops, typename T, unsigned...bits>
struct BitsImpl {
  static constexpr BitOps ops{ w_ops };
  static constexpr unsigned max_bits{Max<bits...>::value};
  using type = T;
  using shift_type = UnsignedTypeForMaxBits<max_bits + 1>;
  using Sequence = UintList<bits...>;
  template <typename UT>
  using ShiftMaskInfo = GroupMaskShifts<UT, bits...>;

  BitsImpl() = default;
  explicit BitsImpl(const type& value)
    : value{value}
  {}

  operator type() {
    return value;
  }

  template <typename w_FormatType>
  static type read_from(const BitValue<w_FormatType>& value) {

    using unsigned_type = typename UnsignedType<typename w_FormatType::type, type>::type;

    using applier = typename BitsImpl::template ShiftMaskInfo<unsigned_type>::group::template apply<ApplyMaskShift>;
    return static_cast<type>(
      applier::convert(static_cast<unsigned_type>(value.value)));
  }

  template <typename w_FormatType>
  void read_value(const BitValue<w_FormatType>& value) {
    this->value = read_from(value);
  }

  // Instantiations of this class contain the value read or written.
  type value{};
};

// namespace nfp is not part of the public API.
namespace nfp {
// Creates the bits... parameter for Bits if not specified. The idea is to allow for omission
// of the bits... parameter for Bits when the register's value is the complete set of bits.
template <typename U, unsigned N, unsigned...bits>
struct DeriveBitsFor;

// Case when the bits... parameter is specified in the original BitsXX declaration.
template <unsigned N, unsigned...orig, unsigned...bits>
struct DeriveBitsFor<UintList<orig...>, N, bits...> {
  using uint_bits = UintList<orig...>;
};

// Case when the bits... parameter is not specified in the original BitsXX declaration
// and we recursively create it.
template <unsigned N, unsigned...bits>
struct DeriveBitsFor<UintList<>, N, bits...> {
  using uint_bits = typename DeriveBitsFor<UintList<>, N - 1, bits..., N - 1>::uint_bits;
};

template <unsigned...bits>
struct DeriveBitsFor<UintList<>, 0, bits...> {
  using uint_bits = UintList<bits...>;
};

template <typename U, unsigned N, unsigned...bits>
using DeriveBitsFor_t = typename DeriveBitsFor<U, N, bits...>::uint_bits;

template <typename T>
struct MakeBitsFor;

template <unsigned...bits>
struct MakeBitsFor<UintList<bits...>> {
  template <BitOps w_ops, typename T>
  using type = BitsImpl<w_ops, T, bits...>;
};

} // namespace nfp

// Handles filling in of ...bits when omitted.
template <BitOps w_ops, typename T, unsigned...bits>
using Bits = typename nfp::MakeBitsFor<nfp::DeriveBitsFor_t<
    UintList<bits...>,
    sizeof(T) * 8 - 1,
    sizeof(T) * 8 - 1>>::template type<w_ops, T>;

template <typename T, unsigned...bits>
struct BitsRO : Bits<BitOps::ReadOnly, GetTypeOf<T>, bits...> {
  using super = Bits<BitOps::ReadOnly, GetTypeOf<T>, bits...>;
  using type = GetTypeOf<T>;
  using outer_type = T;

  BitsRO() = default;

  BitsRO(const type& value)
    : super{ value }
  {}

  template <typename w_FormatType>
  BitsRO(const BitValue<w_FormatType>& value)
    : super{ super::read_from(value) }
  {
    static_assert(w_FormatType::template contains<BitsRO>,
      "Cannot assign value not containing result type");
  }

  template <typename w_FormatType>
  BitsRO& operator=(const BitValue<w_FormatType>& value) {
    static_assert(w_FormatType::template contains<BitsRO>,
      "Cannot assign value not containing result type");
    this->read_value(value);
    return *this;
  }
};

template <typename T, unsigned...bits>
struct BitsWO : Bits<BitOps::WriteOnly, GetTypeOf<T>, bits...> {
  using super = Bits<BitOps::WriteOnly, GetTypeOf<T>, bits...>;
  using type = GetTypeOf<T>;
  using outer_type = T;

  BitsWO() = default;

  BitsWO(const type& value)
    : super{ value }
  {}
};

template <typename T, unsigned...bits>
struct BitsRW : Bits<BitOps::ReadWrite, GetTypeOf<T>, bits...> {
  using super = Bits<BitOps::ReadWrite, GetTypeOf<T>, bits...>;
  using type = GetTypeOf<T>;
  using outer_type = T;

  BitsRW() = default;

  BitsRW(const type& value)
    : super{value}
  {}

  template <typename w_FormatType>
  BitsRW(const BitValue<w_FormatType>& value)
    : super{ super::read_from(value) }
  {
    static_assert(w_FormatType::template contains<BitsRW>,
      "Cannot assign value not containing result type");
  }

  template <typename w_FormatType>
  BitsRW& operator=(const BitValue<w_FormatType>& value) {
    static_assert(w_FormatType::template contains<BitsRW>,
      "Cannot assign value not containing result type");
    this->read_value(value);
    return *this;
  }
};

} // namespace setl
