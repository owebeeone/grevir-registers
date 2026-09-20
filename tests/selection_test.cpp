#include "selection_fixture.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {
using namespace selection_mock;
static_assert(setl::FindRegisterForField<Unused, Registers>::value);
static_assert(std::is_same_v<setl::FindRegisterForField<Unused, Registers>::type, UnusedReg>);
static_assert(std::is_same_v<setl::FindRegisterForField<Wide, Registers>::type, WordReg>);
static_assert(!setl::FindRegisterForField<Low, std::tuple<UnusedReg>>::value);
static_assert(std::is_same_v<setl::FindRegisterForField<Low, std::tuple<UnusedReg>>::type, void>);
static_assert(!setl::FindRegisterForField<Low, std::tuple<>>::value);
}

TEST_CASE_METHOD(Fixture, "selection writes participating registers in tuple order", "[registers]") {
  Memory::bytes[1] = 0xff;
  Selection::ReadModifyWrite(Whole{0x45}, Wide{true}, Low{2}, High{3});
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Read, 1, 1, 0xff}, {Kind::Write, 1, 1, 0x7a},
    {Kind::Read, 3, 2, 0}, {Kind::Write, 3, 2, 0x1000},
    {Kind::Write, 6, 1, 0x45}});
}

TEST_CASE_METHOD(Fixture, "selection reads each participating register once", "[registers]") {
  Memory::bytes[1] = 0xa6;
  Access<std::uint16_t, 3>::set(0x1000);
  Memory::events.clear();
  Low low;
  High high;
  Wide wide;
  Selection::Read(wide, high, low);
  REQUIRE(low.value == 6);
  REQUIRE(high.value == 5);
  REQUIRE(wide.value);
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Read, 1, 1, 0xa6}, {Kind::Read, 3, 2, 0x1000}});
  Memory::events.clear();
  REQUIRE(Selection::Read<High>() == 5);
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Read, 1, 1, 0xa6}});
}

TEST_CASE_METHOD(Fixture, "selection retains broadcast and last matching read behavior", "[registers]") {
  using Mirror = setl::IoRegister<setl::BitFields<Low>, Definition<std::uint8_t, 10>, Access>;
  using Broadcast = setl::RegisterSelector<std::tuple<ByteReg, Mirror>>;
  Broadcast::ReadModifyWrite(Low{3});
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Read, 1, 1, 0}, {Kind::Write, 1, 1, 3},
    {Kind::Read, 10, 1, 0}, {Kind::Write, 10, 1, 3}});
  Memory::bytes[10] = 5;
  Memory::events.clear();
  REQUIRE(Broadcast::Read<Low>() == 5);
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Read, 1, 1, 3}, {Kind::Read, 10, 1, 5}});
}

TEST_CASE_METHOD(Fixture, "selection with no fields performs no access", "[registers]") {
  Selection::ReadModifyWrite();
  Selection::Read();
  setl::RegisterSelector<std::tuple<>>::ReadModifyWrite();
  setl::RegisterSelector<std::tuple<>>::Read();
  REQUIRE(Memory::events.empty());
}
