#pragma once
#include "mops/types.hpp"
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

inline void PrintTo(DecodeError e, std::ostream* os) {
  switch (e) {
  case DecodeError::NotMops:
    *os << "NotMops";
    return;
  case DecodeError::Undefined:
    *os << "Undefined";
    return;
  case DecodeError::Unsupported:
    *os << "Unsupported";
    return;
  }
  *os << "DecodeError(" << static_cast<int>(e) << ")";
}

inline void PrintTo(StageResult r, std::ostream* os) {
  switch (r) {
  case StageResult::Done:
    *os << "Done";
    return;
  case StageResult::Interrupted:
    *os << "Continue";
    return;
  case StageResult::Fault:
    *os << "Fault";
    return;
  case StageResult::MopsException:
    *os << "MopsException";
    return;
  }
  *os << "StepResult(" << static_cast<int>(r) << ")";
}

inline void PrintTo(AccessAttributes attr, std::ostream* os) {
  *os << "{";
  if (attr.unprivileged) {
    *os << " unpriv";
  }
  if (attr.non_temporal) {
    *os << " nt";
  }
  *os << " }";
}

inline void PrintTo(const CpyInstruction& instr, std::ostream* os) {
  PrintTo(instr.family, os);
  *os << " ";
  PrintTo(instr.stage, os);
  *os << " x" << int{instr.rd} << ", x" << int{instr.rs} << ", x" << int{instr.rn};
  *os << " read=";
  PrintTo(instr.read, os);
  *os << " write=";
  PrintTo(instr.write, os);
}

inline void PrintTo(const SetInstruction& instr, std::ostream* os) {
  *os << "SET ";
  PrintTo(instr.stage, os);
  *os << " x" << int{instr.rd} << ", x" << int{instr.rs} << ", x" << int{instr.rn};
  *os << " write=";
  PrintTo(instr.write, os);
}

} // namespace mops