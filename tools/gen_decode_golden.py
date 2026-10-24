"""
Generates tests/data/decode_golden.txt, the expected result of mops::decode

Uses LLVM disassembler as reference.
The only deviation is the constrained-unpredictable register rules (K1.2.17.17) and (K1.2.17.2),
that are checked at execute time. Due to LLVM rejecting some and accepting others they are left out
of the golden data and tested seperately in tests/decode/test_decode_cu.cpp

SETG* is not generated at all. It will be covered by the negative tests.

Output format:
  <word> ok <family> <stage> <rd> <rs> <rn> <r_unpriv> <r_nt> <w_unpriv> <w_nt>
  <word> undefined
"""

import itertools, struct, subprocess, re
from pathlib import Path
from typing import Any, Generator, List, Dict, Tuple, Optional

OUTPUT = Path(__file__).parent.parent / "tests/data/decode_golden.txt"

# "Memory Copy and Memory Set" encoding
MASK = 0b0011_1011_0010_0000_0000_1100_0000_0000
VALUE = 0b0001_1001_0000_0000_0000_0100_0000_0000

# Subset of registers to test
REGS = [0, 1, 2, 4, 8, 16, 31]

INS = re.compile(r"(cpy|cpyf|set)([pme])(t|wt|rt)?(n|wn|rn)?\s+(.*)")
REG = re.compile(r"x(\d+)|xzr") # matches any register, but doesn't capture xzr
LLVM_MC_ENC = re.compile(r"\s*(.*?)\s*\/\/ encoding: \[(.*)\]")

# <family> <stage> <rd> <rs> <rn> <r_unpriv> <r_nt> <w_unpriv> <w_nt>
Fields = Tuple[str, str, int, int, int, int, int, int, int]

# All opcode bit combinations in MOPS, registers from REG
def get_word() -> Generator[int, Any, None]:
  for size, o0, op1, op2 in itertools.product(range(4), range(2), range(4), range(16)):
    if op1 == 0b11 and o0 == 0b1:
      # SETG* tested separately
      continue
    for rs, rd, rn in itertools.product(REGS, repeat=3):
      word = (VALUE | (size << 30) | (o0 << 26) | (op1 << 22) | (rs << 16) | (op2 << 12) | (rn << 5) | rd)
      assert word & MASK == VALUE
      yield word

def word_to_hex_string(word: int) -> str:
  raw = struct.pack("<I", word)
  return " ".join(f"0x{byte:02x}" for byte in raw)

# Maps every word llvm-mc accepts to its assembly text
# Rejected words are absent in result
def disassemble(words: List[int]) -> Dict[int, str]:
  text = "\n".join(word_to_hex_string(word) for word in words)
  out = subprocess.run(["llvm-mc-18", "--disassemble", "--show-encoding", "-triple=aarch64", "-mattr=+mops"], input=text, capture_output=True, text=True).stdout
  
  result: Dict[int, str] = {}
  for line in out.splitlines():
    m = LLVM_MC_ENC.fullmatch(line)
    if m:
      raw = bytes(int(b, 16) for b in m[2].split(","))
      result[struct.unpack("<I", raw)[0]] = re.sub(r"\s+", " ", m[1])
  
  return result

def parse(word) -> Fields:
  m = INS.fullmatch(word)
  if not m:
    raise ValueError(f"unexpected llvm-mc output: {word}")

  family, stage, unpriv, nt, ops = m.groups()
  # findall returns "" for xzr, since the regex doesnt capture it
  regs = [int(r) if r != "" else 31 for r in REG.findall(ops)]

  if family.startswith("cpy"):
    # cpy* [Xd]!, [Xs]!, Xn! with read/write attributes
    rd, rs, rn = regs
    read = (unpriv in ("rt", "t"), nt in ("rn", "n"))
    write = (unpriv in ("wt", "t"), nt in ("wn", "n"))
  else:
    # set* [Xd]!, Xn!, Xs with only write attributes
    rd, rn, rs = regs
    read = (False, False)
    write = (unpriv == "t", nt == "n")

  r_unpriv, r_nt = read
  w_unpriv, w_nt = write
  return family, stage, rd, rs, rn, int(r_unpriv), int(r_nt), int(w_unpriv), int(w_nt)

# All cases in (K1.2.17.17) and (K1.2.17.2) are decided at execute time
# Not part of the golden file, tested seperately in tests/decode/test_decode_cu.cpp
def is_constrained_unpredictable(word: int) -> bool:
  rd, rn, rs = word & 0b11111, (word >> 5) & 0b11111, (word >> 16) & 0b11111
  is_set = (word >> 22) & 0b11 == 0b11
  if (rs == rn) or (rs == rd) or (rn == rd):
    return True
  return (rd == 31) or (rn == 31) or (not is_set and rs == 31)

def main() -> None:
  words = [word for word in get_word() if not is_constrained_unpredictable(word)]
  asms = disassemble(words)

  OUTPUT.parent.mkdir(parents=True, exist_ok=True)

  with OUTPUT.open("w") as f:
    for word in words:
      # LLVM rejects invalid register combinations and opcodes
      asm = asms.get(word)
      if asm is None:
        f.write(f"{word:#010x} undefined\n")
        continue

      f.write(f"{word:#010x} ok " + " ".join(map(str, parse(asm))) + "\n")

if __name__ == "__main__":
  main()