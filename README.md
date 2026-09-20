# Grevir Registers

Portable bit mappings, typed field values, field formats and explicit register
access, extracted from Ardoinus `setl_bit_fields.h`. This first increment retains
the `setl` API names and depends only on Grevir Base. Register selection and
multi-register application operations are now extracted too.

## API and access binding

Include `<GrevirRegisters.h>`, or one of these independent public headers:

| Header | Responsibility |
| --- | --- |
| `grevir/registers/bit_mapping.hpp` | Masks, shifts, sparse and inverse mappings |
| `grevir/registers/bit_values.hpp` | Typed values, semantic identities and `BitsRO/WO/RW` |
| `grevir/registers/fields.hpp` | Field formats, evaluation and assignment |
| `grevir/registers/access.hpp` | Explicit `McuRegister` policy binding and `IoRegister` |
| `grevir/registers/selection.hpp` | Register lookup, selected reads/writes and field readers |
| `grevir/registers/apply.hpp` | Grouped constant values, ordered appliers and explicit barrier scope |

The caller supplies both an address/type definition and an access binding:

```cpp
// Backend supplies read<T>(address), write<T>(address, value), and
// modify<T>(address, value, mask). It owns address interpretation and I/O.
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Backend>;

struct Definition {
  using type = std::uint8_t;
  static constexpr std::ptrdiff_t addr = 3;
};
using Mode = setl::BitsRW<std::uint8_t, 5, 2, 0>;
using Register = setl::IoRegister<setl::BitFields<Mode>, Definition, Access>;
// Inside application code:
// Register::ReadModifyWrite(Mode{5});
```

`modify` preserves bits outside its mask and combines the supplied masked value
inside it. `McuRegister::set_mask` clears input bits outside that mask before
delegation. Partial `ReadModifyWrite` calls `modify`; a full-width update calls
`write` directly. `Write(values...)` clears unspecified bits, while
`Write(defaults, values...)` preserves unspecified bits from the supplied defaults.
`Evaluate` performs no access. `Read()` takes one snapshot, which `Assign` can decode
into several typed fields.

Formats reject fields outside their storage width. Overlapping views may coexist
in a format, but supplying colliding or repeated fields in one write is rejected;
foreign fields are also rejected. The inherited RO/WO tags do not enforce every
hardware access restriction. Backends must provide any required volatile access,
ordering, barriers, atomicity and device-specific behavior such as clear-on-write
flags. No default raw-pointer access or MCU backend is included.

## Selection and multi-register application

`RegisterSelector<std::tuple<...>>` reads or modifies only entries containing the
requested field types. All fields must be supported somewhere in the selection.
Writes combine the fields for each participating register into one operation;
reads take one snapshot per participating tuple entry. Missing fields and
colliding/duplicate fields in one grouped write are compile-time errors. Empty
operations are no-ops, including an empty selection; requesting a field from an
empty selection is an error.

The existing ordering and multiple-match behavior is retained:

| API | Ordering and matching |
| --- | --- |
| `RegisterSelector::ReadModifyWrite` | Forward tuple order; broadcasts to every matching entry |
| `RegisterSelector::Read` | Forward tuple order; the last matching entry supplies a field's final value |
| `FindRegisterForField` / tuple-bound `Applier` | First matching register; a failed finder has `value == false` and `type == void` |
| `ApplierValues<ApplierValue<Field, value>...>::apply<Selector>()` | Groups writes by register; reverse selector tuple order |
| `Appliers<Applier<...>...>` | Explicit operation order, with each operation executed separately |

Register aliases/repeated tuple entries are not deduplicated. An explicit sequence
may intentionally update the same field in separate operations. No operation
implies atomicity across multiple registers.

`ApplierRunner` remains a base class exposing protected helpers. Derived callers
use `applyNoSync<Operations>()`, or `applySync<Operations, MemoryBarrier>()` with a
mandatory caller-supplied RAII barrier type. Its constructor/destructor surround
the operations. The implicit `System::MemoryBarrier` dependency is removed;
platform ordering/atomicity guarantees belong to the chosen policy. This is a
source-level change for synchronized callers. `ApplierReader<Field>` reads through
a selection; its `void` specialization performs no access.

## Extraction corrections and validation

Native compilation exposed a missing dependent `typename` and an invalid narrowing
conversion in `Evaluate`; both are corrected. Register-width promotion previously
caused full 8-bit updates to read unnecessarily, and masked access could modify
bits outside its mask. Both failures were reproduced in the memory fixture before
their fixes. Shift storage now includes the highest bit index (`index + 1` bits),
with assertions for indices 8 and 16; format bounds now have an explicit assertion.

The selection increment reproduced a lookup defect that returned a recursive
helper rather than the matching register (or `void`) after the first tuple entry.
The finder now resolves the recursive result type. Grouped constant writes also
needed a register-width cast to avoid unnecessary reads for full-width writes.

Apple Clang 21 / arm64 macOS / C++23 validation:

- Seven public headers compile independently. Eight assertions relocated from the
  original header and eighteen from the legacy test's pure mapping section compile,
  alongside two new width assertions.
- Eighteen Catch2 cases pass, including all 256 eight-bit mapping inputs, gaps,
  snapshot decoding, partial/full writes, defaults, pure evaluation, masked access
  and unaligned 16-bit memory offsets. Selection and applier cases cover mixed
  widths, skipped registers, snapshot reads, broadcast/last-read behavior, ordering,
  empty operations and the explicit barrier scope. Together: 611 assertions in
  seeded random order.
- Compiler probes accept six operations and reject fifteen cases across direct
  access, selectors and appliers: overlap, duplicate/missing fields, field bounds,
  zero mask and nonempty requests against an empty selection.
- Isolated production and host builds pass with installed dependencies. The
  installed consumer executes sparse/full writes, typed reads, selection, grouped
  constant writes, later-entry lookup and a supplied barrier with Catch2 and Test
  Support discovery disabled.

The byte-array fixture records addresses, widths, values and read/write order;
`memcpy` avoids host alignment assumptions. It models ordinary memory, not MCU
side effects or interrupts. Hardware validation remains on hold. The full legacy
register test mapping and shared `DebugMcuRegister` extraction remain planned;
the local fixture is new, and only the pure mapping test subset was relocated.

## Build and install

With Grevir Base installed under `<prefix>`:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<prefix> \
  -DGREVIR_BUILD_COMPILE_CHECKS=ON
cmake --build build
cmake --install build --prefix <prefix>
```

Consumers use `find_package(grevir-registers CONFIG REQUIRED)` and link
`grevir::registers`. CMake exports C++23 and the Base dependency. Opt-in host tests
use `GREVIR_BUILD_HOST_TESTS=ON` and installed Grevir Test Support/Catch2, or the
workspace's shared setup. Tests and fixtures are not installed as production headers.

Arduino layout and dependency metadata are present, but Arduino/target compiler
compatibility has not been validated. The original Ardoinus sources are unchanged.
