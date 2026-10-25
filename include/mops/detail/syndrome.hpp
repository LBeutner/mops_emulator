#pragma once
#include "mops/types.hpp"
#include <cstdint>

namespace mops::detail {

// Fields of the exception syndrome
struct MopsSyndrome {
  std::uint8_t options;
  bool from_epilogue;
  bool mismatch; // true = option mismatch, otherwise ill-formed parameters
  std::uint8_t rd, rs, rn;
};

// Constructs iss to pass back to host
constexpr std::uint32_t set_iss(const MopsSyndrome& synd) {
  const std::uint32_t formatoption{synd.mismatch ? 0b10U : 0b00U};
  return (1U << 24) | ((synd.options & 0b11U) << 19) | ((synd.from_epilogue ? 1U : 0U) << 18) |
         ((formatoption & 0b11U) << 16) | ((synd.rd & 0b11111U) << 10) | ((synd.rs & 0b11111U) << 5) |
         (synd.rn & 0b11111U);
}

// Converts option struct to std::uint8_t with options at the 2 LSBs
constexpr std::uint8_t set_options(const SetInstruction& instr) {
  return static_cast<std::uint8_t>((instr.write.unprivileged ? 0b01U : 0b00U) |
                                   (instr.write.non_temporal ? 0b10U : 0b00U));
}

} // namespace mops::detail