# Build Flags Reference

This page is the single source of truth for the compiler, linker, and FAUST flags used to build Stratus and Nimbus effect binaries. Use it when you build outside the provided Docker container, when you script your own toolchain, or when you need to know *why* a flag exists before removing it. If you just want to build the examples, follow [build-docker.md](build-docker.md) — the repo's CMake already applies everything on this page.

Both devices use the same TI AM335x Cortex-A8 CPU and the same firmware, so one flag set covers both: a `.so` built with these flags runs identically on Stratus and Nimbus.

## The canonical flag line

This is what the repo's CMake applies to every example (`examples/CMakeLists.txt`, line 7):

```
-fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only
```

A complete known-good manual invocation (run inside the build container, or on the device itself):

```bash
g++ -std=c++14 -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only -I resources <YOUR-EFFECT>.cpp -o <EFFECT-ID>.so
```

- `<YOUR-EFFECT>.cpp` — your effect source file (it must `#include "dsp.hpp"` from `resources/`; see [dsp-contract.md](dsp-contract.md)).
- `<EFFECT-ID>` — the GUID the platform assigns your effect (the name the firmware loads it by); see [Output file naming](#output-file-naming-effect-idso) below.

**Checkpoint:** you have a `.so` with no `lib` prefix, named `<EFFECT-ID>.so`.

## Per-flag rationale

| Flag | Why it is there |
|---|---|
| `-fPIC` | Position-independent code — required for any shared object the firmware loads with `dlopen()`. |
| `-shared` | Produce a shared object (`ET_DYN` ELF). The firmware only loads shared objects, and the upload pipeline rejects anything else. |
| `-O3` | Full optimization. The whole effect chain shares one 2.9 ms audio callback on a single 1 GHz core; unoptimized builds waste that budget. |
| `-mcpu=cortex-a8` | Generate code for the exact CPU in Stratus and Nimbus (TI AM335x, ARM Cortex-A8). Selects the ARMv7-A architecture *and* Cortex-A8 pipeline scheduling in one flag. |
| `-mtune=cortex-a8` | Schedule instructions for the Cortex-A8 pipeline. Redundant next to `-mcpu=cortex-a8` (which implies it) but kept so the flag line also works verbatim with the older `-march=armv7-a` spelling (see below). |
| `-mfloat-abi=hard` | Hard-float ABI: float arguments pass in FPU registers. The device userspace is 32-bit **armhf**; a soft-float binary is ABI-incompatible and will not load. |
| `-mfpu=neon` | Enable the NEON SIMD unit. This is where most of the Cortex-A8's float throughput lives. |
| `-ftree-vectorize` | Let GCC auto-vectorize loops onto NEON. |
| `-ffast-math` | Relaxed IEEE semantics. Required for effective NEON float vectorization (NEON on Cortex-A8 is not fully IEEE-compliant, so GCC will not vectorize float math without it) and it flushes away much denormal handling. **Never use it without `-fno-finite-math-only` — see the warning below.** |
| `-fprefetch-loop-arrays` | Emit prefetch instructions for arrays traversed in loops — helps the A8's simple memory pipeline. |
| `-funroll-loops` | Unroll loops; wins on the A8's in-order pipeline for small DSP kernels. |
| `-funsafe-loop-optimizations` | Assume loops terminate and induction variables don't overflow; unlocks further loop optimization. |
| `-fno-finite-math-only` | **Load-bearing.** Walks back the one part of `-ffast-math` that breaks loading on the device. See the warning below. |

> **Warning:** `-ffast-math` without `-fno-finite-math-only` produces a binary that silently fails to load.
>
> `-ffast-math` implies `-ffinite-math-only`, which makes GCC emit calls to `__expf_finite`, `__logf_finite`, and other `*_finite` libm entry points. The device's libm does not export those symbols. The firmware loads effects with `dlopen(RTLD_NOW)`, which resolves *every* undefined symbol at load time — one unresolvable `*_finite` reference and the load fails.
>
> **How this failure presents:** your effect simply never appears. No crash, no error in the app — the firmware drops the effect from the chain and moves on. The only evidence is a `dlerror` line in the device log:
>
> ```bash
> journalctl -f -u bela_startup.service
> ```
>
> ```text
> ... undefined symbol: __expf_finite
> Effect does not exist.
> ```
>
> Always pair the flags: `-ffast-math -fno-finite-math-only`. This was verified on hardware; it is the single most common "my effect won't load" cause. See [troubleshooting.md](troubleshooting.md).

You can catch this before ever deploying. From inside the build container:

```bash
nm -D --undefined-only <YOUR-EFFECT>.so | grep -i finite
```

Expected output: nothing. Any `__*_finite` line means `-fno-finite-math-only` is missing from your build.

**Checkpoint:** the grep above prints nothing.

### `-march=armv7-a` vs `-mcpu=cortex-a8`

Older documentation — the root `README.md` on-device compile command, `resources/compilation_flags.txt`, and the server-side compile pipeline — spells the target as:

```
-march=armv7-a -mtune=cortex-a8
```

The repo's CMake spells it:

```
-mcpu=cortex-a8 -mtune=cortex-a8
```

These are equivalent for this device: `-mcpu=cortex-a8` selects the ARMv7-A architecture *and* Cortex-A8 tuning, which is exactly what `-march=armv7-a -mtune=cortex-a8` requests explicitly. Binaries built either way are compatible. Use whichever spelling you prefer, but do not mix in a different `-march`/`-mcpu` value — the only CPU in the field is the Cortex-A8.

You may also see `-g` in the older command lines (`resources/compilation_flags.txt`). It embeds debug info and is harmless — the linker flags below strip it from release builds anyway.

## Linker flags

The repo's CMake applies these to shared-library links (`examples/CMakeLists.txt`, line 8):

```
-Wl,-Ofast -Wl,--as-needed -Wl,--strip-debug
```

| Flag | Why it is there |
|---|---|
| `-Wl,-Ofast` | Asks the linker for optimized output (hash-table tuning etc.). Not performance-critical; kept for parity with the shipped examples. |
| `-Wl,--as-needed` | Drop `DT_NEEDED` entries for libraries you don't actually call. Fewer dependencies means fewer things `dlopen(RTLD_NOW)` has to resolve on the device — fewer ways to fail at load. |
| `-Wl,--strip-debug` | Remove debug sections. Smaller `.so` (uploads are capped at 5 MB) and faster load. |

If your effect uses libm functions directly (hand-written C/C++ calling `expf`, `powf`, …), add `-lm` at the end of the link line. The server-side native pipeline does exactly this.

## FAUST production flag set

> This section is for **out-of-band builders** — people compiling FAUST source to a `.so` themselves instead of letting the platform do it. If you write FAUST and submit source, the backend compiles it for you with exactly these flags and you can skip this section. For writing the FAUST itself, see [guide-faust.md](guide-faust.md).

The production pipeline uses **FAUST 2.83.x** with the `stratus.cpp` architecture:

```
faust -a stratus.cpp -lang cpp -i -light -scn FaustDSP -single -ftz 0 -nvi -exp10 -inpl -ct 1 -es 1 -mcd 0 -mdd 1024 -mdy 33 <YOUR-EFFECT>.dsp
```

- `<YOUR-EFFECT>.dsp` — your FAUST source file.

The generated C++ is then compiled with the exact g++ flag line from the top of this page (production uses the `-march=armv7-a -mtune=cortex-a8` spelling — equivalent, as noted above).

| FAUST flag | What it does here |
|---|---|
| `-a stratus.cpp` | The Stratus architecture file: wraps the generated DSP in the class and `extern "C"` exports the firmware expects. |
| `-lang cpp` | Generate C++. |
| `-i` | Inline architecture `#include`s into one self-contained output file. |
| `-light` | Generate only the DSP API actually used — smaller, simpler output. |
| `-scn FaustDSP` | Name the generated superclass `FaustDSP`, which is what `stratus.cpp` expects to wrap. |
| `-single` | Single-precision float everywhere. The device processes float32 audio; doubles are dramatically slower on the A8. |
| `-ftz 0` | No software flush-to-zero code — the firmware already sets hardware FTZ/DN flags on the audio thread, so software FTZ would be pure overhead. |
| `-nvi` | Non-virtual DSP methods in the generated class (the architecture wrapper provides the virtual ABI surface itself). |
| `-exp10` | Use the `exp10` primitive rather than `pow(10, x)`. |
| `-inpl` | Generate `compute()` that is safe when the input and output buffers alias — the host's buffers ARE in-place (`in == out`), so this is mandatory. |
| `-ct 1` | Range-check `rdtable`/`rwtable` indices — an out-of-range table read on the audio thread is otherwise a crash. |
| `-es 1` | Enable `enable()` primitive semantics. |
| `-mcd 0` | Max-copy-delay 0: all delay lines use ring buffers instead of buffer-copy delays. Part of why the generated code is safe under full-buffer `compute()` calls. |
| `-mdd 1024` | Delay-line code-generation threshold (max dense delay), part of the tested production configuration. |
| `-mdy 33` | Delay-line code-generation threshold (min density), part of the tested production configuration. |

> **Gotcha:** the FAUST version matters. Debian bullseye's apt ships FAUST 2.30.5, which does not support `-ct`, `-es`, `-mdd`, or `-mdy` — the production toolchain builds FAUST 2.83.x from source for this reason. If your FAUST rejects one of these flags, your FAUST is too old to reproduce the production build.

## Toolchain compatibility floor (glibc / libstdc++)

Production devices ship a runtime floor of **glibc 2.31 and a GCC 10.3-era libstdc++** (the firmware itself enforces this floor on-device). Your `.so` must not require anything newer.

This is why the provided Docker container ([build-docker.md](build-docker.md)) is Debian **bullseye**: bullseye's glibc is 2.31 and its GCC is 10.x, so anything you build in it is compatible with current production units by construction. Build in the container and you never think about this again.

Build on a random newer distro instead, and your binary can pick up versioned symbol requirements like `GLIBC_2.34` that the device cannot satisfy — and the failure looks exactly like the fast-math trap above: `dlopen(RTLD_NOW)` fails, the effect silently never appears, and the only clue is a `dlerror` in `journalctl -f -u bela_startup.service`.

Pre-flight checks from your build machine. First, list the shared libraries your `.so` demands at load time:

```bash
readelf -d <YOUR-EFFECT>.so | grep NEEDED
```

Expected output — these libraries (plus the dynamic loader) and nothing else:

```text
 0x00000001 (NEEDED)                     Shared library: [libm.so.6]
 0x00000001 (NEEDED)                     Shared library: [libstdc++.so.6]
 0x00000001 (NEEDED)                     Shared library: [libgcc_s.so.1]
 0x00000001 (NEEDED)                     Shared library: [libc.so.6]
```

Anything else in that list — `libfftw3` from a `-lfftw3` you added, `libgomp` pulled in by `-fopenmp`, an extra library a JUCE or CMake module linked for you — must be statically linked into your `.so` or removed. The device does not ship it, so `dlopen(RTLD_NOW)` fails and the effect silently never appears. Never add `-fopenmp` or link runtimes that spawn threads or use heavy thread-local storage: besides the missing-library trap, a static-TLS-heavy library can fail to load with `cannot allocate memory in static TLS block`.

Second, list every versioned symbol requirement — glibc *and* libstdc++:

```bash
objdump -T <YOUR-EFFECT>.so | grep -oE 'GLIBC_2\.[0-9]+|GLIBCXX_[0-9.]+|CXXABI_[0-9.]+' | sort -Vu
```

Expected output: nothing above `GLIBC_2.31`, `GLIBCXX_3.4.28`, or `CXXABI_1.3.12` — the bullseye ceilings. Any higher version means you built with a toolchain newer than the device runtime, and the `.so` will fail to load even if the fast-math grep above passes. (Checking only `GLIBC_*` is not enough: a newer g++ can pass a glibc-only grep and still require `GLIBCXX`/`CXXABI` versions the device's GCC 10.3-era libstdc++ lacks.)

And confirm the architecture is right:

```bash
file <YOUR-EFFECT>.so
```

```text
<YOUR-EFFECT>.so: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

**Checkpoint:** `file` reports a 32-bit ARM shared object, `readelf` shows no unexpected `NEEDED` libraries, and no versioned symbol exceeds the bullseye ceilings.

> **Gotcha:** JUCE-based effects historically required a libstdc++ upgrade on older pedals (see `examples/juce_effect` on the public repo's main branch). Current firmware units ship a newer runtime, but treat JUCE as an advanced, at-your-own-risk path — see [guide-native-cpp.md](guide-native-cpp.md).

## Output file naming: `<EFFECT-ID>.so`

The firmware loads effects strictly by filename from `/opt/update/sftp/firmware/effects/`:

```
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
```

- `<EFFECT-ID>` — a GUID. The file must be named **exactly** `<EFFECT-ID>.so` — no `lib` prefix, no version suffix, nothing else. (`resources/compilation_flags.txt` hints at this with its `"Effect ID".so` output name; the repo's CMake strips the `lib` prefix for the same reason.)

Where the GUID comes from:

- **Hardware testing:** the platform assigns the Effect ID when you create the effect in the FX Builder — you never invent it. Upload your `.so` there and **publish privately**; the platform hosts the binary as `<EFFECT-ID>.so`, and installing the effect from the Chaos Audio app places it on-device under that name — see [deploy-to-hardware.md](deploy-to-hardware.md). (Your local filename doesn't matter — the platform names the hosted copy.)
- **Tone Shop release:** the same Effect ID stays for the life of the effect; ship updates by submitting a new version through the FX Builder. See [release-and-submission.md](release-and-submission.md).

After installing your privately published effect from the Chaos Audio app, verify the load on the device (Stratus shown; Nimbus is identical):

```bash
journalctl -f -u bela_startup.service
```

Look for `Effect found` (good) or a `dlopen`/`dlerror` line (one of the two traps on this page). Full hardware-testing procedure and log walkthrough: [deploy-to-hardware.md](deploy-to-hardware.md); performance measurement: [verification.md](verification.md).

Next: [deploy-to-hardware.md](deploy-to-hardware.md)
