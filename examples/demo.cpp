#include "mops/mops.hpp"
#include "movz.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
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
void print_changes(const CpuState& before, const CpuState& after, std::string_view message) {
  std::cout << std::format("0x{:04X}: {:<12} |", before.pc, message);
  for (std::size_t i{0}; i < after.x.size(); i++) {
    if (before.x.at(i) != after.x.at(i)) {
      std::cout << std::format(" (reg x{}: 0x{:X} -> 0x{:X})", i, before.x.at(i), after.x.at(i));
    }
  }

  const auto flags{
      [](const CpuState& state) { return std::format("{:d}{:d}{:d}{:d}", state.n, state.z, state.c, state.v); }};

  if (flags(before) != flags(after)) {
    std::cout << std::format(" (nzcv {} -> {})", flags(before), flags(after));
  }
  std::cout << "\n";
}

std::vector<std::uint32_t> load_program(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::binary};
  if (!file) {
    throw std::runtime_error("cannot open: " + path.string());
  }
  const std::vector<unsigned char> buf{std::istreambuf_iterator<char>{file}, {}};
  if (buf.size() % 4 != 0) {
    throw std::runtime_error("not multiple of 4 bytes");
  }

  std::vector<std::uint32_t> words;
  words.reserve(buf.size() / 4);
  for (std::size_t i{0}; i < buf.size(); i += 4) {
    words.push_back(std::uint32_t{buf[i]} | (std::uint32_t{buf[i + 1]} << 8) | (std::uint32_t{buf[i + 2]} << 16) |
                    (std::uint32_t{buf[i + 3]} << 24));
  }
  return words;
}

int run(const char* path) {
  const std::uint64_t kMemBase{0x0000};
  const std::uint64_t kMemSize{512};

  const std::vector<std::uint32_t> program{load_program(path)};

  CpuState cpu{};
  Memory ram{kMemBase, kMemSize};

  const mops::ImplDefined cfg{.set_prologue_bytes = 4, .set_epilogue_bytes = 4, .set_block_bytes = 6};

  MopsAdapter adapter{cpu, ram, cfg};

  cpu.irq = true; // Interrupt as soon as its allowed, meaning after set_block_bytes in Main stage

  while (cpu.pc < program.size() * 4) {
    const CpuState before{cpu};

    const std::uint32_t instr{program[cpu.pc / 4]};
    mops::StepResult result{mops::step(adapter, instr)};

    bool stop{false};

    std::visit(mops::overloaded{
                   [&](mops::Completed) {
                     cpu.pc += 4;
                     print_changes(before, cpu, "Completed");
                   },
                   [&](mops::Interrupted) {
                     print_changes(before, cpu, "Interrupted");
                     std::cout << "Handling interrupt and resuming..." << "\n";
                     cpu.irq = false;
                   },
                   [&](mops::Fault fault) {
                     std::cout << "Memory fault at " << std::format("0x{:08X}", fault.address) << "\n";
                     stop = true;
                   },
                   [&](mops::MopsException exception) {
                     std::cout << "MopsException with ISS: " << std::format("0x{:07X}", exception.iss) << "\n";
                     stop = true;
                   },
                   [&](mops::Undefined) {
                     std::cout << "Undefined, either CU or malformed opcodes" << "\n";
                     stop = true;
                   },
                   [&](mops::Unsupported) {
                     std::cout << "Unsupported, not SET*" << "\n";
                     stop = true;
                   },
                   [&](mops::NotMops) {
                     const auto decode_result{movz::decode(instr)};
                     if (!decode_result) {
                       std::cout << "Not Mops or MOVZ" << "\n";
                       stop = true;
                       return;
                     }
                     if (decode_result->reg != 31) { // Dont touch xzr
                       cpu.x.at(decode_result->reg) = std::uint64_t{decode_result->imm} << (decode_result->shift * 16);
                     }
                     cpu.pc += 4;
                     print_changes(before, cpu, "MOVZ");
                   },
               },
               result);

    if (stop) {
      break;
    }
  }

  std::cout << std::format("\nRAM [0x{:04X} ... 0x{:04X}):", kMemBase, kMemBase + kMemSize);
  for (std::size_t i{0}; i < ram.inspect().size(); i++) {
    if (i % 16 == 0) {
      std::cout << std::format("\n0x{:04X}:", i);
    }
    std::cout << std::format(" {:02X}", static_cast<int>(ram.inspect()[i]));
  }
  std::cout << "\n";
  return 0;
}

} // namespace

/////////////////////////
// Small demo emulator //
/////////////////////////

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    std::cout << "no program file provided" << "\n";
    return 1;
  }

  try {
    return run(argv[1]);
  } catch (const std::exception& e) {
    std::cout << e.what() << "\n";
    return 1;
  }
}