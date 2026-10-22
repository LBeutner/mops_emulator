#pragma once
#include <cstdint>
#include <variant>

namespace mops {

// helper for std::visit
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

enum class Option : std::uint8_t { A, B };

enum class Stage : std::uint8_t { Prologue, Main, Epilogue };

enum class CpyFamily : std::uint8_t { Cpy, CpyF };

struct AccessAttributes {
  bool unprivileged;
  bool non_temporal;

  bool operator==(const AccessAttributes&) const = default;
};

struct CpyInstruction {
  CpyFamily family;
  Stage stage;
  std::uint8_t rd, rs, rn;
  AccessAttributes read, write;

  bool operator==(const CpyInstruction&) const = default;
};

struct SetInstruction {
  Stage stage;
  std::uint8_t rd, rs, rn;
  AccessAttributes write;

  bool operator==(const SetInstruction&) const = default;
};

using Decoded = std::variant<CpyInstruction, SetInstruction>;

enum class DecodeError : std::uint8_t {
  NotMops,    // instruction not part of FEAT_MOPS
  Undefined,  // part of FEAT_MOPS but undefined or constrained unpredictable
  Unsupported // part of FEAT_MOPS but not implemented
};

using DecodeResult = std::variant<Decoded, DecodeError>;

enum class StageResult : std::uint8_t {
  Done,
  Interrupted,
  Fault,        // memory access error
  MopsException // f.e. option mismatch
};

} // namespace mops