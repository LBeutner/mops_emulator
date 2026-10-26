#include "mops/mops.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace {

///////////////////////////////////////////////////////
// CPU and Memory would already exist in an emulator //
///////////////////////////////////////////////////////

struct CpuState {
  std::array<std::uint64_t, 31> x{};
  std::uint64_t pc{0};
  bool n{false};
  bool z{false};
  bool c{false};
  bool v{false};
  bool irq{false};
};

class Memory {
public:
  explicit Memory(std::uint64_t base, std::size_t size) : m_base(base), m_ram(size, std::byte{0x00}) {}

  [[nodiscard]] bool read(std::uint64_t addr, std::span<std::byte> buf) {
    if (!accessible(addr, buf.size())) {
      return false;
    }
    const std::size_t offset{static_cast<std::size_t>(addr - m_base)};
    const std::span<std::byte> data{std::span{m_ram}.subspan(offset, buf.size())};
    std::ranges::copy(data, buf.begin());
    return true;
  }
  [[nodiscard]] bool write(std::uint64_t addr, std::span<const std::byte> cbuf) {
    if (!accessible(addr, cbuf.size())) {
      return false;
    }
    const std::size_t offset{static_cast<std::size_t>(addr - m_base)};
    const std::span<std::byte> dest{std::span{m_ram}.subspan(offset, cbuf.size())};
    std::ranges::copy(cbuf, dest.begin());
    return true;
  }
  [[nodiscard]] std::span<const std::byte> inspect() const { return m_ram; }

private:
  [[nodiscard]] bool accessible(std::uint64_t addr, std::size_t len) const {
    if (addr < m_base) {
      return false;
    }
    const std::size_t offset{static_cast<std::size_t>(addr - m_base)};
    return offset <= m_ram.size() && len <= (m_ram.size() - offset);
  }

  std::uint64_t m_base;
  std::vector<std::byte> m_ram;
};

/////////////////////////////////////////////////////
// Adapting to the library API via a small adapter //
/////////////////////////////////////////////////////

class MopsAdapter {
public:
  explicit MopsAdapter(CpuState& cpu, Memory& memory, const mops::ImplDefined& cfg)
      : m_cpu(cpu), m_memory(memory), m_cfg(cfg) {}

  [[nodiscard]] std::uint64_t read_reg(std::uint8_t reg) const { return m_cpu.x.at(reg); }
  void write_reg(std::uint8_t reg, std::uint64_t val) { m_cpu.x.at(reg) = val; }
  [[nodiscard]] std::uint32_t read_nzcv() const {
    return (static_cast<std::uint32_t>(m_cpu.n) << 3) | (static_cast<std::uint32_t>(m_cpu.z) << 2) |
           (static_cast<std::uint32_t>(m_cpu.c) << 1) | static_cast<std::uint32_t>(m_cpu.v);
  }
  void write_nzcv(std::uint32_t flags) {
    m_cpu.n = (flags & 0b1000) != 0;
    m_cpu.z = (flags & 0b0100) != 0;
    m_cpu.c = (flags & 0b0010) != 0;
    m_cpu.v = (flags & 0b0001) != 0;
  }
  // A real emulator would apply permissions for attr.unprivileged here
  // non_temporal has no effect
  [[nodiscard]] bool read_mem(std::uint64_t addr, std::span<std::byte> buf,
                              [[maybe_unused]] mops::AccessAttributes attr) {
    return m_memory.read(addr, buf);
  }
  [[nodiscard]] bool write_mem(std::uint64_t addr, std::span<const std::byte> cbuf,
                               [[maybe_unused]] mops::AccessAttributes attr) {
    return m_memory.write(addr, cbuf);
  }
  [[nodiscard]] bool should_yield() const { return m_cpu.irq; }
  [[nodiscard]] const mops::ImplDefined& impl_defined() const { return m_cfg; }

private:
  CpuState& m_cpu;
  Memory& m_memory;
  mops::ImplDefined m_cfg;
};

static_assert(mops::Host<MopsAdapter>);
static_assert(mops::HasImplDefined<MopsAdapter>);
static_assert(mops::HasYield<MopsAdapter>);

// Helper for printing CPU state
void print_cpu(const CpuState& cpu, std::string_view message) {
  std::cout << std::format("{} | PC: 0x{:02X} | X0(dest): 0x{:04X} | X1(size): {} | NZCV: {:d}{:d}{:d}{:d}", message,
                           cpu.pc, cpu.x.at(0), cpu.x.at(1), cpu.n, cpu.z, cpu.c, cpu.v)
            << std::endl;
}

// Helper for easier program composition
constexpr std::uint32_t encode(const mops::SetInstruction& instr) {
  const std::uint32_t stage{static_cast<std::uint32_t>(instr.stage)}; // Prologue 00, Main 01, Epilogue 10
  const std::uint32_t options{(instr.write.unprivileged ? 0b01U : 0U) | (instr.write.non_temporal ? 0b10U : 0U)};
  const std::uint32_t set{0x19C0'0400U}; // MOPS not SETG*
  return set | (std::uint32_t{instr.rs} << 16) | (((stage << 2) | options) << 12) |
         (std::uint32_t{instr.rn} << 5) | instr.rd;
}

// Test encodings from tools/asm2word.py
static_assert(encode(mops::SetInstruction{.stage = mops::Stage::Prologue, .rd = 0, .rs = 2, .rn = 1, .write = {}}) ==
              0x19c20420);
static_assert(encode(mops::SetInstruction{.stage = mops::Stage::Main, .rd = 0, .rs = 2, .rn = 1, .write = {}}) ==
              0x19c24420);
static_assert(encode(mops::SetInstruction{.stage = mops::Stage::Epilogue, .rd = 0, .rs = 2, .rn = 1, .write = {}}) ==
              0x19c28420);

} // namespace

/////////////////////////
// Small demo emulator //
/////////////////////////

int main() {
  const std::uint64_t kMemBase{0x0000};
  const std::uint64_t kMemSize{32};
  const std::uint64_t kSize{20};

  CpuState cpu{};
  Memory ram{kMemBase, kMemSize};

  const mops::ImplDefined cfg{.set_prologue_bytes = 4, .set_epilogue_bytes = 4, .set_block_bytes = 6};

  MopsAdapter adapter{cpu, ram, cfg};

  cpu.x.at(0) = kMemBase;
  cpu.x.at(1) = kSize;
  cpu.x.at(2) = 0xAB;
  cpu.irq = true; // Interrupt as soon as its allowed, meaning after set_block_bytes in Main stage

  const std::array<std::uint32_t, 3> program{
      encode(mops::SetInstruction{.stage = mops::Stage::Prologue, .rd = 0, .rs = 2, .rn = 1, .write = {}}),
      encode(mops::SetInstruction{.stage = mops::Stage::Main, .rd = 0, .rs = 2, .rn = 1, .write = {}}),
      encode(mops::SetInstruction{.stage = mops::Stage::Epilogue, .rd = 0, .rs = 2, .rn = 1, .write = {}})};

  print_cpu(cpu, "Initial State");

  while (cpu.pc < program.size() * 4) {
    const std::uint32_t instr{program[cpu.pc / 4]};
    mops::StepResult result{mops::step(adapter, instr)};

    bool stop{false};

    std::visit(mops::overloaded{
                   [&](mops::Completed) {
                     cpu.pc += 4;
                     print_cpu(cpu, "Completed");
                   },
                   [&](mops::Interrupted) {
                     print_cpu(cpu, "Interrupted");
                     std::cout << "Handling interrupt and resuming..." << std::endl;
                     cpu.irq = false;
                   },
                   [&](mops::Fault fault) {
                     std::cout << "Memory fault at " << std::format("0x{:08X}", fault.address) << std::endl;
                     stop = true;
                   },
                   [&](mops::MopsException exception) {
                     std::cout << "MopsException with ISS: " << std::format("0x{:07X}", exception.iss) << std::endl;
                     stop = true;
                   },
                   [&](mops::Undefined) {
                     std::cout << "Undefined, either CU or malformed opcodes" << std::endl;
                     stop = true;
                   },
                   [&](mops::Unsupported) {
                     std::cout << "Unsupported, not SET*" << std::endl;
                     stop = true;
                   },
                   [&](mops::NotMops) {
                     std::cout << "NotMops" << std::endl;
                     stop = true;
                   },
               },
               result);

    if (stop) {
      break;
    }
  }

  std::cout << std::format("\nRAM [0x{:04X} ... 0x{:04X}):", kMemBase, kMemBase + kMemSize) << std::endl;
  for (std::byte val : ram.inspect()) {
    std::cout << std::format("{:02X} ", static_cast<int>(val));
  }
  std::cout << std::endl;
  return 0;
}