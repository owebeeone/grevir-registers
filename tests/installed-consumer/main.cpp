#include <GrevirRegisters.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace {
struct Memory {
  static inline std::array<std::uint8_t, 8> bytes{};
  static inline unsigned reads = 0;
  static inline unsigned writes = 0;
  template <typename T>
  static T read(std::ptrdiff_t address) {
    ++reads;
    T value;
    std::memcpy(&value, bytes.data() + address, sizeof(T));
    return value;
  }
  template <typename T>
  static void write(std::ptrdiff_t address, T value) {
    ++writes;
    std::memcpy(bytes.data() + address, &value, sizeof(T));
  }
  template <typename T>
  static void modify(std::ptrdiff_t address, T value, T mask) {
    write<T>(address, static_cast<T>((read<T>(address) & ~mask) | value));
  }
};
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Memory>;
template <typename T, std::ptrdiff_t Address>
struct Definition {
  using type = T;
  static constexpr std::ptrdiff_t addr = Address;
};
using Sparse = setl::BitsRW<std::uint8_t, 5, 2, 0>;
using Byte = setl::BitsRW<std::uint8_t>;
using Word = setl::BitsRW<std::uint16_t>;
using SparseReg = setl::IoRegister<setl::BitFields<Sparse>, Definition<std::uint8_t, 0>, Access>;
using ByteReg = setl::IoRegister<setl::BitFields<Byte>, Definition<std::uint8_t, 0>, Access>;
using WordReg = setl::IoRegister<setl::BitFields<Word>, Definition<std::uint16_t, 1>, Access>;
using Selection = setl::RegisterSelector<std::tuple<SparseReg, WordReg>>;
struct Barrier {
  static inline unsigned enters = 0;
  static inline unsigned exits = 0;
  Barrier() { ++enters; }
  ~Barrier() { ++exits; }
};
struct Runner : setl::ApplierRunner {
  using setl::ApplierRunner::applySync;
};
}

int main() {
  Memory::bytes.fill(0xaa);
  SparseReg::ReadModifyWrite(Sparse{5});
  if (Memory::bytes[0] != 0xab || Memory::reads != 1 || Memory::writes != 1) {
    return 1;
  }
  ByteReg::ReadModifyWrite(Byte{0x55});
  if (Memory::bytes[0] != 0x55 || Memory::reads != 1 || Memory::writes != 2) {
    return 2;
  }
  WordReg::Write(Word{0x1234});
  if (WordReg::Read().value != 0x1234 || Memory::reads != 2 || Memory::writes != 3
      || Memory::bytes[0] != 0x55 || Memory::bytes[3] != 0xaa) {
    return 3;
  }
  Selection::ReadModifyWrite(Sparse{2}, Word{0x4567});
  if (Memory::reads != 3 || Memory::writes != 5 || Memory::bytes[0] != 0x54) {
    return 4;
  }
  Sparse sparse;
  Word word;
  Selection::Read(sparse, word);
  if (sparse.value != 2 || word.value != 0x4567 || Memory::reads != 5) {
    return 5;
  }
  using Values = setl::ApplierValues<setl::ApplierValue<Sparse, 5>,
    setl::ApplierValue<Word, 0x789a>>;
  Values::apply<Selection>();
  if (Memory::reads != 6 || Memory::writes != 7 || Memory::bytes[0] != 0x71) {
    return 6;
  }
  using Ops = setl::Appliers<setl::Applier<Sparse, 0, std::tuple<WordReg, SparseReg>>>;
  Runner::applySync<Ops, Barrier>();
  if (Memory::bytes[0] != 0x50 || WordReg::Read().value != 0x789a
      || Barrier::enters != 1 || Barrier::exits != 1) {
    return 7;
  }
  using Full32 = setl::BitsRW<std::uint32_t>;
  using Full32Reg = setl::IoRegister<setl::BitFields<Full32>,Definition<std::uint32_t,3>,Access>;
  const auto reads_before = Memory::reads;
  Full32Reg::ReadModifyWrite(Full32{0x80000001u});
  if (Memory::reads != reads_before || Full32Reg::Read().value != 0x80000001u) { return 8; }
  return 0;
}
