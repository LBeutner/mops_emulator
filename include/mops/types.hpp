#pragma once
#include <cstdint>
#include <variant>

namespace mops {

// Helper for std::visit
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

///////////////////////////////////////////////////////////////////////
// Shared return types for decode, execute and step(decode + execute) //
///////////////////////////////////////////////////////////////////////

// Instruction completed, PC advances
struct Completed {
  bool operator==(const Completed&) const = default;
};

// Main stage interrupted, registers hold progress, PC stays
struct Interrupted {
  bool operator==(const Interrupted&) const = default;
};

// Memory access failed, PC stays
struct Fault {
  std::uint64_t address;
  bool is_write;
  bool operator==(const Fault&) const = default;
};

// Memory Copy and Memory Set exceptions (D1.4.5.8)
struct MopsException {
  std::uint32_t iss;
  bool operator==(const MopsException&) const = default;
};

// Non existent encoding or constrained unpredictable
struct Undefined {
  bool operator==(const Undefined&) const = default;
};

// Valid, but not implemented MOPS instruction (SETG, OptionA)
struct Unsupported {
  bool operator==(const Unsupported&) const = default;
};

// Non MOPS instruction
struct NotMops {
  bool operator==(const NotMops&) const = default;
};

using DecodeResult = std::variant<Decoded, Undefined, Unsupported, NotMops>;
using ExecuteResult = std::variant<Completed, Interrupted, Fault, MopsException, Undefined, Unsupported>;
using StepResult = std::variant<Completed, Interrupted, Fault, MopsException, Undefined, Unsupported, NotMops>;

} // namespace mops