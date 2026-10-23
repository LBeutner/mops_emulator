import sys, re, subprocess, struct

if len(sys.argv) != 2:
  sys.exit('missing instruction: asm2word.py "<instruction>"')

out = subprocess.run(["llvm-mc-18", "-triple=aarch64", "-mattr=+mops,+mte", "--show-encoding"], input=sys.argv[1], stdout=subprocess.PIPE, text=True).stdout

m = re.search(r"encoding: \[(.*)\]", out)
if not m:
  sys.exit(1)

raw = bytes(int(b, 16) for b in m[1].split(","))
word = struct.unpack("<I", raw)[0]
print(f"{word:#010x}")