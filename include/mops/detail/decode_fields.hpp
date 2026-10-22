#pragma once
#include <cstdint>

namespace mops::detail {

inline constexpr std::uint32_t kMopsMask = 0b0011'1011'0010'0000'0000'1100'0000'0000;
inline constexpr std::uint32_t kMopsValue = 0b0001'1001'0000'0000'0000'0100'0000'0000;

static_assert((~kMopsMask & kMopsValue) == 0, "value has bits outside the mask set");

struct RawFields {
  std::uint8_t size, o0, op1, op2, rs, rn, rd;
};

// instruction is mops, not necessarily a correct one
constexpr bool is_mops(std::uint32_t word) {
  return (word & kMopsMask) == kMopsValue;
}

constexpr RawFields extract_fields(std::uint32_t word) {
  return RawFields{
      .size = static_cast<std::uint8_t>((word >> 30) & 0b11),
      .o0 = static_cast<std::uint8_t>((word >> 26) & 0b1),
      .op1 = static_cast<std::uint8_t>((word >> 22) & 0b11),
      .op2 = static_cast<std::uint8_t>((word >> 12) & 0b1111),
      .rs = static_cast<std::uint8_t>((word >> 16) & 0b11111),
      .rn = static_cast<std::uint8_t>((word >> 5) & 0b11111),
      .rd = static_cast<std::uint8_t>(word & 0b11111),
  };
}

constexpr bool is_set_family(const RawFields& fields) {
  return fields.op1 == 0b11;
}

} // namespace mops::detail