#pragma once
#include "mops/types.hpp"
#include <format>
#include <gtest/gtest.h>
#include <ostream>

namespace mops {

inline void PrintTo(Stage p, std::ostream* os) {
  switch (p) {
  case Stage::Prologue:
    *os << "Prologue";
    return;
  case Stage::Main:
    *os << "Main";
    return;
  case Stage::Epilogue:
    *os << "Epilogue";
    return;
  }
  *os << "Stage(" << static_cast<int>(p) << ")";
}

inline void PrintTo(CpyFamily f, std::ostream* os) {
  switch (f) {
  case CpyFamily::Cpy:
    *os << "CPY";
    return;
  case CpyFamily::CpyF:
    *os << "CPYF";
    return;
  }
  *os << "CpyFamily(" << static_cast<int>(f) << ")";
}

inline void PrintTo(AccessAttributes attr, std::ostream* os) {
  *os << std::format("{{{}{} }}", attr.unprivileged ? " unpriv" : "", attr.non_temporal ? " nt" : "");
}

inline void PrintTo(const CpyInstruction& instr, std::ostream* os) {
  *os << std::format("{} {} x{}, x{}, x{} read={} write={}", testing::PrintToString(instr.family),
                     testing::PrintToString(instr.stage), instr.rd, instr.rs, instr.rn,
                     testing::PrintToString(instr.read), testing::PrintToString(instr.write));
}

inline void PrintTo(const SetInstruction& instr, std::ostream* os) {
  *os << std::format("SET {} x{}, x{}, x{} write={}", testing::PrintToString(instr.stage), instr.rd, instr.rs, instr.rn,
                     testing::PrintToString(instr.write));
}

inline void PrintTo([[maybe_unused]] const Completed completed, std::ostream* os) {
  *os << "Completed";
}

inline void PrintTo([[maybe_unused]] const Interrupted interrupted, std::ostream* os) {
  *os << "Interrupted";
}

inline void PrintTo(const Fault fault, std::ostream* os) {
  *os << std::format("Fault{{0x{:x}, {}}}", fault.address, fault.is_write ? "write" : "read");
}

inline void PrintTo(const MopsException exception, std::ostream* os) {
  *os << std::format("MopsException{{iss=0x{:x}}}", exception.iss);
}

inline void PrintTo([[maybe_unused]] const Undefined undefined, std::ostream* os) {
  *os << "Undefined";
}

inline void PrintTo([[maybe_unused]] const Unsupported unsupported, std::ostream* os) {
  *os << "Unsupported";
}

inline void PrintTo([[maybe_unused]] const NotMops notmops, std::ostream* os) {
  *os << "NotMops";
}

} // namespace mops