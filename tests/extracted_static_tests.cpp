#include <GrevirRegisters.h>

namespace setl {
using namespace nfp;
static_assert(Max<1, 2>::value == 2, "Max calculation is wrong.");

static_assert(Max<2, 1>::value == 2, "Max calculation is wrong.");

static_assert(Max<1, 1>::value == 1, "Max calculation is wrong.");

static_assert(
  std::is_same_v<
    DeriveBitsFor_t<UintList<4, 3>, 7, 7>,
    UintList<4, 3>>,
  "DeriveBitsFor is broken"
);

static_assert(
  std::is_same_v<
    DeriveBitsFor_t<UintList<>, 7, 7>,
    UintList<7, 6, 5, 4, 3, 2, 1, 0>>,
  "DeriveBitsFor is broken"
);

static_assert(
  std::is_same_v<
    MakeBitsFor<UintList<3, 2, 1>>::type<BitOps::ReadOnly, int>,
    BitsImpl<BitOps::ReadOnly, int, 3, 2, 1>>,
  "MakeBitsFor is broken"
);

static_assert(
  std::is_same_v<
    Bits<BitOps::ReadOnly, char, 3, 2, 1>,
    BitsImpl<BitOps::ReadOnly, char, 3, 2, 1>>,
  "Bits is broken"
);

static_assert(
  std::is_same_v<
    Bits<BitOps::ReadOnly, char>,
    BitsImpl<BitOps::ReadOnly, char, 7, 6, 5, 4, 3, 2, 1, 0>>,
  "Bits is broken"
);

}
