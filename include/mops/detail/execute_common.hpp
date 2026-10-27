#pragma once
#include "mops/host_concepts.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace mops::detail {

// ArchMaxMOPSBlockSize
inline constexpr std::uint64_t kArchMaxMopsBlockSize{0x7FFF'FFFF'FFFF'FFFF};

// NZCV after Prologue for Option B
inline constexpr std::uint32_t kNzcvOptionB{0b0010};

// PSTATE.{N,Z,C,V} bit mask for C
inline constexpr std::uint32_t kPstateC{0b0010};

// Size of stack buffer, not visible by host
inline constexpr std::size_t kChunkBytes{4096};

// Reasons for the main loop to end
enum class LoopEnd : std::uint8_t { Done, Interrupted, Fault };

// Outcome of a stage loop
struct LoopResult {
  LoopEnd end{LoopEnd::Done};
  std::int64_t done{0}; // Completed bytes before fault
};

// At most size bytes, at least 0 for size >= 0.
// For a negative size (Option A format in M/E) we return 0 and check_set_params() raises the exception.
constexpr std::int64_t clamp_to_size(std::int64_t bytes, std::int64_t size) {
  return (size >= 0) ? std::clamp<std::int64_t>(bytes, 0, size) : 0;
}

// SETSizeChoice(), clamped to [1, stagesize]
constexpr std::int64_t clamp_bytes(std::int64_t configured, std::int64_t stagesize) {
  return std::min(std::max<std::int64_t>(configured, 1), stagesize);
}

// MemSetBytes()
// Writes n bytes at addr to data
// Returns the number of bytes written before a fault occured
template <Host H>
std::int64_t set_bytes(H& host, std::uint64_t addr, std::byte data, std::int64_t n, AccessAttributes attr) {
  const auto total{static_cast<std::size_t>(n)};

  // Host is able to provide direct memory access
  std::byte* direct_ptr{get_host_ptr(host, addr, total, attr)};
  if (direct_ptr) {
    std::fill_n(direct_ptr, total, data);
    return n;
  }

  // We can only write to host via write_mem
  std::array<std::byte, kChunkBytes> chunk{};
  chunk.fill(data);
  std::size_t done{0};
  while (done < total) {
    const std::span<const std::byte> bytes{chunk.data(), std::min(kChunkBytes, total - done)};
    // Try writing whole chunk
    if (!host.write_mem(addr + done, bytes, attr)) {
      // Chunk write, failed, write byte by byte to check where
      for (std::size_t i{0}; i < bytes.size(); i++) {
        if (!host.write_mem(addr + done + i, bytes.subspan(i, 1), attr)) {
          return static_cast<std::int64_t>(done + i);
        }
      }
    }
    done += bytes.size();
  }
  return n;
}

} // namespace mops::detail