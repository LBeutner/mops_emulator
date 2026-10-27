# MOPS emulator

A header-only C++20 library that emulates the Arm FEAT_MOPS instructions, plus a small demo that shows how to use it in an existing emulator.

The library receives a 32 bit instruction word. It decodes it and runs it on the emulator's registers and memory.
The library has no CPU state, memory or PC of its own, the emulator provides those through a small interface.

```cpp
#include "mops/mops.hpp"

mops::StepResult result{mops::step(host, word)}; // decode + execute one instruction
```

## Build and run

### Docker (recommended)

The dev container has the exact toolchain that CI uses (LLVM 18, CMake, Ninja). In VS Code use "Reopen in Container", or:

```bash
docker build -t mops-dev .devcontainer
docker run --rm -it -u "$(id -u):$(id -g)" -v "$PWD":/workspace mops-dev bash
```

Inside the container:

```bash
# build and test
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
# same with sanitizers
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
# formatting and clang-tidy
cmake --build --preset format-check && cmake --build --preset tidy
# run the demo
./build/debug/demo examples/example.bin
```

### Native

You need `CMake 3.25+`, `Ninja` and `clang++ 18` with `libstdc++ 13`. The asan preset also needs `libclang-rt-18-dev`, the lint presets need `clang-format-18` and `clang-tidy-18`.
GoogleTest is downloaded on the first configure.
After that, the commands are the same as above.
`tools/` has three Python 3 scripts that use `llvm-mc-18`.

### Demo

The demo runs a flat binary of AArch64 instructions.
Besides SET* it understands MOVZ, so a program can load its own
registers, see `examples/example.s`.
To build the binary:

```bash
python3 tools/asm2program.py my_program.s my_program.bin
./build/debug/demo my_program.bin
```

The registers and flags that change are printed after each instruction and the memory at the end.
The interrupt and the `ImplDefined` choices are set in `main`.

## Scope

Each MOPS operation is split into three instructions (prologue, main, epilogue). I went for one complete and well
tested slice, instead of a bit of everything:

- **Decode for every MOPS instruction** (CPY*, CPYF*, SET*, all variants), checked against LLVM.
- **SET\* execution**, including the unprivileged and non-temporal variants. SET* is the simplest operation, but it has everything that makes MOPS interesting: the three stages, registers that allow resuming after an interruption, faults, interrupts and the Memory Set exception.
- **`step()`**, which goes from an instruction word all the way to the result in memory.
- **A demo emulator** that uses the library through a small adapter.

Left for later. These already decode and return `Unsupported`:

- **CPYF\*** would be the next step. It reuses the SET structure and adds reading memory, roughly a day with tests.
- **CPY\*** builds on CPYF and adds copying backwards for overlapping buffers, which is where most of the tricky cases are.
- **SETG\*** needs memory tagging (MTE), which is a feature of its own.
- **Option A:** one format is enough for emulation. If SETM or SETE get registers in Option A format, the library still notices and raises the Memory Set exception, like a real Option B core would.

## Design

**Header-only, templates instead of virtual interfaces.** The library calls into the emulator for every register
access and memory chunk, templates let these calls be inlined instead of going through a vtable. The emulator's types also don't have to inherit from anything, and missing members show up as a concept error at compile time.

**Results.** decode, execute and step return the same result types: `Completed`, `Interrupted`, `Fault{address, is_write}`, `MopsException{iss}`, `Undefined`, `Unsupported` and `NotMops`. Some of them can come from more than one stage (`Undefined` from decode for a nonexistent encoding, from execute for invalid register combinations), so `step` just passes them on and the emulator only handles one set of results.

**Implementation defined choices.** The Arm spec leaves many details to the hardware vendor.
Since the goal is running code on hosts without FEAT_MOPS, and correct software doesn't depend on these details, most of them are fixed.
Only two things are configurable through `ImplDefined`: how much work the prologue and epilogue do (mostly for testing), and the block size, which decides how often the Main stage can be interrupted.
Fixed: Option B, no option check when there is nothing left to do, but checked even when the current stage has nothing to do (`MemStageSetZeroSizeCheck() = true`), parameters are never rejected as ill-formed, and constrained unpredictable register combinations are UNDEFINED instead of NOP.

**Memory writes.** The library writes in chunks up to 4096 bytes and only retries failing chunks byte by byte, to reduce overhead. Faults return the exact byte, just like the Arm pseudocode.

## Integrating

Add the project with `add_subdirectory` and link `mops::mops`.

The emulator passes a type that satisfies `mops::Host`.
How that type is implemented is up to the emulator, `examples/demo.cpp` shows one way with a small adapter around existing CPU and memory types.

Required members:
```cpp
std::uint64_t read_reg(std::uint8_t reg); // X0..X30, XZR handled by the lib
void write_reg(std::uint8_t reg, std::uint64_t val);
std::uint32_t read_nzcv(); // PSTATE.{N,Z,C,V} in bits [3:0]
void write_nzcv(std::uint32_t flags);
bool read_mem(std::uint64_t addr, std::span<std::byte> buf, mops::AccessAttributes attr); // false = fault
bool write_mem(std::uint64_t addr, std::span<const std::byte> buf, mops::AccessAttributes attr); // false = fault
```

Optional: `should_yield()` to interrupt the Main stage, `impl_defined()` for the configurable choices and `host_ptr()` for direct memory access.
`host_ptr()` must return `nullptr` unless the whole range is accessible.

The library never touches the PC or takes exceptions. Depending on the result, the emulator advances the PC (`Completed`), runs the instruction again later (`Interrupted`), raises the matching exception (`Fault`, `MopsException`, `Undefined`) or hands the word to other decoders (`NotMops`) or stops/logs on (`Unsupported`).
Checking whether MOPS is enabled at the current EL is also left to the emulator.

## Testing

All tests use GoogleTest and run in CI for the debug, asan and release builds.

- **Decode**: compared against LLVM-18. The `gen_decode_golden.py` script builds every combination of the opcode fields with a subset of registers, lets `llvm-mc-18` disassemble them and stores the expected result. Register combinations that LLVM refuses to assemble are tested separately. Negative tests cover SETG*, invalid size bits and non-MOPS instructions.
- **Execute**: a sweep over size, prologue size, epilogue size and block size checks that the result is always a correct memset and that the registers are right after every stage. Further tests cover interrupts and resuming, faults and restarting, the MOPS exception values and invalid register combinations. The test host fills its memory with a fixed value and throws when the xzr register is touched.
- **Step**: a SETP/SETM/SETE sequence encoded by llvm-mc runs through `step`, and every result from decode and execute comes out unchanged.

## Limitations

- Execute is only tested against my own understanding of the Arm spec and uses no external reference.
- Implementation choices that depend on the address can't be expressed.
- When a chunk write fails, the byte by byte retry writes some bytes twice. This might cause problems for memory with side effects.

## With more time

- CPYF*, then CPY*, reusing the SET structure.
- Function callbacks in `ImplDefined` for implementation choices that depend on the address.