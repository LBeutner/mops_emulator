#pragma once
#include "mops/detail/decode_fields.hpp"
#include "mops/types.hpp"
#include <optional>

namespace mops::detail {

// [00] = Prologue, [01] = Main, [10] = Epilogue and [11] = unsupported
constexpr std::optional<Stage> decode_set_stage(std::uint8_t bits) {
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

// op2[0] encodes privilege level, op2[1] encodes temporal/non-temporal
constexpr AccessAttributes decode_set_attr(std::uint8_t bits) {
  return AccessAttributes{.unprivileged = static_cast<bool>(bits & 0b1),
                          .non_temporal = static_cast<bool>((bits >> 1) & 0b1)};
}

constexpr DecodeResult decode_set(const RawFields& fields) {
  // o0[0] encodes type of SET instruction, SETG not supported
  if (fields.o0 == 0b1) {
    return Unsupported{};
  }

  // op2[3:2] encodes stage
  const auto stage{decode_set_stage(fields.op2 >> 2)};
  if (!stage) {
    return Undefined{};
  }

  // op2[1:0] encodes attributes
  const AccessAttributes attr{decode_set_attr(fields.op2 & 0b11)};

  return SetInstruction{.stage = *stage, .rd = fields.rd, .rs = fields.rs, .rn = fields.rn, .write = attr};
}

} // namespace mops::detail