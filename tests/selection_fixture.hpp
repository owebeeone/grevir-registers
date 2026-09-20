#pragma once

#include "memory_access.hpp"

namespace selection_mock {
using namespace register_mock;
using Low = setl::BitsRW<setl::SemanticType<10, std::uint8_t>, 2, 1, 0>;
using High = setl::BitsRW<setl::SemanticType<11, std::uint8_t>, 7, 6, 5>;
using Wide = setl::BitsRW<setl::SemanticType<12, bool>, 12>;
using Whole = setl::BitsRW<setl::SemanticType<13, std::uint8_t>>;
using Unused = setl::BitsRW<setl::SemanticType<14, bool>, 0>;
using ByteReg = setl::IoRegister<setl::BitFields<Low, High>, Definition<std::uint8_t, 1>, Access>;
using WordReg = setl::IoRegister<setl::BitFields<Wide>, Definition<std::uint16_t, 3>, Access>;
using WholeReg = setl::IoRegister<setl::BitFields<Whole>, Definition<std::uint8_t, 6>, Access>;
using UnusedReg = setl::IoRegister<setl::BitFields<Unused>, Definition<std::uint8_t, 8>, Access>;
using Registers = std::tuple<UnusedReg, ByteReg, WordReg, WholeReg>;
using Selection = setl::RegisterSelector<Registers>;
struct Runner : setl::ApplierRunner {
  using setl::ApplierRunner::applyNoSync;
  using setl::ApplierRunner::applySync;
};
} // namespace selection_mock
