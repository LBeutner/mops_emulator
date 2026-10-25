#include "mops/decode.hpp"
#include "mops/detail/constrained_unpredictable.hpp"
#include "support/printers.hpp"
#include <cstdint>
#include <gtest/gtest.h>

// Constrained-unpredictable register combinations (K1.2.17.2, K1.2.17.17) are not rejected by decode,
// they are handled by execute. LLVM refuses to assemble them, so they are not part of the
// golden file.

namespace {

using mops::AccessAttributes;
using mops::CpyFamily;
using mops::Stage;

constexpr AccessAttributes kPlain{.unprivileged = false, .non_temporal = false};
constexpr AccessAttributes kUnpriv{.unprivileged = true, .non_temporal = false};
constexpr AccessAttributes kNonTemporal{.unprivileged = false, .non_temporal = true};
constexpr AccessAttributes kUnprivNonTemporal{.unprivileged = true, .non_temporal = true};

mops::DecodeResult set_insn(Stage stage, std::uint8_t rd, std::uint8_t rs, std::uint8_t rn, AccessAttributes write) {
  return mops::SetInstruction{.stage = stage, .rd = rd, .rs = rs, .rn = rn, .write = write};
}

mops::DecodeResult cpy_insn(CpyFamily family, Stage stage, std::uint8_t rd, std::uint8_t rs, std::uint8_t rn,
                            AccessAttributes read, AccessAttributes write) {
  return mops::CpyInstruction{
      .family = family, .stage = stage, .rd = rd, .rs = rs, .rn = rn, .read = read, .write = write};
}

} // namespace

TEST(DecodeConstrainedUnpredictable, SetKeepsRegistersAndAttributes) {
  // setp [x1]!, x1!, x2 with Xd == Xn
  EXPECT_EQ(mops::decode(0x19c20421), set_insn(Stage::Prologue, 1, 2, 1, kPlain));
  // setmt [x0]!, x2!, x2 with Xs == Xn
  EXPECT_EQ(mops::decode(0x19c25440), set_insn(Stage::Main, 0, 2, 2, kUnpriv));
  // seten [x2]!, x1!, x2 with Xs == Xd
  EXPECT_EQ(mops::decode(0x19c2a422), set_insn(Stage::Epilogue, 2, 2, 1, kNonTemporal));
  // setptn [xzr]!, x1!, x2 with Xd == 31
  EXPECT_EQ(mops::decode(0x19c2343f), set_insn(Stage::Prologue, 31, 2, 1, kUnprivNonTemporal));
  // setm [x0]!, xzr!, x2 with Xn == 31
  EXPECT_EQ(mops::decode(0x19c247e0), set_insn(Stage::Main, 0, 2, 31, kPlain));
}

TEST(DecodeConstrainedUnpredictable, CpyKeepsRegistersAndAttributes) {
  // cpyfp [x0]!, [x0]!, x2! with Xd == Xs
  EXPECT_EQ(mops::decode(0x19000440), cpy_insn(CpyFamily::CpyF, Stage::Prologue, 0, 0, 2, kPlain, kPlain));
  // cpyfmwt [x0]!, [x1]!, x1! with Xs == Xn
  EXPECT_EQ(mops::decode(0x19411420), cpy_insn(CpyFamily::CpyF, Stage::Main, 0, 1, 1, kPlain, kUnpriv));
  // cpyern [x0]!, [x1]!, x0! with Xn == Xd
  EXPECT_EQ(mops::decode(0x1d818400), cpy_insn(CpyFamily::Cpy, Stage::Epilogue, 0, 1, 0, kNonTemporal, kPlain));
  // cpyprtwn [xzr]!, [x1]!, x2! with Xd == 31
  EXPECT_EQ(mops::decode(0x1d01645f), cpy_insn(CpyFamily::Cpy, Stage::Prologue, 31, 1, 2, kUnpriv, kNonTemporal));
  // cpymtn [x0]!, [xzr]!, x2! with Xs == 31
  EXPECT_EQ(mops::decode(0x1d5ff440),
            cpy_insn(CpyFamily::Cpy, Stage::Main, 0, 31, 2, kUnprivNonTemporal, kUnprivNonTemporal));
  // cpyfe [x0]!, [x1]!, xzr! with Xn == 31
  EXPECT_EQ(mops::decode(0x198107e0), cpy_insn(CpyFamily::CpyF, Stage::Epilogue, 0, 1, 31, kPlain, kPlain));
}

// XZR as Xs is valid for SET, unlike CPY
static_assert(!mops::detail::set_constrained_unpredictable(0, 31, 2));
static_assert(mops::detail::cpy_constrained_unpredictable(0, 31, 2));