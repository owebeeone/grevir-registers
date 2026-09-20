#include "selection_fixture.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {
using namespace selection_mock;
using Values = setl::ApplierValues<setl::ApplierValue<Low, 2>,
  setl::ApplierValue<High, 3>, setl::ApplierValue<Wide, true>,
  setl::ApplierValue<Whole, 0x45>>;
struct Barrier {
  Barrier() { Memory::events.push_back({Kind::Read, -1, 0, 0}); }
  ~Barrier() { Memory::events.push_back({Kind::Write, -1, 0, 0}); }
};
}

TEST_CASE_METHOD(Fixture, "constant applier groups fields and preserves reverse register order", "[registers]") {
  Memory::bytes[1] = 0xff;
  Values::apply<Selection>();
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Write, 6, 1, 0x45},
    {Kind::Read, 3, 2, 0}, {Kind::Write, 3, 2, 0x1000},
    {Kind::Read, 1, 1, 0xff}, {Kind::Write, 1, 1, 0x7a}});
}

TEST_CASE_METHOD(Fixture, "explicit applier sequence keeps caller order and finds later registers", "[registers]") {
  using Ops = setl::Appliers<setl::Applier<Whole, 0x55, Registers>,
    setl::Applier<Low, 3, ByteReg>, setl::Applier<Wide, true, Registers>>;
  Runner::applyNoSync<Ops>();
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Write, 6, 1, 0x55},
    {Kind::Read, 1, 1, 0}, {Kind::Write, 1, 1, 3},
    {Kind::Read, 3, 2, 0}, {Kind::Write, 3, 2, 0x1000}});
}

TEST_CASE_METHOD(Fixture, "synchronized applier uses the caller supplied scope barrier", "[registers]") {
  using Ops = setl::Appliers<setl::Applier<Whole, 0x55, WholeReg>>;
  Runner::applySync<Ops, Barrier>();
  REQUIRE(Memory::events == std::vector<Event>{
    {Kind::Read, -1, 0, 0}, {Kind::Write, 6, 1, 0x55}, {Kind::Write, -1, 0, 0}});
}

TEST_CASE_METHOD(Fixture, "empty appliers perform no access", "[registers]") {
  setl::ApplierValues<>::apply<Selection>();
  setl::ApplierValues<>::apply<setl::RegisterSelector<std::tuple<>>>();
  Runner::applyNoSync<setl::Appliers<>>();
  REQUIRE(Memory::events.empty());
}

TEST_CASE_METHOD(Fixture, "applier reader reads typed field and void reader does nothing", "[registers]") {
  Memory::bytes[1] = 6;
  REQUIRE(setl::ApplierReader<Low>::read<Selection>().value == 6);
  setl::ApplierReader<void>::read<Selection>();
  REQUIRE(Memory::events == std::vector<Event>{{Kind::Read, 1, 1, 6}});
}
