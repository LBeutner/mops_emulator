#pragma once
#include "mops/detail/execute_set.hpp"
#include "mops/host_concepts.hpp"
#include <variant>

namespace mops {

template <Host H> ExecuteResult execute(H& host, const Decoded& decoded) {
  return std::visit<ExecuteResult>(
      overloaded{[&host](const SetInstruction& instr) { return detail::execute_set(host, instr); },
                 []([[maybe_unused]] const CpyInstruction& instr) { return Unsupported{}; }},
      decoded);
}

} // namespace mops