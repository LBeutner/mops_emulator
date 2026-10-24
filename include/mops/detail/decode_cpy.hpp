#pragma once
#include "mops/detail/decode_fields.hpp"
#include "mops/types.hpp"
#include <optional>

namespace mops::detail {

struct CpyAttributes {
  AccessAttributes read;
  AccessAttributes write;
};

constexpr std::optional<Stage> decode_cpy_stage(std::uint8_t bits) {
  switch (bits) {
  case 0b11:
    return std::nullopt;
  case 0b00:
    return Stage::Prologue;
  case 0b01:
    return Stage::Main;
  case 0b10:
    return Stage::Epilogue;
  default:
    return std::nullopt;
  }
}

// op2[0] encodes write privilege level, op2[1] encodes read privilege level
// op2[2] encodes write temporal/non-temporal, op2[3] encodes read temporal/non-temporal
constexpr CpyAttributes decode_cpy_attr(std::uint8_t bits) {
  AccessAttributes read{.unprivileged = static_cast<bool>((bits >> 1) & 0b1),
                        .non_temporal = static_cast<bool>((bits >> 3) & 0b1)};

  AccessAttributes write{.unprivileged = static_cast<bool>(bits & 0b1),
                         .non_temporal = static_cast<bool>((bits >> 2) & 0b1)};

  return CpyAttributes{.read = read, .write = write};
}

constexpr DecodeResult decode_cpy(const RawFields& fields) {
  // op1[1:0] encodes stage, [00] = Prologue, [01] = Main, [10] = Epilogue
  const auto stage{decode_cpy_stage(fields.op1)};
  // Should never return std::nullopt, unless called directly
  if (!stage) {
    return Undefined{};
  }

  // o0[0] encodes type of CPY, either [0] = CPYF or [1] = CPY
  const CpyFamily family{static_cast<bool>(fields.o0) ? CpyFamily::Cpy : CpyFamily::CpyF};

  // op2[3:0] encodes attributes
  const CpyAttributes attr{decode_cpy_attr(fields.op2)};

  return CpyInstruction{.family = family,
                        .stage = *stage,
                        .rd = fields.rd,
                        .rs = fields.rs,
                        .rn = fields.rn,
                        .read = attr.read,
                        .write = attr.write};
}

} // namespace mops::detail