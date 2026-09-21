#pragma once

#include <GrevirRegisters.h>
#include <grevir/test/register_memory.hpp>

namespace register_mock {
using grevir::test::Kind;
using grevir::test::Event;
using Memory = grevir::test::RegisterMemory<32>;
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Memory>;
template <typename T, std::ptrdiff_t Address>
struct Definition {
  using type = T;
  static constexpr std::ptrdiff_t addr = Address;
};
struct Fixture {
  Fixture() { Memory::reset(); }
};
} // namespace register_mock
