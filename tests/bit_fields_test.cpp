#include "legacy_fields_fixture.hpp"
#include <catch2/catch_test_macros.hpp>

namespace { using namespace legacy_fields; }

TEST_CASE_METHOD(Fixture, "legacy testRegSelector asserts the printed before and after values", "[registers]") {
  RegSelectorTCCR1::ReadModifyWrite(BitsCOM1A{EnumCOMn::disconnect}, BitsICNC1{false});
  REQUIRE(BitsCOM1A{RegisterTCCR1A::Read()}.value == EnumCOMn::disconnect);
  REQUIRE_FALSE(BitsICNC1{RegisterTCCR1B::Read()}.value);
  RegSelectorTCCR1::ReadModifyWrite(BitsCOM1A{EnumCOMn::set}, BitsICNC1{true});
  BitsCOM1A a;
  BitsICNC1 b;
  RegSelectorTCCR1::Read(a, b);
  REQUIRE(a.value == EnumCOMn::set);
  REQUIRE(b.value);
  REQUIRE(BitsCOM1A{RegisterTCCR1A::Read()}.value == a.value);
  REQUIRE(BitsICNC1{RegisterTCCR1B::Read()}.value == b.value);
}

TEST_CASE_METHOD(Fixture, "legacy testRegSelector2 asserts grouped constants and preserved fields", "[registers]") {
  setl::ApplierValues<>::apply<RegSelectorTCCR1>();
  REQUIRE(Memory::events.empty());
  using Initial = setl::ApplierValues<
    setl::ApplierValue<BitsCOM1A, EnumCOMn::disconnect>,
    setl::ApplierValue<BitsCOM1B, EnumCOMn::disconnect>,
    setl::ApplierValue<BitsICNC1, false>>;
  Initial::apply<RegSelectorTCCR1>();
  BitsCOM1A a;
  BitsCOM1B b;
  BitsICNC1 c;
  RegSelectorTCCR1::Read(a, b, c);
  REQUIRE(a.value == EnumCOMn::disconnect);
  REQUIRE(b.value == EnumCOMn::disconnect);
  REQUIRE_FALSE(c.value);
  using Next = setl::ApplierValues<setl::ApplierValue<BitsCOM1A, EnumCOMn::set>,
    setl::ApplierValue<BitsCOM1B, EnumCOMn::toggle>, setl::ApplierValue<BitsICNC1, true>>;
  Next::apply<RegSelectorTCCR1>();
  RegSelectorTCCR1::Read(a, b, c);
  REQUIRE(a.value == EnumCOMn::set);
  REQUIRE(b.value == EnumCOMn::toggle);
  REQUIRE(c.value);
  setl::ApplierValues<setl::ApplierValue<BitsCOM1A, EnumCOMn::clear>>::apply<RegSelectorTCCR1>();
  RegSelectorTCCR1::Read(a, b, c);
  REQUIRE(a.value == EnumCOMn::clear);
  REQUIRE(b.value == EnumCOMn::toggle);
  REQUIRE(c.value);
}

TEST_CASE_METHOD(Fixture, "legacy getTypeWGM1 asserts snapshot assignment", "[registers]") {
  BitsCOM1A a;
  BitsCOM1B b;
  BitsWGM1_10 mode;
  setl::Assign(mode, a, b) = RegisterTCCR1A::Read();
  REQUIRE(a.value == EnumCOMn::disconnect);
  REQUIRE(b.value == EnumCOMn::disconnect);
  REQUIRE(mode.value == TypeWGM1::normal);
  RegisterTCCR1A::ioregister::set(0xff);
  setl::Assign(mode, a, b) = RegisterTCCR1A::Read();
  REQUIRE(a.value == EnumCOMn::set);
  REQUIRE(b.value == EnumCOMn::set);
  REQUIRE(mode.value == TypeWGM1::pwm_phase_correct_10bit);
  setl::Assign(a, b) = RegisterTCCR1A::Read(0xff);
  REQUIRE(a.value == EnumCOMn::set);
  REQUIRE(b.value == EnumCOMn::set);
  a = RegisterTCCR1A::Read(0x7f);
  REQUIRE(a.value == EnumCOMn::toggle);
}

TEST_CASE_METHOD(Fixture, "legacy rwTypeWGM1 asserts default and partial writes", "[registers]") {
  auto defaults = RegisterTCCR1A::Read(0xff);
  BitsCOM1A a;
  BitsCOM1B b;
  BitsWGM1_10 mode;
  setl::Assign(a, b) = defaults;
  a = defaults;
  RegisterTCCR1A::Write(defaults, a, BitsCOM1B{EnumCOMn::clear});
  REQUIRE(Memory::bytes[0x80] == 0xef);
  setl::Assign(mode, a, b) = RegisterTCCR1A::Read();
  REQUIRE(a.value == EnumCOMn::set);
  REQUIRE(b.value == EnumCOMn::clear);
  REQUIRE(mode.value == TypeWGM1::pwm_phase_correct_10bit);
  RegisterTCCR1A::ReadModifyWrite(BitsCOM1A{EnumCOMn::toggle},
    BitsCOM1B{EnumCOMn::disconnect}, BitsWGM1_10{TypeWGM1::pwm_phase_correct_9bit});
  REQUIRE(Memory::bytes[0x80] == 0x4e);
  setl::Assign(mode, a, b) = RegisterTCCR1A::Read();
  REQUIRE(a.value == EnumCOMn::toggle);
  REQUIRE(b.value == EnumCOMn::disconnect);
  REQUIRE(mode.value == TypeWGM1::pwm_phase_correct_9bit);
}

TEST_CASE_METHOD(Fixture, "legacy split enum views preserve logical positions across registers", "[registers]") {
  for (unsigned value = 0; value < 16; ++value) {
    Memory::reset(0xff);
    const auto mode = static_cast<TypeWGM1>(value);
    RegSelectorTCCR1::ReadModifyWrite(BitsWGM1_10{mode}, BitsWGM1_32{mode});
    REQUIRE(Memory::bytes[0x80] == (0xfc | (value & 3)));
    REQUIRE(Memory::bytes[0x81] == (0xe7 | ((value & 12) << 1)));
    BitsWGM1_10 low;
    BitsWGM1_32 high;
    RegSelectorTCCR1::Read(low, high);
    REQUIRE((static_cast<unsigned>(low.value) | static_cast<unsigned>(high.value)) == value);
  }
}
