#pragma once
#include <GrevirRegisters.h>
#include <grevir/test/register_memory.hpp>

// Portable arrangements adapted from setl_bit_fields_test.cxx. Addresses and
// bit positions are test data, not an exported device definition.
namespace legacy_fields {
enum class TypeWGM1 : unsigned char {
  normal = 0b0000,
  pwm_phase_correct_8bit = 0b0001,
  pwm_phase_correct_9bit = 0b0010,
  pwm_phase_correct_10bit = 0b0011,
  ctc_ocr1a = 0b0100,
  fast_pwm_8bit = 0b0101,
  fast_pwm_9bit = 0b0110,
  fast_pwm_10bit = 0b0111,
  pwm_phase_freq_correct_icr1 = 0b1000,
  pwm_phase_freq_correct_ocr1a = 0b1001,
  pwm_phase_correct_icr1 = 0b1010,
  pwm_phase_correct_ocr1a = 0b1011,
  ctc_icr1 = 0b1100,
  reserved_d = 0b1101,
  fast_pwm_icr1 = 0b1110,
  fast_pwm_ocr1a = 0b1111,
};
enum class TypeCS1 : unsigned char {
  no_clk = 0b000,
  clk1 = 0b001,
  clk8 = 0b010,
  clk64 = 0b011,
  clk256 = 0b100,
  clk1024 = 0b101,
  ext_clk_falling = 0b110,
  ext_clk_rising = 0b111,
};
enum class EnumCOMn : std::uint8_t { disconnect, toggle, clear, set };
using BitsCOM1A = setl::BitsRW<setl::SemanticType<setl::hash("COM1A"), EnumCOMn>, 7, 6>;
using BitsCOM1B = setl::BitsRW<setl::SemanticType<setl::hash("COM1B"), EnumCOMn>, 5, 4>;
using BitsWGM1_10 = setl::BitsRW<TypeWGM1, setl::NA, setl::NA, 1, 0>;
using BitsWGM1_32 = setl::BitsRW<TypeWGM1, 4, 3, setl::NA, setl::NA>;
using BitsCS11 = setl::BitsRW<TypeCS1, 2, 1, 0>;
using BitsICNC1 = setl::BitsRW<bool, 7>;
using BitsICES1 = setl::BitsRW<bool, 6>;
struct Identity;
using Memory = grevir::test::RegisterMemory<256, Identity>;
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Memory>;
template <typename T, std::ptrdiff_t Address>
struct MemRegisterDef { using type = T; static constexpr std::ptrdiff_t addr = Address; };
using FieldsA = setl::BitFields<BitsCOM1A, BitsCOM1B, BitsWGM1_10>;
using FieldsB = setl::BitFields<BitsWGM1_32, BitsCS11, BitsICES1, BitsICNC1>;
using RegisterTCCR1A = setl::IoRegister<FieldsA, MemRegisterDef<std::uint8_t, 0x80>, Access>;
using RegisterTCCR1B = setl::IoRegister<FieldsB, MemRegisterDef<std::uint8_t, 0x81>, Access>;
using RegSelectorTCCR1 = setl::RegisterSelector<std::tuple<RegisterTCCR1B, RegisterTCCR1A>>;
using Traits = setl::BitTypesTraits<RegisterTCCR1A::FormatType, BitsCOM1A, BitsWGM1_10>::traits;
static_assert(Traits::all_contained && Traits::in_mask == 0xc3 && Traits::collision_mask == 0);
static_assert(RegisterTCCR1A::FormatType::contains<BitsCOM1A>);
static_assert(!RegisterTCCR1A::FormatType::contains<BitsWGM1_32>);
struct Fixture { Fixture() { Memory::reset(); } };
} // namespace legacy_fields
