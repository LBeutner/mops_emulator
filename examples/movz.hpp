#pragma once
#include <cstdint>
#include <optional>

namespace movz {

inline constexpr std::uint32_t kMovzMask = 0b0111'1111'1000'0000'0000'0000'0000'0000;
inline constexpr std::uint32_t kMovzValue = 0b0101'0010'1000'0000'0000'0000'0000'0000;

struct Decoded {
  std::uint8_t reg;
  std::uint16_t imm;
  std::uint8_t shift;
};

constexpr std::optional<Decoded> decode(std::uint32_t word) {
  // Not Movz or 32-bit variant
  if ((word & kMovzMask) != kMovzValue || ((word >> 31) & 0b1) == 0) {
    return std::nullopt;
  }

  Decoded decoded{.reg = static_cast<std::uint8_t>(word & 0b11111),
                  .imm = static_cast<std::uint16_t>((word >> 5) & 0xFFFF),
                  .shift = static_cast<std::uint8_t>((word >> 21) & 0b11)};

  return decoded;
}

} // namespace movz