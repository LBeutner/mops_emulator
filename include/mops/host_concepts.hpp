#pragma once
#include "mops/types.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace mops {

// required concept
template <typename H>
concept Host = requires(H host, std::uint8_t reg, std::uint32_t flags, std::uint64_t val, std::uint64_t addr,
                        std::span<std::byte> buf, std::span<const std::byte> cbuf, AccessAttributes attr) {
  { host.read_reg(reg) } -> std::same_as<std::uint64_t>; // x0-x30, the library handles xzr
  { host.write_reg(reg, val) } -> std::same_as<void>;
  { host.read_nzcv() } -> std::same_as<std::uint32_t>; // expects PSTATE.{N,Z,C,V} in bits [3:0]
  { host.write_nzcv(flags) } -> std::same_as<void>;
  { host.read_mem(addr, buf, attr) } -> std::convertible_to<bool>;
  { host.write_mem(addr, cbuf, attr) } -> std::convertible_to<bool>;
};

// optional concept, if host allows direct memory access
template <typename H>
concept HasDirectMem = requires(H host, std::uint64_t addr, std::size_t len, AccessAttributes attr) {
  { host.host_ptr(addr, len, attr) } -> std::same_as<std::byte*>;
};

template <typename H> std::byte* get_host_ptr(H& host, std::uint64_t addr, std::size_t len, AccessAttributes attr) {
  if constexpr (HasDirectMem<H>) {
    return host.host_ptr(addr, len, attr);
  } else {
    return nullptr;
  }
}

// optional concept, if host allows interrupts
template <typename H>
concept HasYield = requires(H host) {
  { host.should_yield() } -> std::same_as<bool>;
};

template <typename H> bool should_yield(H& host) {
  if constexpr (HasYield<H>) {
    return host.should_yield();
  } else {
    return false;
  }
}

// optional concept, if host has custom implementation
template <typename H>
concept HasImplDefined = requires(const H host) {
  { host.impl_defined() } -> std::same_as<const ImplDefined&>;
};

template <typename H> const ImplDefined& get_impl_defined(const H& host) {
  static constexpr ImplDefined kDefault{};
  if constexpr (HasImplDefined<H>) {
    return host.impl_defined();
  } else {
    return kDefault;
  }
}

} // namespace mops