#include "mops/step.hpp"
#include "support/dummy_host.hpp"
#include "support/printers.hpp"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

// Instructions from llvm-mc-18 (tools/asm2word.py), using x0 = Xd, x1 = Xn, x2 = Xs
namespace {

constexpr std::uint32_t kSetp{0x19c20420}; // setp [x0]!, x1!, x2
constexpr std::uint32_t kSetm{0x19c24420}; // setm [x0]!, x1!, x2
constexpr std::uint32_t kSete{0x19c28420}; // sete [x0]!, x1!, x2

// Register values used for testing
constexpr std::uint64_t kDest{0x10};
constexpr std::uint64_t kSize{100};
constexpr std::byte kValue{0xAB};

// Host memory prefilled with repeating kMem
constexpr std::byte kMem{0xCD};

// PSTATE for option A after Prologue
constexpr std::uint32_t kOptionA{0b0000};

const mops::StepResult kCompleted{mops::Completed{}};
const mops::StepResult kInterrupted{mops::Interrupted{}};
const mops::StepResult kUndefined{mops::Undefined{}};
const mops::StepResult kUnsupported{mops::Unsupported{}};
const mops::StepResult kNotMops{mops::NotMops{}};

DummyHost make_host() {
  DummyHost host;
  host.memory.fill(kMem);
  host.regs.at(0) = kDest;
  host.regs.at(1) = kSize;
  host.regs.at(2) = static_cast<std::uint64_t>(kValue);
  return host;
}

// True if memory [kDest, kDest + size) holds value and everything else is untouched
bool mem_correct(const DummyHost& host, std::size_t size, std::byte value = kValue) {
  for (std::size_t i{0}; i < host.memory.size(); i++) {
    const bool inside{i >= kDest && i < kDest + size};
    if (host.memory[i] != (inside ? value : kMem)) {
      return false;
    }
  }
  return true;
}

} // namespace

TEST(Step, EncodedSetSequence) {
  DummyHost host{make_host()};
  for (const std::uint32_t word : {kSetp, kSetm, kSete}) {
    EXPECT_EQ(mops::step(host, word), kCompleted);
  }

  EXPECT_EQ(host.regs.at(0), kDest + kSize);
  EXPECT_EQ(host.regs.at(1), 0);
  EXPECT_TRUE(mem_correct(host, kSize));
}

TEST(Step, PassesDecodeResultsThrough) {
  DummyHost host{make_host()};
  EXPECT_EQ(mops::step(host, 0xd28000a0), kNotMops);     // mov x0 #5
  EXPECT_EQ(mops::step(host, 0x59c20420), kUndefined);   // setp [x0]!, x1!, x2 with sz = 01
  EXPECT_EQ(mops::step(host, 0x1dc20420), kUnsupported); // setgp [x0]!, x1!, x2
}

TEST(Step, PassesExecuteResultsThrough) {
  DummyHost host{make_host()};
  EXPECT_EQ(mops::step(host, 0x19c20421), kUndefined);   // setp [x1]!, x1!, x2 with Xd == Xn
  EXPECT_EQ(mops::step(host, 0x19010440), kUnsupported); // cpyfp [x0]!, [x1]!, x2! with CPY not implemented

  // Mismatched core type passed through
  host.nzcv = kOptionA;
  mops::StepResult result{mops::MopsException{.iss = 0x01020041}};
  EXPECT_EQ(mops::step(host, kSetm), result);

  // Memory fault passed through
  const std::uint64_t fault_offset{50};
  host.fault_address = kDest + fault_offset;
  EXPECT_EQ(mops::step(host, kSetp), kCompleted);
  const mops::StepResult expected_fault{mops::Fault{.address = kDest + fault_offset, .is_write = true}};
  EXPECT_EQ(mops::step(host, kSetm), expected_fault);

  // Interrupt passed through
  host.fault_address = ULONG_MAX;
  host.config.set_block_bytes = 16;
  host.yield = true;
  EXPECT_EQ(mops::step(host, kSetm), kInterrupted);
}