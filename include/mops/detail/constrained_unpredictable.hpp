#pragma once
#include <cstdint>

namespace mops::detail {

constexpr bool set_constrained_unpredictable(std::uint8_t rd, std::uint8_t rs, std::uint8_t rn) {
  return (rs == rn) || (rs == rd) || (rn == rd) || (rd == 31) || (rn == 31);
}

constexpr bool cpy_constrained_unpredictable(std::uint8_t rd, std::uint8_t rs, std::uint8_t rn) {
  return (rs == rn) || (rs == rd) || (rn == rd) || (rd == 31) || (rs == 31) || (rn == 31);
}

} // namespace mops::detail