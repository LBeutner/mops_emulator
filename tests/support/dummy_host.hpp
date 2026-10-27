#pragma once
#include "mops/host_concepts.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

struct DummyHost {
  [[nodiscard]] std::uint64_t read_reg(std::uint8_t reg) const { return regs.at(reg); }
  void write_reg(std::uint8_t reg, std::uint64_t val) { regs.at(reg) = val; }
  [[nodiscard]] std::uint32_t read_nzcv() const { return nzcv; }
  void write_nzcv(std::uint32_t flags) { nzcv = flags; }
  [[nodiscard]] bool accessible(std::uint64_t addr, std::size_t len) const {
    return addr <= memory.size() && len <= memory.size() - addr &&
           (fault_address < addr || fault_address >= addr + len);
  }
  [[nodiscard]] bool read_mem(std::uint64_t addr, std::span<std::byte> buf,
                              [[maybe_unused]] mops::AccessAttributes attr) {
    if (!accessible(addr, buf.size())) {
      return false;
    }
    std::copy_n(memory.begin() + addr, buf.size(), buf.begin());
    return true;
  }
  [[nodiscard]] bool write_mem(std::uint64_t addr, std::span<const std::byte> cbuf,
                               [[maybe_unused]] mops::AccessAttributes attr) {
    write_calls++;
    if (!accessible(addr, cbuf.size())) {
      return false;
    }
    std::copy(cbuf.begin(), cbuf.end(), memory.begin() + addr);
    return true;
  }
  [[nodiscard]] bool should_yield() const { return yield; }
  [[nodiscard]] std::byte* host_ptr(std::uint64_t addr, std::size_t len, [[maybe_unused]] mops::AccessAttributes attr) {
    if (!direct_mem || !accessible(addr, len)) {
      return nullptr;
    }
    return memory.data() + addr;
  }
  [[nodiscard]] const mops::ImplDefined& impl_defined() const { return config; }

  std::array<std::uint64_t, 31> regs{}; // 31 because xzr is handled by library
  std::uint32_t nzcv{};
  std::array<std::byte, 8192> memory{};
  mops::ImplDefined config{};
  bool yield{false};
  std::uint64_t fault_address{ULLONG_MAX};
  bool direct_mem{false};     // override to make a test use direct memory access
  std::size_t write_calls{0}; // write_mem call counter
};

static_assert(mops::Host<DummyHost>);
static_assert(mops::HasYield<DummyHost>);
static_assert(mops::HasImplDefined<DummyHost>);
static_assert(mops::HasDirectMem<DummyHost>);