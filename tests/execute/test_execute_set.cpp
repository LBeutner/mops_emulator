#include "mops/execute.hpp"
#include "support/dummy_host.hpp"
#include "support/printers.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <string>
#include <tuple>

namespace {

using mops::ExecuteResult;
using mops::Stage;

// Register to use for all tests
constexpr std::size_t kDestReg{0};
constexpr std::size_t kSizeReg{1};
constexpr std::size_t kValueReg{2};

// Register values used for testing
constexpr std::uint64_t kDest{0x10};
constexpr std::byte kValue{0xAB};

// Host memory prefilled with repeating kMem
constexpr std::byte kMem{0xCD};

// PSTATE for option A and B after Prologue
constexpr std::uint32_t kOptionB{0b0010};
constexpr std::uint32_t kOptionA{0b0000};

// Max block size
constexpr std::uint64_t kArchMaxMopsBlockSize{0x7FFF'FFFF'FFFF'FFFF};

const ExecuteResult kCompleted{mops::Completed{}};
const ExecuteResult kInterrupted{mops::Interrupted{}};
const ExecuteResult kUndefined{mops::Undefined{}};

// SETx [kDestReg]!, kSizeReg!, kValueReg
ExecuteResult run(DummyHost& host, Stage stage, mops::AccessAttributes write = {}) {
  return mops::execute(
      host, mops::SetInstruction{.stage = stage, .rd = kDestReg, .rs = kValueReg, .rn = kSizeReg, .write = write});
}

// Memory filled with kMem, kDestReg = Xd, kSizeReg = size, kValueReg = kValue
DummyHost make_host(std::uint64_t size) {
  DummyHost host;
  host.memory.fill(kMem);
  host.regs.at(kDestReg) = kDest;
  host.regs.at(kSizeReg) = size;
  host.regs.at(kValueReg) = static_cast<std::uint64_t>(kValue);
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

// Sweep helpers

// size, prologue bytes, epilogue bytes, block bytes
class SetSweep : public testing::TestWithParam<std::tuple<std::uint64_t, std::int64_t, std::int64_t, std::int64_t>> {};

auto names = [](const testing::TestParamInfo<SetSweep::ParamType>& info) {
  const auto [size, prologue, epilogue, block] = info.param;
  std::string name{std::format("Size{}_P{}_E{}_BSize{}", size, prologue, epilogue, block)};
  return name;
};

} // namespace

//////////////////////////////////////////////////////
// Sweep for size, prologue, epilogue and blocksize //
//////////////////////////////////////////////////////

TEST_P(SetSweep, BehavesLikeMemset) {
  const auto [size, prologue, epilogue, block] = GetParam();
  DummyHost host{make_host(size)};
  host.config = {.set_prologue_bytes = prologue, .set_epilogue_bytes = epilogue, .set_block_bytes = block};
  const std::uint64_t pre{std::min<std::uint64_t>(prologue, size)};
  const std::uint64_t post{std::min<std::uint64_t>(epilogue, size - pre)};

  EXPECT_EQ(run(host, Stage::Prologue), kCompleted);
  EXPECT_EQ(host.nzcv, kOptionB);
  EXPECT_EQ(host.regs[kDestReg], kDest + pre);
  EXPECT_EQ(host.regs[kSizeReg], size - pre);

  EXPECT_EQ(run(host, Stage::Main), kCompleted);
  EXPECT_EQ(host.nzcv, kOptionB);
  EXPECT_EQ(host.regs[kDestReg], kDest + size - post);
  EXPECT_EQ(host.regs[kSizeReg], post);

  EXPECT_EQ(run(host, Stage::Epilogue), kCompleted);
  EXPECT_EQ(host.nzcv, kOptionB);
  EXPECT_EQ(host.regs[kDestReg], kDest + size);
  EXPECT_EQ(host.regs[kSizeReg], 0);

  EXPECT_TRUE(mem_correct(host, size));
}

INSTANTIATE_TEST_SUITE_P(Splits, SetSweep,
                         testing::Combine(testing::Values(0, 1, 4, 32, 128, 1024, 4096, 8000),
                                          testing::Values(0, 2, 64, 10000), testing::Values(0, 2, 64, 10000),
                                          testing::Values(1, 16, 4096, 10000)),
                         names);

///////////////
// RegValues //
///////////////

TEST(SetExecute, LowByteOfXs) {
  DummyHost host{make_host(16)};
  host.regs[kValueReg] = 0x1234'5678'9ABC'DEF0;
  for (const Stage stage : {Stage::Prologue, Stage::Main, Stage::Epilogue}) {
    EXPECT_EQ(run(host, stage), kCompleted);
  }
  EXPECT_TRUE(mem_correct(host, 16, std::byte{0xF0}));
}

TEST(SetExecute, XzrAsXsZeros) {
  DummyHost host{make_host(16)};
  for (const Stage stage : {Stage::Prologue, Stage::Main, Stage::Epilogue}) {
    const mops::SetInstruction instr{.stage = stage, .rd = 0, .rs = 31, .rn = 1, .write = {}};
    EXPECT_EQ(mops::execute(host, instr), kCompleted);
  }
  EXPECT_TRUE(mem_correct(host, 16, std::byte{0}));
}

TEST(SetExecute, PrologueClampsSize) {
  DummyHost host{make_host(kArchMaxMopsBlockSize + 1)};
  EXPECT_EQ(run(host, Stage::Prologue), kCompleted);
  EXPECT_EQ(host.regs[kSizeReg], kArchMaxMopsBlockSize);
  // only Prologue, no work done
  EXPECT_TRUE(mem_correct(host, 0));
}

////////////////
// Interrupts //
////////////////

TEST(SetExecute, MainYieldsAfterEachBlock) {
  DummyHost host{make_host(100)};
  // Execute checks for yield every set_block_bytes bytes
  host.config.set_block_bytes = 16;
  host.yield = true;

  EXPECT_EQ(run(host, Stage::Prologue), kCompleted);
  EXPECT_EQ(run(host, Stage::Main), kInterrupted);

  // Registers hold progress
  EXPECT_EQ(host.regs[kDestReg], kDest + 16);
  EXPECT_EQ(host.regs[kSizeReg], 84);

  for (int block{0}; block < 5; block++) {
    EXPECT_EQ(run(host, Stage::Main), kInterrupted);
  }
  EXPECT_EQ(host.regs[kDestReg], kDest + 96);
  EXPECT_EQ(host.regs[kSizeReg], 4);

  // Nothing left to yield
  EXPECT_EQ(run(host, Stage::Main), kCompleted);
  EXPECT_EQ(run(host, Stage::Epilogue), kCompleted);
  EXPECT_TRUE(mem_correct(host, 100));
}

////////////
// Faults //
////////////

TEST(SetExecute, FaultStopsAtFaultingByte) {
  // Fault in a single larger block or a later smaller block
  for (const std::int64_t block : {16, 4096}) {
    SCOPED_TRACE(block);
    DummyHost host{make_host(100)};
    host.config.set_block_bytes = block;
    const std::uint64_t fault_offset{50};
    host.fault_address = kDest + fault_offset;

    EXPECT_EQ(run(host, Stage::Prologue), kCompleted);

    mops::ExecuteResult expected_fault{mops::Fault{.address = kDest + fault_offset, .is_write = true}};
    EXPECT_EQ(run(host, Stage::Main), expected_fault);

    // Registers hold progress up to fault
    EXPECT_EQ(host.regs[kDestReg], kDest + fault_offset);
    EXPECT_EQ(host.regs[kSizeReg], 100 - fault_offset);
    EXPECT_TRUE(mem_correct(host, fault_offset));

    // Assume that OS fixes the fault and reruns SETM*
    host.fault_address = ULLONG_MAX;
    EXPECT_EQ(run(host, Stage::Main), kCompleted);
    EXPECT_EQ(run(host, Stage::Epilogue), kCompleted);
    EXPECT_TRUE(mem_correct(host, 100));
  }
}

// Fault in non zero Prologue, doesn't touch PSTATE.[N,Z,C,V]
TEST(SetExecute, PrologueFaultLeavesNzcv) {
  DummyHost host{make_host(100)};
  host.config.set_prologue_bytes = 32;
  const std::uint32_t nzcv{0b0101};
  host.nzcv = nzcv;
  const std::uint64_t fault_offset{10};
  host.fault_address = kDest + fault_offset;

  mops::ExecuteResult expected_fault{mops::Fault{.address = kDest + fault_offset, .is_write = true}};
  EXPECT_EQ(run(host, Stage::Prologue), expected_fault);

  // PSTATE.[N,Z,C,V] unchanged
  EXPECT_EQ(host.nzcv, nzcv);
  EXPECT_EQ(host.regs[kDestReg], kDest + fault_offset);
  EXPECT_EQ(host.regs[kSizeReg], 100 - fault_offset);
}

////////////////////
// MopsExceptions //
////////////////////

TEST(SetExecute, OptionAFormatRaisesMismatch) {
  DummyHost host{make_host(static_cast<std::uint64_t>(-10))};
  // Registers in Option A format
  host.nzcv = kOptionA;

  // Mismatched core types
  // Expected iss has formatoption = 0b10
  mops::ExecuteResult expected_fault{mops::MopsException{.iss = 0x01020041}};
  EXPECT_EQ(run(host, Stage::Main), expected_fault);

  // Mismatched core types
  // Independent of stage and options
  host.config.set_epilogue_bytes = 16;
  const mops::AccessAttributes attr{.unprivileged = true, .non_temporal = false};
  // Expected iss has formatoption = 0b10, options = 0b01
  expected_fault = mops::MopsException{.iss = 0x010E0041};
  EXPECT_EQ(run(host, Stage::Epilogue, attr), expected_fault);
  // No memory touched
  EXPECT_EQ(host.regs[kSizeReg], static_cast<std::uint64_t>(-10));
  EXPECT_TRUE(mem_correct(host, 0));
}

TEST(SetExecute, EpilogueWithWrongSizeRaisesException) {
  DummyHost host{make_host(5)};
  host.nzcv = kOptionB;

  // Epilogue size 0 but 5 bytes left
  // Expected iss has formatoption = 0b00
  mops::ExecuteResult expected_fault{mops::MopsException{.iss = 0x01040041}};
  EXPECT_EQ(run(host, Stage::Epilogue), expected_fault);
  // No memory touched
  EXPECT_TRUE(mem_correct(host, 0));
}

TEST(SetExecute, EpilogueWithWrongSizeAndOptionIsMismatch) {
  DummyHost host{make_host(static_cast<std::uint64_t>(-10))};
  host.nzcv = kOptionA;

  // Epilogue size 0 but 10 bytes left and wrong option
  // Expected iss has formatoption = 0b10
  mops::ExecuteResult expected_fault{mops::MopsException{.iss = 0x01060041}};
  EXPECT_EQ(run(host, Stage::Epilogue), expected_fault);
  // No memory touched
  EXPECT_EQ(host.regs[kSizeReg], static_cast<std::uint64_t>(-10));
  EXPECT_EQ(host.regs[kDestReg], kDest);
  EXPECT_TRUE(mem_correct(host, 0));
}

TEST(SetExecute, SyndromeHoldsFiveBitRegisters) {
  DummyHost host;
  host.nzcv = kOptionA;
  host.regs.at(17) = 10;

  // Mismatched core types
  const mops::SetInstruction setm{.stage = Stage::Main, .rd = 16, .rs = 18, .rn = 17, .write = {}};

  // Expected iss has formatoption = 0b10 and correct registers
  mops::ExecuteResult expected_fault{mops::MopsException{.iss = 0x01024251}};
  EXPECT_EQ(mops::execute(host, setm), expected_fault);
}

TEST(SetExecute, ZeroSizeSkipsCheck) {
  // Dont check option when size = 0
  DummyHost host{make_host(0)};
  host.nzcv = kOptionA;

  EXPECT_EQ(run(host, Stage::Main), kCompleted);
  EXPECT_EQ(run(host, Stage::Epilogue), kCompleted);
}

///////////////////////////////
// CONSTRAINED UNPREDICTABLE //
///////////////////////////////

TEST(SetExecute, ConstrainedUnpredictableIsUndefined) {
  const std::array<mops::SetInstruction, 5> cases{
      mops::SetInstruction{.stage = Stage::Main, .rd = 2, .rs = 1, .rn = 1, .write = {}},  // Xs == Xn
      mops::SetInstruction{.stage = Stage::Main, .rd = 1, .rs = 1, .rn = 2, .write = {}},  // Xs == Xd
      mops::SetInstruction{.stage = Stage::Main, .rd = 1, .rs = 2, .rn = 1, .write = {}},  // Xn == Xd
      mops::SetInstruction{.stage = Stage::Main, .rd = 31, .rs = 1, .rn = 2, .write = {}}, // Xd == 31
      mops::SetInstruction{.stage = Stage::Main, .rd = 1, .rs = 2, .rn = 31, .write = {}}, // Xn == 31
  };
  DummyHost host{make_host(10)};
  host.nzcv = kOptionB;
  // All constrained unpredictable cases return Undefined
  for (const mops::SetInstruction& instr : cases) {
    SCOPED_TRACE(testing::PrintToString(instr));
    EXPECT_EQ(mops::execute(host, instr), kUndefined);
  }

  // No registers or memory touched
  EXPECT_EQ(host.regs[kSizeReg], 10U);
  EXPECT_EQ(host.nzcv, kOptionB);
  EXPECT_TRUE(mem_correct(host, 0));
}

//////////////////////////
// Direct memory access //
//////////////////////////

TEST(SetExecute, DirectMemoryAccess) {
  DummyHost host{make_host(100)};
  host.direct_mem = true;

  for (const Stage stage : {Stage::Prologue, Stage::Main, Stage::Epilogue}) {
    EXPECT_EQ(run(host, stage), kCompleted);
  }
  EXPECT_TRUE(mem_correct(host, 100));
  // Everything went through host_ptr()
  EXPECT_EQ(host.write_calls, 0);
}

TEST(SetExecute, DirectMemoryFallsBackOnFault) {
  DummyHost host{make_host(100)};
  host.direct_mem = true;
  const std::uint64_t fault_offset{50};
  host.fault_address = kDest + fault_offset;

  EXPECT_EQ(run(host, Stage::Prologue), kCompleted);

  // Host refuses ptr, due to fault, write_mem is used instead
  const mops::ExecuteResult expected_fault{mops::Fault{.address = kDest + fault_offset, .is_write = true}};
  EXPECT_EQ(run(host, Stage::Main), expected_fault);

  EXPECT_GT(host.write_calls, 0);
  EXPECT_EQ(host.regs[kDestReg], kDest + fault_offset);
  EXPECT_TRUE(mem_correct(host, fault_offset));
}

////////////////
// OoB Config //
////////////////

TEST(SetExecute, OutOfBoundsStageSize) {
  DummyHost host{make_host(100)};
  host.config.set_epilogue_bytes = -10;
  host.config.set_prologue_bytes = -10;

  // Stagesizes get clamped to 0, all work done in Main
  for (const Stage stage : {Stage::Prologue, Stage::Main, Stage::Epilogue}) {
    EXPECT_EQ(run(host, stage), kCompleted);
  }
  EXPECT_TRUE(mem_correct(host, 100));
}

TEST(SetExecute, OutOfBoundsBlockSize) {
  DummyHost host{make_host(100)};
  host.config.set_block_bytes = -10;

  // Block size clamped to 1
  for (const Stage stage : {Stage::Prologue, Stage::Main, Stage::Epilogue}) {
    EXPECT_EQ(run(host, stage), kCompleted);
  }
  EXPECT_TRUE(mem_correct(host, 100));
}