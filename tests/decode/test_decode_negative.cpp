#include "mops/decode.hpp"
#include "support/printers.hpp"
#include <gtest/gtest.h>

// Cases that the golden test doesn't cover, since LLVM only return invalid
// SETG* should always be Unsupported
// Anything not MOPS should return NotMops

namespace {

const mops::DecodeResult kUnsupported{mops::DecodeError::Unsupported};
const mops::DecodeResult kUndefined{mops::DecodeError::Undefined};
const mops::DecodeResult kNotMops{mops::DecodeError::NotMops};

TEST(DecodeSETG, ValidEncodingsAreUnsupported) {
  EXPECT_EQ(mops::decode(0x1dc20420), kUnsupported); // setgp [x0]!, x1!, x2
  EXPECT_EQ(mops::decode(0x1dc24420), kUnsupported); // setgm [x0]!, x1!, x2
  EXPECT_EQ(mops::decode(0x1dc28420), kUnsupported); // setge [x0]!, x1!, x2
  EXPECT_EQ(mops::decode(0x1dc21420), kUnsupported); // setgpt [x0]!, x1!, x2
  EXPECT_EQ(mops::decode(0x1dc22420), kUnsupported); // setgpn [x0]!, x1!, x2
  EXPECT_EQ(mops::decode(0x1dc23420), kUnsupported); // setgptn [x0]!, x1!, x2
}

TEST(DecodeSETG, InvalidEncodingsAreUnsupported) {
  EXPECT_EQ(mops::decode(0x1dc10420), kUnsupported); // setgp [x0]!, x1!, x1
  EXPECT_EQ(mops::decode(0x1dc2043f), kUnsupported); // setgp [xzr]!, x1!, x2
}

TEST(DecodeSETG, SizeIsUndefined) {
  EXPECT_EQ(mops::decode(0x5dc20420), kUndefined); // setgp [x0]!, x1!, x2 with sz=01
  EXPECT_EQ(mops::decode(0x9dc20420), kUndefined); // setgp [x0]!, x1!, x2 with sz=10
  EXPECT_EQ(mops::decode(0xddc20420), kUndefined); // setgp [x0]!, x1!, x2 with sz=11
}

TEST(DecodeNotMOPS, OtherInstructions) {
  EXPECT_EQ(mops::decode(0xd28000a0), kNotMops); // mov x0 #5
  EXPECT_EQ(mops::decode(0xd503201f), kNotMops); // nop
  EXPECT_EQ(mops::decode(0xf9400020), kNotMops); // ldr x0, [x1]
}

} // namespace