#pragma once
#include "mops/detail/constrained_unpredictable.hpp"
#include "mops/detail/execute_common.hpp"
#include "mops/detail/syndrome.hpp"
#include "mops/host_concepts.hpp"
#include <optional>

namespace mops::detail {

struct SetParams {
  Stage stage;
  std::byte data;
  std::uint32_t nzcv;
  std::uint64_t toaddress;
  std::int64_t setsize;
  std::int64_t stagesetsize;
  std::uint8_t rd, rs, rn;
};

template <Host H> SetParams read_set_params(H& host, const SetInstruction& instr) {
  SetParams params{};

  params.stage = instr.stage;

  // Xs = xrz is allowed, we check here
  const std::uint64_t raw_data{instr.rs == 31 ? 0 : host.read_reg(instr.rs)};
  params.data = static_cast<std::byte>(raw_data & 0xFF); // only take lower 8 bits

  const std::uint64_t raw_size{host.read_reg(instr.rn)};
  if (instr.stage == Stage::Prologue) {
    // In pseudocode, setsize is read as unsigned 64 bit integer, by capping it early with
    // kArchMaxMopsBlockSize = 2^63 - 1, it fits into a std::int64_t
    params.setsize = static_cast<std::int64_t>(std::min(raw_size, kArchMaxMopsBlockSize));
  } else {
    params.setsize = static_cast<std::int64_t>(raw_size);
  }

  params.nzcv = host.read_nzcv();
  params.toaddress = host.read_reg(instr.rd);
  params.stagesetsize = 0;
  params.rd = instr.rd;
  params.rs = instr.rs;
  params.rn = instr.rn;

  return params;
}

// SETPostSizeChoice()
// Ensure we do at most params.setsize amount of work
// In Main and Epilogue setsize is SInt(Xn) and can be negative
// The option check in check_set_params() runs after this, so the result needs to be well defined
constexpr std::int64_t set_post_size(const SetParams& params, const ImplDefined& cfg) {
  return clamp_to_size(cfg.set_epilogue_bytes, params.setsize);
}

// SETPreSizeChoice()
// Same as set_post_size()
constexpr std::int64_t set_pre_size(const SetParams& params, const ImplDefined& cfg) {
  return clamp_to_size(cfg.set_prologue_bytes, params.setsize);
}

// MemSetStageSize
// For all stages the architecture supports stage dependent sizes
// This library implements a fixed stage size
// set_pre_size and set_post_size ensure that we do at most setsize bytes of work
constexpr std::int64_t set_stage_size(const SetParams& params, const ImplDefined& cfg) {
  switch (params.stage) {
  case Stage::Prologue:
    return set_pre_size(params, cfg);
  case Stage::Main:
    return params.setsize - set_post_size(params, cfg);
  case Stage::Epilogue:
    return set_post_size(params, cfg);
  }
  return 0;
}

// CheckMemSetParams
inline std::optional<MopsException> check_set_params(const SetParams& params, const ImplDefined& cfg,
                                                     std::uint8_t options) {

  // Helper to construct MopsExceptions based on current params
  const auto exception = [&](bool mismatch) {
    MopsSyndrome synd{.options = options,
                      .from_epilogue = (params.stage == Stage::Epilogue),
                      .mismatch = mismatch,
                      .rd = params.rd,
                      .rs = params.rs,
                      .rn = params.rn};
    return MopsException{.iss = set_iss(synd)};
  };

  // wrong sizes or handed option A state
  if (params.stagesetsize != 0 && params.setsize != 0 && (params.nzcv & kPstateC) == 0) {
    return exception(true);
  }

  if (params.stage != Stage::Main && params.setsize != set_post_size(params, cfg)) {
    return exception(false);
  }

  return std::nullopt;
}

// main run loop
// Only valid for option B
template <Host H> LoopResult run_set_loop(H& host, SetParams& params, AccessAttributes attr, const ImplDefined& cfg) {
  while (params.stagesetsize > 0) {
    // Architecture supports dynamic block sizes
    // This library only defines an upper limit
    const std::int64_t block{clamp_bytes(cfg.set_block_bytes, params.stagesetsize)};

    // Write bytes
    const std::int64_t done{set_bytes(host, params.toaddress, params.data, block, attr)};

    // Write failed done bytes into the block
    if (done != block) {
      return LoopResult{.end = LoopEnd::Fault, .done = done};
    }

    params.toaddress += static_cast<std::uint64_t>(block);
    params.setsize -= block;
    params.stagesetsize -= block;

    // In case we are in Main, have work remaining and host receives interrupt
    if (params.stage == Stage::Main && params.stagesetsize > 0 && should_yield(host)) {
      return LoopResult{.end = LoopEnd::Interrupted};
    }
  }
  return LoopResult{};
}

// UpdateSetRegisters()
// Only valid for option B
// Only on faulty write (memory fault) is (done != 0)
template <Host H> void update_set_registers(H& host, const SetParams& params, std::int64_t done) {
  host.write_reg(params.rn, static_cast<std::uint64_t>(params.setsize - done));
  host.write_reg(params.rd, params.toaddress + static_cast<std::uint64_t>(done));
}

// Entry point for SET* instructions
template <Host H> ExecuteResult execute_set(H& host, const SetInstruction& instr) {
  const ImplDefined& cfg{get_impl_defined(host)};

  // CheckSETConstrainedUnpredictable
  if (set_constrained_unpredictable(instr.rd, instr.rs, instr.rn)) {
    return Undefined{};
  }

  SetParams params{read_set_params(host, instr)};

  if (instr.stage == Stage::Prologue) {
    // We only implement Option B
    params.nzcv = kNzcvOptionB;
  }

  params.stagesetsize = set_stage_size(params, cfg);

  if (params.stage != Stage::Prologue) {
    const auto exception{check_set_params(params, cfg, set_options(instr))};
    // MopsException
    if (exception) {
      return *exception;
    }
  }

  const LoopResult result{run_set_loop(host, params, instr.write, cfg)};
  update_set_registers(host, params, result.done);

  if (result.end == LoopEnd::Fault) {
    return Fault{.address = params.toaddress + static_cast<std::uint64_t>(result.done), .is_write = true};
  }
  if (result.end == LoopEnd::Interrupted) {
    return Interrupted{};
  }
  if (params.stage == Stage::Prologue) {
    host.write_nzcv(params.nzcv);
  }
  return Completed{};
}

} // namespace mops::detail
