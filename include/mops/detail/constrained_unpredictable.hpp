#pragma once
#include <cstdint>
#include <optional>

namespace mops::detail {

enum class CuKind : std::uint8_t { RegisterOverlap, Register31 };

constexpr std::optional<CuKind> set_constrained_unpredictable(std::uint8_t rd, std::uint8_t rs, std::uint8_t rn) {
  if ((rs == rn) || (rs == rd) || (rn == rd)) {
    return CuKind::RegisterOverlap;
  }
  if ((rd == 31) || (rn == 31)) {
    return CuKind::Register31;
  }
  return std::nullopt;
}

constexpr std::optional<CuKind> cpy_constrained_unpredictable(std::uint8_t rd, std::uint8_t rs, std::uint8_t rn) {
  if ((rs == rn) || (rs == rd) || (rn == rd)) {
    return CuKind::RegisterOverlap;
  }
  if ((rd == 31) || (rs == 31) || (rn == 31)) {
    return CuKind::Register31;
  }
  return std::nullopt;
}

} // namespace mops::detail