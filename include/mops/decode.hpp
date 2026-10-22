#pragma once
#include "mops/detail/decode_cpy.hpp"
#include "mops/detail/decode_fields.hpp"
#include "mops/detail/decode_set.hpp"
#include "mops/types.hpp"

namespace mops {

constexpr DecodeResult decode(std::uint32_t word) noexcept {
  if (!detail::is_mops(word)) {
    return DecodeError::NotMops;
  }
  const detail::RawFields fields{detail::extract_fields(word)};
  if (fields.size != 0b00) {
    return DecodeError::Undefined;
  }
  return detail::is_set_family(fields) ? detail::decode_set(fields) : detail::decode_cpy(fields);
}

} // namespace mops