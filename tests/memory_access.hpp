#pragma once

#include <GrevirRegisters.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace register_mock {
enum class Kind { Read, Write };
struct Event {
  Kind kind;
  std::ptrdiff_t address;
  std::size_t width;
  std::uint64_t value;
  bool operator==(const Event&) const = default;
};
struct Memory {
  inline static std::array<unsigned char, 32> bytes{};
  inline static std::vector<Event> events;
  template <typename T>
  static void check(std::ptrdiff_t address) {
    static_assert(sizeof(T) <= 32);
    if (address < 0 || std::size_t(address) > bytes.size() - sizeof(T)) {
      throw std::out_of_range("mock register address");
    }
  }
  template <typename T>
  static T read(std::ptrdiff_t address) {
    check<T>(address);
    T value{};
    std::memcpy(&value, bytes.data() + address, sizeof(T));
    events.push_back({Kind::Read, address, sizeof(T), value});
    return value;
  }
  template <typename T>
  static void write(std::ptrdiff_t address, T value) {
    check<T>(address);
    std::memcpy(bytes.data() + address, &value, sizeof(T));
    events.push_back({Kind::Write, address, sizeof(T), value});
  }
  template <typename T>
  static void modify(std::ptrdiff_t address, T value, T mask) {
    const T original = read<T>(address);
    write<T>(address, static_cast<T>((original & ~mask) | value));
  }
};
template <typename T, std::ptrdiff_t Address>
using Access = setl::McuRegister<T, Address, Memory>;
template <typename T, std::ptrdiff_t Address>
struct Definition {
  using type = T;
  static constexpr std::ptrdiff_t addr = Address;
};
struct Fixture {
  Fixture() {
    Memory::bytes.fill(0);
    Memory::events.clear();
  }
};
} // namespace register_mock
