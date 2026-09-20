#pragma once

#include <grevir/registers/bit_values.hpp>

namespace setl {

// Create list of types from tuple of Bits types.
template <typename...Bs>
struct TypesOfBits;

template <>
struct TypesOfBits<> {
  using type = std::tuple<>;
};

template <typename T, typename...Ts>
struct TypesOfBits<T, Ts...> {
  using type = tuple_concat_t<
      std::tuple<typename T::type>,
      typename TypesOfBits<Ts...>::type>;
};

// Create list of types from tuple of shift bits types.
template <typename...Bs>
struct TypesOfShiftBits;

template <>
struct TypesOfShiftBits<> {
  using type = std::tuple<>;
};

template <typename T, typename...Ts>
struct TypesOfShiftBits<T, Ts...> {
  using type = tuple_concat_t<
    std::tuple<typename T::shift_type>,
    typename TypesOfShiftBits<Ts...>::type>;
};

template <typename w_UnsignedType, typename w_BitsType>
struct BitTypeAppliers {
  using unsigned_type = w_UnsignedType;
  using BitType = w_BitsType;

  using applier = typename BitType
    ::template ShiftMaskInfo<unsigned_type>::group
    ::template apply<ApplyMaskShift>;

  using inv_applier = typename BitType
    ::template ShiftMaskInfo<unsigned_type>::inverse_group
    ::template apply<ApplyMaskShift>;
};

template <typename w_UnsignedType,
          typename w_FormatType,
          typename...w_BitsTypes>
struct BitTypesTraitsHelper;

template <typename w_UnsignedType, typename w_FormatType>
struct BitTypesTraitsHelper<w_UnsignedType, w_FormatType> {
  using FormatType = w_FormatType;
  using unsigned_type = w_UnsignedType;

  static constexpr bool all_contained = true;

  static constexpr unsigned_type in_mask = { 0 };

  static constexpr unsigned_type collision_mask{ 0 };
};

template <typename w_UnsignedType,
          typename w_FormatType,
          typename w_BitsType,
          typename...w_BitsTypes>
struct BitTypesTraitsHelper<
    w_UnsignedType, w_FormatType, w_BitsType, w_BitsTypes...> {
  using BitType = w_BitsType;
  using FormatType = w_FormatType;
  using unsigned_type = w_UnsignedType;
  using appliers = BitTypeAppliers<unsigned_type, BitType>;
  static constexpr bool contained = w_FormatType::template contains<w_BitsType>;

  /// Are all w_BitsTypes are specified in FormatType.
  static constexpr bool all_contained =
    contained
    && BitTypesTraitsHelper<unsigned_type, FormatType, w_BitsTypes...>::all_contained;

  // This only contribututes to in_mask if it is contained.
  static constexpr auto this_in_mask =
      (contained ? appliers::applier::in_mask : unsigned_type{ 0 });

  /// The mask of all the incoming bittypes.
  static constexpr unsigned_type in_mask =
    this_in_mask
    | BitTypesTraitsHelper<unsigned_type, FormatType, w_BitsTypes...>::in_mask;

  /// If non zero there are some w_BitsTypes writing to the same bits.
  static constexpr unsigned_type collision_mask = (this_in_mask
    & BitTypesTraitsHelper<unsigned_type, FormatType, w_BitsTypes...>::in_mask)
    | BitTypesTraitsHelper<unsigned_type, FormatType, w_BitsTypes...>::collision_mask;
};

/**
 * Provides masks and converters for the format/bittypes combination.
 */
template <typename w_FormatType, typename...w_BitsTypes>
struct BitTypesTraits {
  using bits_types = typename TypesOfBits<w_BitsTypes...>::type;
  using all_types = tuple_concat_t<std::tuple<typename w_FormatType::type>, bits_types>;
  using unsigned_type = typename UnsignedType<all_types>::type;

  using traits = BitTypesTraitsHelper<unsigned_type, w_FormatType, w_BitsTypes...>;
};


template <typename w_UnsignedType, typename w_FormatType, typename...w_BitsTypes>
struct BitTypesEvaluatorHelper;

template <typename w_UnsignedType, typename w_FormatType>
struct BitTypesEvaluatorHelper<w_UnsignedType, w_FormatType> {
  using unsigned_type = w_UnsignedType;

  static constexpr unsigned_type evaluate() {
    return unsigned_type{ 0 };
  }
};

template <typename w_UnsignedType, typename w_FormatType, typename w_BitsType, typename...w_BitsTypes>
struct BitTypesEvaluatorHelper<w_UnsignedType, w_FormatType, w_BitsType, w_BitsTypes...> {

  using traits = typename BitTypesTraits<w_FormatType, w_BitsType, w_BitsTypes...>::traits;
  using unsigned_type = typename traits::unsigned_type;
  static constexpr bool contained = w_FormatType::template contains<w_BitsType>;

  static constexpr unsigned_type evaluate(w_BitsType bitValue, w_BitsTypes...bitsValues) {
    return (contained
      ? traits::appliers::inv_applier::convert(static_cast<unsigned_type>(bitValue.value))
      : unsigned_type{0})
      | BitTypesEvaluatorHelper<w_UnsignedType, w_FormatType, w_BitsTypes...>::evaluate(bitsValues...);
  }
};

template <bool allow_unreferenced,
          typename w_FormatType,
          typename...w_BitsTypes>
struct BitTypesEvaluator {

  using bits_traits = BitTypesTraits<w_FormatType, w_BitsTypes...>;
  using traits = typename bits_traits::traits;
  using unsigned_type = typename traits::unsigned_type;
  using helper = BitTypesEvaluatorHelper<unsigned_type, w_FormatType, w_BitsTypes...>;

  static_assert(allow_unreferenced || traits::all_contained,
    "The target value does not contain all the bit values provided.");

  static_assert(!traits::collision_mask,
    "The provided bit values have colliding bit positions in the target.");

  static constexpr unsigned_type evaluate(w_BitsTypes...bitsValues) {
    return helper::evaluate(bitsValues...);
  }
};

// Specialization allowing w_BitsTypes to be a tuple.
template <bool allow_unreferenced,
  typename w_FormatType,
  typename...w_BitsTypes>
  struct BitTypesEvaluator<allow_unreferenced, w_FormatType, std::tuple<w_BitsTypes...>>
      : BitTypesEvaluator<allow_unreferenced, w_FormatType, w_BitsTypes...> {
};

/**
 * Assigns multiple bitfields from a single value.
 */
template <typename...w_Proxies>
struct Assigner;

template <typename w_Proxy>
struct Assigner<w_Proxy> {
  using ProxyType = w_Proxy;

  Assigner(const Assigner& rhs) = default;
  Assigner& operator=(const Assigner& rhs) = delete;

  Assigner(w_Proxy& proxy)
    : proxy{&proxy}
  {}

  template <typename w_FormatType, typename T>
  void Assign(const T& value) const {
    static_assert(w_FormatType::template contains<ProxyType>,
      "Cannot assign value not containing result type");
    using appliers = BitTypeAppliers<T, ProxyType>;
    proxy->value = static_cast<typename ProxyType::type>(appliers::applier::convert(value));
  }

  template <typename w_FormatType>
  const Assigner& operator=(const BitValue<w_FormatType>& value) const {
    static_assert(w_FormatType::template contains<ProxyType>,
      "Cannot assign value not containing result type");
    using FormatTypeType = typename w_FormatType::type;
    using TypesOfBitsType = typename TypesOfBits<w_Proxy>::type;
    using unsigned_type = typename UnsignedType<
      tuple_concat_t<std::tuple<FormatTypeType>, TypesOfBitsType>>::type;
    Assign<w_FormatType>(static_cast<unsigned_type>(value.value));
    return *this;
  }

  /**
   * AssignSparse will assign only the fields associated with the format/register
   * provided.
   */
  template <typename w_FormatType, typename w_unsigned_type>
  void AssignSparse(const w_unsigned_type& value) const {
    if (w_FormatType::template contains<ProxyType>) {
      using appliers = BitTypeAppliers<w_unsigned_type, ProxyType>;
      proxy->value = static_cast<typename ProxyType::type>(appliers::applier::convert(value));
    }
  }

  template <typename w_FormatType>
  const Assigner& AssignSparse(const BitValue<w_FormatType>& value) const {
    using FormatTypeType = typename w_FormatType::type;
    using TypesOfBitsType = typename TypesOfBits<ProxyType>::type;

    using unsigned_type = typename UnsignedType<
      tuple_concat_t<std::tuple<FormatTypeType>, TypesOfBitsType>>::type;
    AssignSparse<w_FormatType, unsigned_type>(static_cast<unsigned_type>(value.value));
    return *this;
  }

  ProxyType* const proxy;
};

template <typename w_Proxy, typename...w_Proxies>
struct Assigner<w_Proxy, w_Proxies...> : Assigner<w_Proxies...> {
  using ProxyType = w_Proxy;

  Assigner(const Assigner& rhs) = default;
  Assigner& operator=(const Assigner& rhs) = delete;

  Assigner(w_Proxy& proxy, w_Proxies&... proxies)
    : Assigner<w_Proxies...>{proxies...}, proxy{&proxy}
  {
  }

  template <typename w_FormatType, typename T>
  void Assign(const T& value) const {
    static_assert(w_FormatType::template contains<ProxyType>,
      "Cannot assign value not containing result type");
    const Assigner<w_Proxies...>& super{ *this };
    super.template Assign<w_FormatType, T>(value);

    using appliers = BitTypeAppliers<T, ProxyType>;
    proxy->value = static_cast<typename ProxyType::type>(appliers::applier::convert(value));
  }

  template <typename w_FormatType>
  const Assigner& operator=(const BitValue<w_FormatType>& value) const {
    static_assert(w_FormatType::template contains<ProxyType>,
      "Cannot assign value not containing result type");

    using FormatTypeType = typename w_FormatType::type;
    using TypesOfBitsType = typename TypesOfBits<w_Proxy, w_Proxies...>::type;
    using unsigned_type = typename UnsignedType<
      tuple_concat_t<std::tuple<FormatTypeType>, TypesOfBitsType>>::type;
    Assign<w_FormatType, unsigned_type>(static_cast<unsigned_type>(value.value));
    return *this;
  }

  /**
   * AssignSparse will assign only the fields associated with the format/register
   * provided.
   */
  template <typename w_FormatType, typename T>
  void AssignSparse(const T& value) const {
    const Assigner<w_Proxies...>& super{ *this };
    super.template AssignSparse<w_FormatType, T>(value);
    if (w_FormatType::template contains<ProxyType>) {
      using appliers = BitTypeAppliers<T, ProxyType>;
      proxy->value = static_cast<typename ProxyType::type>(appliers::applier::convert(value));
    }
  }

  template <typename w_FormatType>
  const Assigner& AssignSparse(const BitValue<w_FormatType>& value) const {
    using FormatTypeType = typename w_FormatType::type;
    using TypesOfBitsType = typename TypesOfBits<w_Proxy, w_Proxies...>::type;
    using TypesOfShiftBitsType = typename TypesOfShiftBits<w_Proxy, w_Proxies...>::type;

    // The type used to shift bits around needs to be at least the size of either the largest
    // destination type or the largest shifted bit, hence the selection of an unsigned type
    // at least as large as all thise types including the type of the format value.
    using unsigned_type = typename UnsignedType<
      tuple_concat_t<std::tuple<FormatTypeType>, TypesOfBitsType, TypesOfShiftBitsType>>::type;
    AssignSparse<w_FormatType, unsigned_type>(static_cast<unsigned_type>(value.value));
    return *this;
  }

  ProxyType* const proxy;
};

template <typename...w_Proxies>
Assigner<w_Proxies...> Assign(w_Proxies&... proxies) {
  return Assigner<w_Proxies...>(proxies...);
}

template <typename T, typename...w_BitTypes>
struct Format {
  using type = T;
  using bit_types = std::tuple<w_BitTypes...>;

  template <typename w_BitType>
  static constexpr bool contains = has_type<w_BitType, bit_types>::value;

  static_assert(((w_BitTypes::max_bits < sizeof(T) * 8) && ...),
    "GREVIR_REGISTER_FIELD_OUT_OF_RANGE");
};

template <typename...w_BitTypes>
struct BitFields {
  using bit_types = std::tuple<w_BitTypes...>;

  template <typename w_BitType>
  static constexpr bool contains = has_type<w_BitType, bit_types>::value;

  template <typename T>
  using FormatOf = Format<T, w_BitTypes...>;
};


} // namespace setl
