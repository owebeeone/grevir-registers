#include <GrevirRegisters.h>

namespace {
struct Policy {
  template <typename T> static T read(std::ptrdiff_t);
  template <typename T> static void write(std::ptrdiff_t, T);
  template <typename T> static void modify(std::ptrdiff_t, T, T);
};
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Policy>;
template <typename T>
struct Definition { using type = T; static constexpr std::ptrdiff_t addr = 3; };
using Low = setl::BitsRW<std::uint8_t, 2, 1, 0>;
using High = setl::BitsRW<std::uint8_t, 7, 6, 5>;
using Overlap = setl::BitsRW<bool, 2>;
using Outside = setl::BitsRW<bool, 8>;
using Reg = setl::IoRegister<setl::BitFields<Low, High, Overlap>, Definition<std::uint8_t>, Access>;
using WideReg = setl::IoRegister<setl::BitFields<Outside>, Definition<std::uint16_t>, Access>;
template <typename Register, typename... Values>
struct Write {
  static void run() { Register::ReadModifyWrite(Values{}...); }
};
template <typename T>
struct Validate {
  static void run() { static_assert(sizeof(T) > 0); }
};
template <int Id>
struct Case;
template <> struct Case<0> { using Type = Write<Reg, Low, High>; };
template <> struct Case<1> { using Type = Write<Reg, Low, Overlap>; };
template <> struct Case<2> { using Type = Write<Reg, Outside>; };
template <> struct Case<3> { using Type = Validate<setl::Format<std::uint8_t, Outside>>; };
template <> struct Case<4> { using Type = Write<Reg, Low, Low>; };
template <> struct Case<5> { using Type = Validate<setl::MaskShift<std::uint8_t, 0, 0>>; };
template <> struct Case<6> { using Type = Write<WideReg, Outside>; };
using Selection = setl::RegisterSelector<std::tuple<Reg, WideReg>>;
using ByteSelection = setl::RegisterSelector<std::tuple<Reg>>;
using EmptySelection = setl::RegisterSelector<std::tuple<>>;
template <typename Register, typename Value>
struct Read {
  static void run() { (void)Register::template Read<Value>(); }
};
template <typename Selector, typename... Values>
struct Apply {
  static void run() { setl::ApplierValues<Values...>::template apply<Selector>(); }
};
struct Runner : setl::ApplierRunner {
  using setl::ApplierRunner::applyNoSync;
};
struct LaterApplier {
  static void run() {
    Runner::applyNoSync<setl::Appliers<setl::Applier<Outside, true, std::tuple<Reg, WideReg>>>>();
  }
};
struct EmptyOperations {
  static void run() {
    EmptySelection::ReadModifyWrite();
    EmptySelection::Read();
    setl::ApplierValues<>::apply<EmptySelection>();
  }
};
using LowValue = setl::ApplierValue<Low, 3>;
using HighValue = setl::ApplierValue<High, 2>;
using OverlapValue = setl::ApplierValue<Overlap, true>;
using OutsideValue = setl::ApplierValue<Outside, true>;
template <> struct Case<7> { using Type = Write<Selection, Low, High, Outside>; };
template <> struct Case<8> { using Type = Write<ByteSelection, Outside>; };
template <> struct Case<9> { using Type = Read<ByteSelection, Outside>; };
template <> struct Case<10> { using Type = Write<ByteSelection, Low, Overlap>; };
template <> struct Case<11> { using Type = Write<ByteSelection, Low, Low>; };
template <> struct Case<12> { using Type = Apply<Selection, LowValue, HighValue, OutsideValue>; };
template <> struct Case<13> { using Type = Apply<ByteSelection, OutsideValue>; };
template <> struct Case<14> { using Type = Apply<ByteSelection, LowValue, OverlapValue>; };
template <> struct Case<15> { using Type = Apply<ByteSelection, LowValue, LowValue>; };
template <> struct Case<16> { using Type = Validate<setl::Applier<Outside, true, std::tuple<Reg>>>; };
template <> struct Case<17> { using Type = LaterApplier; };
template <> struct Case<18> { using Type = EmptyOperations; };
template <> struct Case<19> { using Type = Write<EmptySelection, Low>; };
template <> struct Case<20> { using Type = Read<EmptySelection, Low>; };
void instantiate() { Case<CASE_ID>::Type::run(); }
}
