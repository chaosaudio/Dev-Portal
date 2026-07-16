# Repository Map & Guided Tour of the Examples

This page walks you through everything in the Dev-Portal repository: what each example effect teaches, how hard it is, which shared helpers live in `common/`, what each third-party submodule in `modules/` is for, and why the `featured/` effects are worth reading as production-grade code. Read it after [quickstart.md](quickstart.md), before you start writing your own effect. Everything here applies identically to **Stratus** (the pedal) and **Nimbus** (the 100 W smart amp) — one `.so`, both devices.

## The repository at a glance

```
Dev-Portal/
├── resources/        # dsp.hpp — THE authoritative ABI header — plus compile flags & UI art
├── examples/         # small teaching effects (this page's main subject)
│   ├── equalizer/    #   ← best first read
│   ├── pluto/
│   ├── spectrometer/
│   ├── juce_effect/  #   advanced: JUCE integration
│   ├── pitchshifter.cpp, dsp/, signalsmith-stretch.h   # vendored library + sketch
│   └── dsp.hpp       #   ⚠ legacy header — do not use
├── common/           # reusable DSP helpers: Biquad, Eq2Bands, Eq5Bands, ValueSmoother
├── modules/          # git submodules: RTNeural, FFTConvolver, r8brain, dr_libs
├── featured/         # production-grade effects: aida-x-convolver, aida-x-player, aida-x-tests
├── tests/            # benchmark-plugin dlopen harness (see verification.md)
├── Dockerfile        # armhf build container (see build-docker.md)
└── CMakeLists.txt
```

Before anything builds, fetch the submodules — the top-level `CMakeLists.txt` adds `featured/aida-x-convolver` and `featured/aida-x-tests` unconditionally, so a clone without submodules fails to configure:

```bash
git submodule update --init --recursive
```

Expected output (trimmed):

```
Submodule 'modules/FFTConvolver' (https://github.com/falkTX/FFTConvolver.git) registered for path 'modules/FFTConvolver'
Submodule 'modules/RTNeural' (https://github.com/MaxPayne86/RTNeural.git) registered for path 'modules/RTNeural'
...
Submodule path 'modules/r8brain': checked out '...'
```

**Checkpoint:** `modules/FFTConvolver`, `modules/RTNeural`, `modules/r8brain` and `modules/dr_libs` now contain source files instead of being empty directories.

> **Warning:** the repo contains TWO files named `dsp.hpp`. The only authoritative ABI header is [`resources/dsp.hpp`](../resources/dsp.hpp) (`DSP_VERSION "2.0.0"`, byte-identical to the firmware's copy). `examples/dsp.hpp` is a stale v1-era header with a different object layout — never include it in your own effect. The examples in subdirectories are safe: `examples/CMakeLists.txt` puts `../resources` on the include path first, so `#include "dsp.hpp"` inside `examples/equalizer/` etc. resolves to the real header. See [dsp-contract.md](dsp-contract.md) for the full ABI.

> **Gotcha:** `examples/README.md` is *not* a guide to the examples — it is the vendored README of the Signalsmith Stretch pitch-shifting library (which lives at `examples/signalsmith-stretch.h` and `examples/dsp/`). This page is the examples guide.

## The examples, in recommended reading order

| Example | Complexity | Teaches | Builds? |
|---|---|---|---|
| `examples/equalizer/` | ★☆☆☆ | dsp.hpp subclassing, Biquad/Eq5Bands usage, knob-change detection, correct exports | yes |
| `examples/pluto/` | ★★☆☆ | what FAUST-generated code looks like against `dsp.hpp`; delay-line modulation | yes |
| `examples/spectrometer/` | ★★☆☆ | FAUST-generated six-band peaking EQ; per-block coefficient recompute | yes |
| `examples/pitchshifter.cpp` | sketch | vendored-library integration idea only — **do not copy** | no (not wired into CMake) |
| `examples/juce_effect/` | ★★★☆ | JUCE integration, own Docker/toolchain build | separate build (skipped in `examples/CMakeLists.txt`) |
| `featured/aida-x-*` | ★★★★ | production-grade file loading, convolution, neural inference | convolver+tests yes; player disabled |

### 1. `equalizer` — start here (★☆☆☆)

[`examples/equalizer/equalizer.cpp`](../examples/equalizer/equalizer.cpp) is a five-band EQ (bass/mid/treble/depth/presence, ±8 dB each, plus a sweepable mid frequency and a mid peak/bandpass switch). In ~130 lines it demonstrates almost the entire hand-written-C++ pattern from [guide-native-cpp.md](guide-native-cpp.md):

- Subclass `dsp`, override the two pure virtuals `instanceConstants()` and `compute(int count, FAUSTFLOAT* input0, FAUSTFLOAT* output0)`.
- Export the required symbols:

  ```cpp
  extern "C" dsp* create() { return new Equalizer(); }
  extern "C" void destroy(dsp* p) { delete p; }
  extern "C" const char* dsp_version = DSP_VERSION;
  ```

- **Knob-change detection** — the single most important CPU habit on this platform. `compute()` compares `knobs[i]` against a cached `knobs_old[i]` and only calls the (transcendental-heavy) `eq5Bands->updateBass()`-style coefficient recompute when a knob actually moved. Copy this pattern.
- The `MAP(x, in_min, in_max, out_min, out_max)` macro from `resources/dsp.hpp` for translating app-side 0–10 knob values into real units.

Two things in it predate the current guidelines — read them as history, not as a template:

- *note:* `instanceConstants()` here resets `knobs[0..5]` to defaults. This predates the current guidelines: the host sets knobs immediately *after* `instanceConstants()`, so it happens to be harmless, but treat `instanceConstants()` as "reset DSP state", not "reset parameters" (see [dsp-contract.md](dsp-contract.md)).
- *note:* it `delete`s and re-`new`s its `Eq5Bands` on every `instanceConstants()` call, and `Eq5Bands` has no destructor for the five `Biquad`s it `new`s — a small leak on every chain assembly. This predates the current guidelines; prefer allocating once in the constructor and resetting state in `instanceConstants()`.
- *note:* it trusts `knobs[]` to be in 0–10. Values arrive raw and unclamped from presets/BLE (real presets have contained 128 where 0–10 was expected) — clamp every knob in your own effect (see [rt-safety.md](rt-safety.md)).

**Checkpoint:** you can point at the three `extern "C"` exports, the two overridden virtuals, and the knob-change-detection pattern in `equalizer.cpp` and say what each is for.

### 2. `pluto` — FAUST output, readable size (★★☆☆)

`examples/pluto/` ships both the FAUST source ([`pluto.dsp`](../examples/pluto/pluto.dsp), 12 lines — a pitch-wobble/vibrato built from `ef.transpose` driven by `os.osc`) and the generated C++ ([`pluto.cpp`](../examples/pluto/pluto.cpp), header says "Code generated with Faust 2.37.3"). Reading the two side by side shows you exactly what FAUST turns a `vslider` into: a `knobs[N]` read smoothed by a one-pole (`fRec0[0] = fSlow0 + 0.993f * fRec0[1]`), a 131072-sample delay line as a plain member array, and a 65536-entry sine table filled once in `instanceConstants()`.

What to notice:

- `compute()` is fully RT-safe: fixed member arrays, no allocation, one tight sample loop.
- The sine-table fill in `instanceConstants()` heap-allocates a small helper object — fine, because `instanceConstants()` runs on the controller thread, never the audio thread ([runtime-environment.md](runtime-environment.md)).
- *note:* `instanceConstants()` resets `knobs[0..2] = 5.0` — same predates-the-guidelines caveat as equalizer.
- *note:* pluto exports **only** `create()` — no `dsp_version` symbol. The firmware therefore treats it as a v1 "legacy" effect and calls `compute()` in 16-sample chunks. It still works, but new effects should export `dsp_version` and take the single full-buffer call ([dsp-contract.md](dsp-contract.md)).
- These two examples were generated with an older FAUST flow and hand-adapted to subclass `resources/dsp.hpp`. The *production* FAUST pipeline (FX Builder / Tone Shop) uses a different architecture file and different conventions — if you're writing FAUST, follow [guide-faust.md](guide-faust.md), and use pluto only to understand what generated DSP code looks like.

### 3. `spectrometer` — six-band FAUST EQ (★★☆☆)

`examples/spectrometer/` is the same shape as pluto (FAUST 2.37.3 source + generated C++): six `fi.peak_eq` bands at 100/200/400/800/1600/3200 Hz, one knob each. It is a good second FAUST read because the generated code is pure filtering — no tables, no delay lines.

One instructive contrast with equalizer: spectrometer recomputes all of its `fSlow*` coefficient block (including `std::pow` calls) at the top of **every** `compute()` call, whether or not a knob moved. That is standard FAUST codegen — once per block, not per sample, so it fits the budget here — but it is exactly the cost that equalizer's change-detection avoids. When you hand-write C++, detect changes. Like pluto, it exports only `create()` (legacy 16-sample chunking applies) and resets `knobs[0..5]` in `instanceConstants()` — both predate the current guidelines.

### 4. `pitchshifter.cpp` — a sketch, not an example (read, don't copy)

`examples/pitchshifter.cpp` wires the vendored Signalsmith Stretch library (`examples/signalsmith-stretch.h` + the `examples/dsp/` headers) into a `dsp` subclass for a +12-semitone shift. Be aware:

- It is **not built**: `examples/CMakeLists.txt` only iterates *directories*, so top-level `.cpp` files are never compiled into a target.
- Its `compute()` is wrong as written — it calls `stretch.process(...)` on the whole buffer once per sample (`for (i0 < count) { stretch.process(inputBuffers, count, outputBuffers, count); }`) and then calls `stretch.reset()` every block, which discards the stretcher's state each callback.
- Because it sits directly in `examples/`, its `#include "dsp.hpp"` resolves to the **stale** `examples/dsp.hpp` sitting next to it, not `resources/dsp.hpp`.

Treat it as a pointer to a useful library, nothing more.

### 5. `juce_effect` — JUCE integration (★★★☆, at your own risk)

[`examples/juce_effect/`](../examples/juce_effect/) is a self-contained template (own `Dockerfile`, own CMake toolchain files, a vendored JUCE checkout in `examples/juce_effect/JUCE/` — its README pins **JUCE 7.0.6** as the known-good version) that builds a one-knob `juce::dsp::IIR` low-pass filter. It exists for one reason: pulling JUCE classes, or third-party libraries written for JUCE, into a Stratus/Nimbus effect. It is deliberately excluded from the main examples build (`examples/CMakeLists.txt` skips the directory with an `@TODO`).

The effect source ([`src/juce_effect.cpp`](../examples/juce_effect/src/juce_effect.cpp)) is short and shows the essentials: wrapping the in-place mono buffer in `juce::dsp::AudioBlock`, polling `knobs[0]` for changes (there is no knob callback), and mapping 0–10 to 20 Hz–5 kHz with `juce::jmap`.

> **Warning:** the libstdc++ caveat. `juce_effect/README.md` documents that **older pedals** shipped Debian Stretch, whose libstdc++ cannot run JUCE-built effects — those units needed a forced libstdc++ upgrade toward Bullseye. Current firmware units ship a newer runtime, so this mostly affects old stock; still, position JUCE as an advanced, at-your-own-risk path compared to plain C++ or FAUST. Build inside the provided containers to stay on the supported glibc/libstdc++ floor ([build-docker.md](build-docker.md)).

Two of its assumptions predate the current guidelines:

- *note:* `samplesPerBlockDefault = 16` ("Most likely won't change") reflects the v1 legacy chunking era — juce_effect exports only `create()`, so the firmware does feed it 16-sample chunks. A modern effect exporting `dsp_version = "2.0.0"` sees one full-buffer call (128 frames in production) and must handle **any** `count` (see [runtime-environment.md](runtime-environment.md)).
- *note:* `compute()` calls `prepareToPlay(count)` when the block size changes, and `juce::dsp` `prepare()` calls may allocate — that's heap work on the audio thread, which [rt-safety.md](rt-safety.md) forbids. Pre-prepare for your maximum block size in the constructor instead.
- Its README's deploy instructions (SFTP + `./fw`) are dated; to run your compiled `.so` on hardware, upload it to the FX Builder and publish privately — see [deploy-to-hardware.md](deploy-to-hardware.md).
- *note:* JUCE's large translation units are memory-hungry under QEMU emulation — if a Docker build dies with `g++: internal compiler error: Killed (program cc1plus)` or exit code 137, raise Docker Desktop's memory allowance (see the [build-docker.md troubleshooting table](build-docker.md#troubleshooting)).

## `common/` — shared DSP helpers

Four header-level helpers, all usable from any effect (`examples/CMakeLists.txt` already has `common/` on the include path). None of the tutorial examples except equalizer currently use them; the `featured/` effects use them heavily.

**`Biquad`** (`common/Biquad.h`/`.cpp`) — Nigel Redmon's EarLevel biquad: one second-order filter section with double-precision coefficients/state and an inlined per-sample `process()`. Note that `Fc` is *normalized* (frequency ÷ sample rate), as the Eq classes show (`bass_freq / samplerate`).

```cpp
Biquad(int type, double Fc, double Q, double peakGainDB);   // types: bq_type_lowpass, _highpass,
void  setBiquad(int type, double Fc, double Q, double peakGainDB); // _bandpass, _notch, _peak,
float process(float in);                                    // _lowshelf, _highshelf
```

**`Eq2Bands`** (`common/Eq2Bands.hpp`) — a minimal tone stack: high-pass ("bass", default 250 Hz) into low-pass ("treble", default 1500 Hz), each a `Biquad`. Set the public `bass_freq` / `treble_freq` fields, call the matching `update*()`, then `process()` a block. Used by `featured/aida-x-convolver` for post-IR tone shaping. *note:* it `new`s its two Biquads and defines no destructor — this predates the current guidelines; allocate it once and keep it.

```cpp
Eq2Bands(float sr);
void updateBass();  void updateTreble();
void process(int count, const float* input, float* output);
```

**`Eq5Bands`** (`common/Eq5Bands.hpp`) — the five-band amp-style EQ behind the equalizer example: bass low-shelf, mid peak-or-bandpass (`mid_type`), treble high-shelf, depth peak at 75 Hz, presence high-shelf at 900 Hz. Same usage pattern (set public fields → `update*()` → `process()`), same no-destructor caveat as `Eq2Bands`.

```cpp
Eq5Bands(float sr);
// public fields: bass_boost_db, mid_boost_db, mid_freq, mid_q, mid_type (0=peak,1=bandpass),
//                treble_boost_db, depth_boost_db, presence_boost_db
void updateBass(); void updateMid(); void updateTreble(); void updateDepth(); void updatePresence();
void process(int count, const float* input, float* output);
```

**`ValueSmoother`** (`common/ValueSmoother.hpp`) — DPF's `ExponentialValueSmoother` and `LinearValueSmoother`, allocation-free and ideal for de-zippering knob changes: set the target from the (controller-thread-written) knob once per block, call `next()` once per sample inside your loop. This is the recommended way to smooth parameters ([rt-safety.md](rt-safety.md)).

```cpp
ExponentialValueSmoother s;            // or LinearValueSmoother
s.setSampleRate(44100.0f);
s.setTimeConstant(0.02f);              // T60 in seconds (exponential) / segment time (linear)
s.setTargetValue(gain);                // per block
float g = s.next();                    // per sample
```

## `modules/` — third-party submodules

| Submodule | What it does | Used by |
|---|---|---|
| `modules/FFTConvolver` (falkTX) | Partitioned FFT convolution — long impulse responses at bounded per-block cost | `featured/aida-x-convolver` (`#include <FFTConvolver.h>`) |
| `modules/RTNeural` (MaxPayne86 fork) | Real-time neural-network inference (the engine behind AIDA-X amp models) | `featured/aida-x-player` (`#include <RTNeural/RTNeural.h>`) |
| `modules/r8brain` | High-quality sample-rate conversion | `featured/aida-x-convolver` (`CDSPResampler.h`, resampling IR files to 44.1 kHz) |
| `modules/dr_libs` | Single-file WAV/FLAC decoders (`dr_wav.h`, `dr_flac.h`) | `featured/aida-x-convolver` (decoding IR files) |

Two more third-party libraries live *outside* `modules/` in this tree: the Signalsmith DSP/Stretch code is vendored directly into `examples/` (`signalsmith-stretch.h`, `examples/dsp/`), and JUCE is checked out inside `examples/juce_effect/JUCE/`.

## `featured/` — production-grade reading

These are real shipped-quality effects (GPL-3.0, by Massimo Pennazio / AIDA DSP) and the best place to see the full contract exercised — file loading, the `MAXFILES` file-path API, correct exports, and heavy-but-legal work kept out of the audio thread.

- **`featured/aida-x-convolver`** — an IR loader / cabinet simulator. `instanceConstants()` decodes up to `MAXFILES` (= 5) WAV impulse responses with dr_wav from `getFilePath(i)`, resamples, and initializes one `FFTConvolver` per slot; `compute()` picks the active IR with the `SELECTOR(knob, n)` macro, runs optional `Eq2Bands` tone shaping (with equalizer-style knob-change detection), then convolves in place. Note the honest source comment on the loader: `/* @TODO: warning!!! This is heavy and should NEVER be used from an audio rt thread */` — heavy I/O belongs in `instanceConstants()` (controller thread), never in `compute()`. Its `printf` calls are also controller-thread only. Exports all three symbols (`create`/`destroy`/`dsp_version`).
- **`featured/aida-x-player`** — neural amp modeling: loads AIDA-X model files via RTNeural, dispatching over model architectures with `std::visit` (`model_variant.hpp`). Same structure as the convolver (load in `instanceConstants()`, `SELECTOR` to switch models, in-place `compute()`). Currently *disabled* in the top-level `CMakeLists.txt` (its `add_subdirectory` line is commented out) — read it, don't expect a binary from the default build.
- **`featured/aida-x-tests`** — offline benchmark drivers (`benchmark_convolver.cpp`, `benchmark_player.cpp`) for the two effects above; a model for writing your own host-side performance tests.

## `tests/` and `resources/`

- `tests/benchmark_plugin.cpp` builds the `benchmark-plugin` harness: it `dlopen`s any effect `.so`, feeds it noise, and reports an x-realtime figure. Full usage, and why QEMU numbers are only relative, in [verification.md](verification.md).
- `resources/dsp.hpp` — the ABI header ([dsp-contract.md](dsp-contract.md)). `resources/compilation_flags.txt` — the flag list, explained in [build-flags-reference.md](build-flags-reference.md). `resources/ui_assets/` — the artwork templates for Tone Shop listings ([release-and-submission.md](release-and-submission.md)).

## Examples you may hear about that are not in this tree

Some newer examples — a spring reverb, "subspace" (a dynamics/expander effect), a Leslie sim, and a pitch-tracking harmonizer / polyphonic harmonizer — exist on Chaos Audio development branches but have not landed on `main`. If you see them referenced in Discord or commit history, that's what they are; this page will cover them when they merge. Don't go looking for `examples/springreverb` in this checkout — it isn't here yet.

**Checkpoint:** you know which example to open first (`equalizer`), which files are traps (`examples/dsp.hpp`, `examples/README.md`, `pitchshifter.cpp`), and where every submodule is actually consumed.

Next: [release-and-submission.md](release-and-submission.md)
