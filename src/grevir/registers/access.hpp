#pragma once

#include <grevir/registers/fields.hpp>
#include <grevir/base/compat/cstddef.hpp>

namespace setl {

/** Address/type binding to an explicit access policy.
 * Access provides read<T>(address), write<T>(address, value) and
 * modify<T>(address, value, mask). The policy owns barriers and atomicity.
 */
template <typename T, std::ptrdiff_t Address, typename Access>
struct McuRegister {
  static constexpr std::ptrdiff_t addrx = Address;
  using nv_type = typename std::remove_cv<T>::type;
  using type = volatile nv_type;

  static void set(nv_type value) {
    Access::template write<nv_type>(addrx, value);
  }
  static void set_mask(nv_type value, nv_type mask) {
    Access::template modify<nv_type>(addrx, static_cast<nv_type>(value & mask), mask);
  }
  static nv_type get() {
    return Access::template read<nv_type>(addrx);
  }
};

/**
 * Defines an MCU register. Provides register read and write functions for
 * the bitfields defined in the bitfields format.
 */
template <typename w_BitFields,
          typename w_Register,
          template <typename T, std::ptrdiff_t u_addrx> typename RegisterType>
struct IoRegister {
  using register_def = w_Register;
  using type = typename register_def::type;
  using FormatType = typename w_BitFields::template FormatOf<type>;
  static constexpr std::ptrdiff_t addrx = register_def::addr;
  using ioregister = RegisterType<type, addrx>;

  static BitValue<FormatType> Read() {
    return Read(ioregister::get());
  }

  static BitValue<FormatType> Read(const type& value) {
    return BitValue<FormatType>{value};
  }

  /**
   * Perform a write operation with the given bit field values by first reading
   * the register (if any bits will survive the write) and then writing the new
   * values in a single write.
   */
  template <bool allow_unreferenced, typename...BitsTypes>
  static void ReadModifyWriteEx(BitsTypes...bitsValues) {
    using Evaluator = BitTypesEvaluator<allow_unreferenced, FormatType, BitsTypes...>;
    auto new_bits = Evaluator::evaluate(bitsValues...);
    auto mask = Evaluator::traits::in_mask;
    if (static_cast<type>(~mask)) {
      ioregister::set_mask(static_cast<type>(new_bits), static_cast<type>(mask));
    } else {
      ioregister::set(static_cast<type>(new_bits));
    }
    return;
  }

  /**
   * Perform a write operation with the given bit field values by first reading
   * the register (if any bits will survive the write) and then writing the new
   * values in a single write.
   */
  template <typename...BitsTypes>
  static void ReadModifyWrite(BitsTypes...bitsValues) {
    ReadModifyWriteEx<false>(bitsValues...);
  }

  /**
   * Perform a write operation with the given bit field values with the provided
   * default value.
   */
  template <typename...BitsTypes>
  static void Write(
      const BitValue<FormatType>& defaultBits, const BitsTypes&...bitsValues) {
    using Evaluator = BitTypesEvaluator<false, FormatType, BitsTypes...>;
    auto new_bits = Evaluator::evaluate(bitsValues...);
    auto mask = Evaluator::traits::in_mask;
    if (static_cast<type>(~mask)) {
      ioregister::set(static_cast<type>(new_bits)
          | (static_cast<type>(~mask) & defaultBits.value));
    } else {
      ioregister::set(static_cast<type>(new_bits));
    }
  }

  /**
   * Perform a write operation with the given bit field values.
   */
  template <typename...BitsTypes>
  static void Write(const BitsTypes&...bitsValues) {
    using Evaluator = BitTypesEvaluator<false, FormatType, BitsTypes...>;
    auto new_bits = Evaluator::evaluate(bitsValues...);
    auto mask = Evaluator::traits::in_mask;
    ioregister::set(static_cast<type>(new_bits));
  }

  /**
   * Evaluates a new value from the given bitfields and default bits.
   */
  template <typename...BitsTypes>
  static BitValue<FormatType> Evaluate(
      const BitValue<FormatType>& defaultBits, BitsTypes...bitsValues) {
    using Evaluator = BitTypesEvaluator<false, FormatType, BitsTypes...>;
    auto new_bits = Evaluator::evaluate(bitsValues...);
    auto mask = Evaluator::traits::in_mask;
    if (static_cast<type>(~mask)) {
      return BitValue<FormatType>{static_cast<type>(new_bits
        | (static_cast<type>(~mask) & defaultBits.value))};
    }
    return BitValue<FormatType>{static_cast<type>(new_bits)};
  }
};



} // namespace setl
