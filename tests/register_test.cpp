#include "memory_access.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {
using namespace register_mock;
using Sparse = setl::BitsRW<std::uint8_t, 5, 2, 0>;
using Flag = setl::BitsRW<setl::SemanticType<1, bool>, 7>;
using Reg = setl::IoRegister<setl::BitFields<Sparse, Flag>, Definition<std::uint8_t, 3>, Access>;
using Full = setl::BitsRW<std::uint8_t>;
using FullReg = setl::IoRegister<setl::BitFields<Full>, Definition<std::uint8_t, 4>, Access>;
}

TEST_CASE("sparse mappings encode decode and mask every eight bit input", "[registers]") {
  using Forward = setl::GroupMaskShifts<std::uint8_t, 5, 2, 0>::group::apply<setl::ApplyMaskShift>;
  using Inverse = setl::GroupMaskShifts<std::uint8_t, 5, 2, 0>::inverse_group::apply<setl::ApplyMaskShift>;
  REQUIRE(Forward::in_mask == 0x25);
  REQUIRE(Inverse::out_mask == 0x25);
  for (unsigned value = 0; value < 256; ++value) {
    auto encoded = Inverse::convert(static_cast<std::uint8_t>(value));
    REQUIRE(encoded == (((value & 4) << 3) | ((value & 2) << 1) | (value & 1)));
    REQUIRE(Forward::convert(encoded) == (value & 7));
  }
}

TEST_CASE("mapping gaps retain logical bit positions", "[registers]") {
  using Map = setl::GroupMaskShifts<std::uint16_t, 7, 6, setl::NA, 1, 0>;
  using Encode = Map::inverse_group::apply<setl::ApplyMaskShift>;
  using Decode = Map::group::apply<setl::ApplyMaskShift>;
  for (unsigned value = 0; value < 32; ++value) {
    auto encoded = Encode::convert(value);
    REQUIRE(encoded == (((value & 0x18) << 3) | (value & 3)));
    REQUIRE(Decode::convert(encoded) == (value & 0x1b));
  }
}

TEST_CASE_METHOD(Fixture, "partial register update preserves unrelated bits in one read and write", "[registers]") {
  Memory::bytes[3] = 0xaa;
  Reg::ReadModifyWrite(Sparse{5});
  REQUIRE(Memory::bytes[3] == 0xab);
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Read, 3, 1, 0xaa}, {Kind::Write, 3, 1, 0xab}});
}

TEST_CASE_METHOD(Fixture, "full register update performs no read", "[registers]") {
  Memory::bytes[4] = 0xaa;
  FullReg::ReadModifyWrite(Full{0x55});
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Write, 4, 1, 0x55}});
}

TEST_CASE_METHOD(Fixture, "register read decodes multiple fields from one snapshot", "[registers]") {
  Memory::bytes[3] = 0xab;
  auto snapshot = Reg::Read();
  Sparse sparse;
  Flag flag;
  setl::Assign(sparse, flag) = snapshot;
  REQUIRE(sparse.value == 5);
  REQUIRE(flag.value);
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Read, 3, 1, 0xab}});
}

TEST_CASE_METHOD(Fixture, "default field writes preserve default bits without reading memory", "[registers]") {
  auto default_value = Reg::Read(std::uint8_t{0xff});
  Reg::Write(default_value, Sparse{0});
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Write, 3, 1, 0xda}});
  Memory::events.clear();
  Reg::Write(Sparse{3}, Flag{true});
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Write, 3, 1, 0x85}});
}

TEST_CASE_METHOD(Fixture, "register evaluation changes no memory", "[registers]") {
  const auto defaults = Reg::Read(std::uint8_t{0xff});
  const auto value = Reg::Evaluate(defaults, Sparse{0}, Flag{false});
  REQUIRE(value.value == 0x5a);
  REQUIRE(Memory::events.empty());
}

TEST_CASE_METHOD(Fixture, "typed access honors unaligned byte offsets and widths", "[registers]") {
  using Word = setl::BitsRW<std::uint16_t>;
  using WordReg = setl::IoRegister<setl::BitFields<Word>, Definition<std::uint16_t, 1>, Access>;
  Memory::bytes.fill(0xa5);
  WordReg::Write(Word{0x1234});
  REQUIRE(WordReg::Read().value == 0x1234);
  REQUIRE(Memory::bytes[0] == 0xa5);
  REQUIRE(Memory::bytes[3] == 0xa5);
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Write, 1, 2, 0x1234}, {Kind::Read, 1, 2, 0x1234}});
}

TEST_CASE_METHOD(Fixture, "masked access cannot change bits outside the mask", "[registers]") {
  Memory::bytes[3] = 0xa0;
  Access<std::uint8_t, 3>::set_mask(0xff, 0x0f);
  REQUIRE(Memory::bytes[3] == 0xaf);
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Read, 3, 1, 0xa0}, {Kind::Write, 3, 1, 0xaf}});
}
