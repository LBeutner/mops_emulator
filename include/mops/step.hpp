#pragma once
#include "mops/decode.hpp"
#include "mops/execute.hpp"

namespace mops {

template <Host H> StepResult step(H& host, std::uint32_t word) {
  DecodeResult decode_result{decode(word)};

  return std::visit<StepResult>(
      overloaded{
          [](Undefined result) { return result; },
          [](Unsupported result) { return result; },
          [](NotMops result) { return result; },
          [&host](const Decoded& decoded) {
            return std::visit<StepResult>(
                overloaded{[](Completed result) { return result; }, [](Interrupted result) { return result; },
                           [](Fault result) { return result; }, [](MopsException result) { return result; },
                           [](Undefined result) { return result; }, [](Unsupported result) { return result; }},
                execute(host, decoded));
          },
      },
      decode_result);
}

} // namespace mops