import sys, subprocess, tempfile
from pathlib import Path

if len(sys.argv) != 3:
  sys.exit('usage: asm2program.py "input.s" "output.bin"')

source, output = sys.argv[1], sys.argv[2]

with tempfile.TemporaryDirectory() as temp:
  obj = Path(temp) / "program.o"
  subprocess.run(["clang", "--target=aarch64-none-elf", "-march=armv8-a+mops", "-c", source, "-o", str(obj)], check=True)
  subprocess.run(["llvm-objcopy", "-O", "binary", "-j", ".text", str(obj), output], check=True)