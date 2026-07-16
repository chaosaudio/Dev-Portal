# Native C++ Guide: Build a Tremolo, Start to Finish

This page walks you through writing a complete effect in plain C++ — a tremolo with a rate knob, a depth knob, and a 3-position wave selector — from empty file to audio coming out of your pedal. You should have already read [the DSP contract](dsp-contract.md) and completed [the quickstart](quickstart.md); everything here applies identically to **Stratus** and **Nimbus** (same CPU, same firmware, same `.so` format — build once, runs on both).

If you'd rather not write C++ at all, the [FAUST guide](guide-faust.md) is the shorter path. Come back here when you need full control.

## What you're building

A classic tremolo: an LFO modulates the signal's volume.

| Control    | Index      | App range | Mapped to                          |
|------------|------------|-----------|------------------------------------|
| Rate       | `knobs[0]` | 0–10      | 0.5 Hz – 12 Hz LFO frequency       |
| Depth      | `knobs[1]` | 0–10      | 0–100 % modulation depth           |
| Wave       | `knobs[2]` | 0–10      | 3-position selector: sine / triangle / square |

The wave selector is a knob used as a stepped selector via the `SELECTOR` macro from `resources/dsp.hpp`. That is the most convenient choice while testing with the Beta app's 9 KNOB effect, which exposes nine 0–10 knobs. A production effect can instead use a real 3-position switch through `switches[]` (`SWITCH_STATE`: `UP=0`, `DOWN=1`, `MIDDLE=2`) — see [the DSP contract](dsp-contract.md) for switch semantics.

## Before you start

1. Clone the repo and initialize submodules (required — the top-level CMake builds targets that live in submodules):

```bash
git clone https://github.com/chaosaudio/Dev-Portal.git
cd Dev-Portal
git submodule update --init --recursive
```

2. Build the Docker build image once, per [the Docker build guide](build-docker.md).

3. Remember the environment you are targeting ([runtime details](runtime-environment.md)): one ARM Cortex-A8 core at 1 GHz shared by the whole device, fixed 44 100 Hz, mono, float32, in-place buffers, production block size 128 frames = 2.9 ms per callback.

## Step 0: Create your effect folder

The build system auto-discovers examples. From `examples/CMakeLists.txt`, every subdirectory of `examples/` becomes a shared-library target built from a `.cpp` file **with the same name as the directory**:

```cmake
foreach(EFFECT_DIR ${EFFECTS_DIRS})
    ...
    get_filename_component(EFFECT_NAME ${EFFECT_DIR} NAME_WE)
    ...
    add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp)
```

So create:

```bash
mkdir examples/tremolo
touch examples/tremolo/tremolo.cpp
```

**Checkpoint:** you have `examples/tremolo/tremolo.cpp` (empty for now), and the folder name matches the file name exactly.

## Step 1: The class skeleton

Open `examples/tremolo/tremolo.cpp`. Every hand-written effect subclasses `struct dsp` from **`resources/dsp.hpp`** — the one authoritative ABI header, byte-identical to the firmware's copy. The include path is already wired up: `examples/CMakeLists.txt` contains `include_directories(../resources)` and `include_directories(../common)`, so a plain `#include "dsp.hpp"` resolves to the right file.

> **Warning:** The repo historically also carried a stale v1 header at `examples/dsp.hpp` with a *different, incompatible object layout*. Never copy it into your project and never include it by explicit path. `resources/dsp.hpp` is the only header the firmware agrees with.

```cpp
#include "dsp.hpp"   // resources/dsp.hpp -- the authoritative ABI header

#include <cmath>
#include <cstdint>

class Tremolo : public dsp {
public:
    Tremolo();

    virtual void instanceConstants() override;
    virtual void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override;

private:
    void handleKnobs();
    static float sanitizeKnob(float v, float fallback);

    // DSP state -- declared here, reset in instanceConstants()
    // (members added in Step 2)
};
```

Why subclassing matters: the firmware dispatches your methods **by vtable slot, not by name**. `struct dsp` declares the virtuals in the frozen order `setKnob, getKnob, setSwitch, getSwitch, setStompSwitch, getStompSwitch, stompSwitchPressed, instanceConstants, compute, benchmark`. Inherit from it and you get that order for free; roll your own class layout and knob changes will dispatch into the wrong function or crash. You only need to override the two pure virtuals: `instanceConstants()` and `compute()`.

Your class can be named anything (`Tremolo` here) — the firmware never sees the name, it only calls your exported `create()` factory (Step 6).

Two things you should **not** do:

- Do not override `stompSwitchPressed()`. The base-class version is the bypass mechanism: it calls `compute()` only when the stomp switch is engaged (`stompSwitch != UP`; `UP=0` means *bypassed*, `DOWN=1` means *engaged*). Because buffers are in-place and the host pre-fills them with the dry signal, *doing nothing is bypass*.
- Do not export any symbol named `getExtensions`. That symbol is how the firmware detects FAUST-compiled effects, and when present the firmware **skips calling `instanceConstants()` entirely** — your reset hook would silently never run.

## Step 2: Members and the constructor — pre-allocate everything

Add the members. A tremolo's state is tiny, but the rule it demonstrates is universal: **every byte your effect will ever need is allocated in the constructor (or as fixed-size members), never in `compute()`**. The audio thread runs under Xenomai real-time scheduling; a single `new`, `malloc`, `std::vector` growth, lock, or `printf` in the audio path causes a mode switch and an audible dropout. See [RT safety](rt-safety.md).

```cpp
private:
    // --- LFO state (reset in instanceConstants) ---
    float phase;          // 0..1 cyclic LFO phase
    float phaseInc;       // per-sample phase increment, derived from the Rate knob

    // --- depth smoothing (avoids zipper noise on knob moves) ---
    float depthTarget;    // 0..1, set by handleKnobs()
    float depthSmoothed;  // one-pole tracker toward depthTarget
    float smoothCoeff;    // per-sample smoothing coefficient

    // --- knob cache: re-derive coefficients only when a knob actually moves ---
    float knobsCache[MAXKNOBS];
    int   wave;           // 0 = sine, 1 = triangle, 2 = square

    // If your effect needed a delay line, it would be a fixed-size member too:
    //   float delayLine[44100];   // 1 s at 44.1 kHz -- sized at compile time
    // or heap-allocated ONCE in the constructor. Never in compute().
```

```cpp
Tremolo::Tremolo() {
    // The constructor runs once per pooled instance. Pre-allocate here.
    // Do NOT rely on it for resets -- see Step 3.
    phase = 0.0f;
    phaseInc = 0.0f;
    depthTarget = 0.0f;
    depthSmoothed = 0.0f;
    smoothCoeff = 0.0f;
    wave = 0;
    for (int i = 0; i < MAXKNOBS; ++i) knobsCache[i] = -1.0f;
}
```

## Step 3: `instanceConstants()` — your only reset hook

Firmware instances are **pooled per effect and never destroyed** (`~dsp` is non-virtual; your destructor will never run). When any preset that uses your effect is assembled, the firmware fetches a possibly *dirty* pooled instance — still carrying filter states, delay-line contents, and envelope values from the last preset — and calls `instanceConstants()` on it. It is your one chance to reset.

Reset **all DSP state** here. Do *not* try to reset `knobs[]` to defaults — the host sets the real knob values immediately after `instanceConstants()` returns, and treating this hook as "reset parameters" instead of "reset DSP state" is a recurring bug in older examples.

```cpp
void Tremolo::instanceConstants() {
    name = "Tremolo";
    version = "1.0.0";

    // Reset ALL internal DSP state -- this instance may be dirty
    // from a previous preset (instances are pooled, never destroyed).
    phase = 0.0f;
    phaseInc = 0.0f;
    depthTarget = 0.0f;
    depthSmoothed = 0.0f;
    wave = 0;

    // ~5 ms exponential smoothing time, derived from the actual sample rate.
    // Hardware is fixed at 44100, but use getSampleRate() so offline
    // test harnesses can run you at other rates.
    smoothCoeff = 1.0f - expf(-1.0f / (0.005f * (float)getSampleRate()));

    // Invalidate the knob cache so the first compute() re-derives everything
    // from whatever the host puts into knobs[].
    for (int i = 0; i < MAXKNOBS; ++i) knobsCache[i] = -1.0f;
}
```

`instanceConstants()` runs on the controller thread and is never called concurrently with `compute()`, so heavier setup work (table generation, file loading via `getFilePath()`) is legal *here* — just not in `compute()`.

**Checkpoint:** your class now compiles conceptually: skeleton, members, constructor, and a reset hook that touches every piece of DSP state and leaves `knobs[]` alone.

## Step 4: Knob validation and mapping

Knob values arrive **raw and unclamped**. The app convention is 0–10 (5 = noon), but the firmware performs no validation, and real-world presets have delivered values like `128` where 0–10 was expected. Also, `setKnob()` runs on the controller thread *while* `compute()` runs on the audio thread — a knob can change mid-buffer. Single-float reads are atomic enough on ARM; the rules that follow from this are:

1. **Sanitize every knob read** — clamp to the expected range, fall back to a default on NaN.
2. **Cache and compare** — re-derive coefficients only when a knob actually changed, never per sample.
3. **Read each knob once per block** into a local — don't re-read `knobs[i]` mid-loop.

```cpp
float Tremolo::sanitizeKnob(float v, float fallback) {
    if (v != v) return fallback;   // NaN guard -- works because the build
                                   // flags include -fno-finite-math-only
    if (v < 0.0f)  return 0.0f;
    if (v > 10.0f) return 10.0f;
    return v;
}

void Tremolo::handleKnobs() {
    // knobs[0]=Rate  knobs[1]=Depth  knobs[2]=Wave selector
    const float rate  = sanitizeKnob(knobs[0], 5.0f);
    const float depth = sanitizeKnob(knobs[1], 5.0f);
    const float sel   = sanitizeKnob(knobs[2], 0.0f);

    if (rate != knobsCache[0]) {
        knobsCache[0] = rate;
        const float hz = MAP(rate, 0.0f, 10.0f, 0.5f, 12.0f);  // 0-10 -> Hz
        phaseInc = hz / (float)getSampleRate();
    }
    if (depth != knobsCache[1]) {
        knobsCache[1] = depth;
        depthTarget = depth * 0.1f;                             // 0-10 -> 0..1
    }
    if (sel != knobsCache[2]) {
        knobsCache[2] = sel;
        float norm = sel * 0.1f;                 // 0-10 -> 0..1
        if (norm > 0.999f) norm = 0.999f;        // keep full-CW on the top position
        wave = SELECTOR(norm, 3);                // 0, 1, or 2
    }
}
```

`MAP` and `SELECTOR` come straight from `resources/dsp.hpp`:

```cpp
/* Define a macro to re-maps a number from one range to another  */
#define MAP(x, in_min, in_max, out_min, out_max) (((x - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min)

/* Define a macro to implement a selector out of a knob */
#define SELECTOR(knob, n) ((uint8_t)(knob * n) % n)
```

> **Gotcha:** `SELECTOR(knob, n)` expects a **0..1** input and *wraps at exactly full scale*: with `n = 3`, an input of exactly `1.0` computes `(uint8_t)3 % 3 = 0` — the knob fully clockwise would snap back to position 0. Normalize your 0–10 knob to 0..1 and clamp just below 1.0, as above.

## Step 5: `compute()` — count-agnostic, in-place, allocation-free

The signature is `compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs)` with `FAUSTFLOAT` = `float`. Facts your loop must respect:

- **In-place:** `inputs == outputs`. Read `inputs[i]`, write `outputs[i]`; they are the same memory. The host pre-fills the buffer with the dry signal.
- **Count-agnostic:** production calls you with `count = 128` (2.9 ms), but legacy hosts chunk in 16s, offline tools use other sizes, and the base-class `benchmark()` calls `compute()` **once with `sampleRate × seconds` samples** (e.g. 88 200). Never size a stack buffer to a fixed block size; loop over `count`, whatever it is.
- **RT-safe:** no allocation, no locks, no I/O, no syscalls — everything was pre-allocated in Steps 2–3. See [RT safety](rt-safety.md).

```cpp
void Tremolo::compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) {
    handleKnobs();   // cheap: three compares per block in the common case

    for (int i = 0; i < count; ++i) {
        // Advance the LFO
        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;

        // LFO value in 0..1
        float lfo;
        switch (wave) {
            default:
            case 0: lfo = 0.5f + 0.5f * sinf(6.28318530f * phase);            break; // sine
            case 1: lfo = (phase < 0.5f) ? (2.0f * phase)
                                         : (2.0f - 2.0f * phase);             break; // triangle
            case 2: lfo = (phase < 0.5f) ? 1.0f : 0.0f;                       break; // square
        }

        // Smooth the depth so knob moves (and the square wave's duty edges
        // scaled by a moving depth) don't zipper.
        depthSmoothed += smoothCoeff * (depthTarget - depthSmoothed);

        // gain = 1 at LFO peak, (1 - depth) at LFO trough
        const float gain = 1.0f - depthSmoothed * (1.0f - lfo);
        outputs[i] = inputs[i] * gain;
    }
}
```

A tremolo has no decaying feedback state, so denormals are not a concern here. If your effect has long decays (reverb tails, feedback delays), read the denormal section of [RT safety](rt-safety.md) — subnormal floats cost 10–100× per operation on the Cortex-A8.

## Step 6: The exports block

The firmware loads your `.so` with `dlopen(RTLD_NOW)` and resolves symbols with `dlsym`. Three exports, verbatim:

```cpp
extern "C" dsp* create() {
    return new Tremolo();
}

extern "C" void destroy(dsp* p) {
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;
```

- `create` is **mandatory** — the firmware refuses to load the effect without it.
- `dsp_version` is a **data symbol** (a `const char*` *variable*), not a function. The firmware dlsym's it as `const char**` and dereferences. If you write `const char* dsp_version()` (a function) instead, the firmware reads code bytes as a string and your effect misbehaves. `DSP_VERSION` is `"2.0.0"` (from `resources/dsp.hpp`), which opts you into a single full-buffer `compute()` call per block; if the symbol is missing you are treated as a v1 legacy effect and fed 16-sample chunks.
- `destroy` is convention: the firmware pools instances and never actually calls it, but test harnesses may.
- Optionally, a v2 effect may export `extern "C" void setSampleRate(int)`; the firmware calls it once per `.so` with `44100` at load time. A fixed-rate effect like this one doesn't need it.

## The complete source

`examples/tremolo/tremolo.cpp`, ready to compile:

```cpp
// Tremolo -- example native C++ effect for Chaos Audio Stratus / Nimbus.
// knobs[0] = Rate (0-10 -> 0.5-12 Hz)
// knobs[1] = Depth (0-10 -> 0-100 %)
// knobs[2] = Wave selector (3 positions: sine / triangle / square)

#include "dsp.hpp"   // resources/dsp.hpp -- the authoritative ABI header

#include <cmath>
#include <cstdint>

class Tremolo : public dsp {
private:
    // --- LFO state (reset in instanceConstants) ---
    float phase;
    float phaseInc;

    // --- depth smoothing ---
    float depthTarget;
    float depthSmoothed;
    float smoothCoeff;

    // --- knob cache ---
    float knobsCache[MAXKNOBS];
    int   wave;   // 0 = sine, 1 = triangle, 2 = square

    static float sanitizeKnob(float v, float fallback) {
        if (v != v) return fallback;   // NaN guard (needs -fno-finite-math-only)
        if (v < 0.0f)  return 0.0f;
        if (v > 10.0f) return 10.0f;
        return v;
    }

    void handleKnobs() {
        const float rate  = sanitizeKnob(knobs[0], 5.0f);
        const float depth = sanitizeKnob(knobs[1], 5.0f);
        const float sel   = sanitizeKnob(knobs[2], 0.0f);

        if (rate != knobsCache[0]) {
            knobsCache[0] = rate;
            const float hz = MAP(rate, 0.0f, 10.0f, 0.5f, 12.0f);
            phaseInc = hz / (float)getSampleRate();
        }
        if (depth != knobsCache[1]) {
            knobsCache[1] = depth;
            depthTarget = depth * 0.1f;
        }
        if (sel != knobsCache[2]) {
            knobsCache[2] = sel;
            float norm = sel * 0.1f;
            if (norm > 0.999f) norm = 0.999f;
            wave = SELECTOR(norm, 3);
        }
    }

public:
    Tremolo() {
        phase = 0.0f;
        phaseInc = 0.0f;
        depthTarget = 0.0f;
        depthSmoothed = 0.0f;
        smoothCoeff = 0.0f;
        wave = 0;
        for (int i = 0; i < MAXKNOBS; ++i) knobsCache[i] = -1.0f;
    }

    virtual void instanceConstants() override {
        name = "Tremolo";
        version = "1.0.0";

        // Full DSP-state reset: this pooled instance may be dirty.
        phase = 0.0f;
        phaseInc = 0.0f;
        depthTarget = 0.0f;
        depthSmoothed = 0.0f;
        wave = 0;

        smoothCoeff = 1.0f - expf(-1.0f / (0.005f * (float)getSampleRate()));

        for (int i = 0; i < MAXKNOBS; ++i) knobsCache[i] = -1.0f;
    }

    virtual void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override {
        handleKnobs();

        for (int i = 0; i < count; ++i) {
            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;

            float lfo;
            switch (wave) {
                default:
                case 0: lfo = 0.5f + 0.5f * sinf(6.28318530f * phase);  break;
                case 1: lfo = (phase < 0.5f) ? (2.0f * phase)
                                             : (2.0f - 2.0f * phase);   break;
                case 2: lfo = (phase < 0.5f) ? 1.0f : 0.0f;             break;
            }

            depthSmoothed += smoothCoeff * (depthTarget - depthSmoothed);

            const float gain = 1.0f - depthSmoothed * (1.0f - lfo);
            outputs[i] = inputs[i] * gain;
        }
    }
};

extern "C" dsp* create() {
    return new Tremolo();
}

extern "C" void destroy(dsp* p) {
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;
```

## Wire it into the build

For a single-file effect named to match its folder (`examples/tremolo/tremolo.cpp`), **no CMake edit is required** — the `foreach` loop shown in Step 0 picks it up automatically, strips the `lib` prefix, and installs it to `bins/`:

```cmake
    # Remove the 'lib' prefix from the output library name
    set_target_properties(${EFFECT_NAME} PROPERTIES PREFIX "")

    # Install the library to the 'bins' directory
    install(TARGETS ${EFFECT_NAME} LIBRARY DESTINATION bins)
```

If your effect grows to multiple source files, add a special case to the loop in `examples/CMakeLists.txt`, exactly the way the repo already does for `equalizer` (which links the shared `Biquad` object library):

```cmake
    # If the current file is Equalizer.cpp, link it with Biquad
    if(EFFECT_NAME STREQUAL "equalizer")
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp $<TARGET_OBJECTS:Biquad>)
    else()
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp)
    endif()
```

For a hypothetical two-file tremolo, the edit would be:

```cmake
    if(EFFECT_NAME STREQUAL "tremolo")
        add_library(${EFFECT_NAME} SHARED
            ${EFFECT_DIR}/tremolo.cpp
            ${EFFECT_DIR}/lfo_tables.cpp)
    elseif(EFFECT_NAME STREQUAL "equalizer")
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp $<TARGET_OBJECTS:Biquad>)
    else()
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp)
    endif()
```

The correct optimization and ABI flags (`-fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math ... -fno-finite-math-only`) are already set for every target in `examples/CMakeLists.txt` — you inherit them for free. See [the build flags reference](build-flags-reference.md) for what each one does and why `-fno-finite-math-only` in particular is load-bearing.

> **Gotcha:** All of those flags live in `CMAKE_CXX_FLAGS_RELEASE` (`examples/CMakeLists.txt` line 7), so they only apply to Release builds. The container's entrypoint passes `-DCMAKE_BUILD_TYPE=Release` for you; if you ever run `cmake` manually, you must pass it too — a configure without it silently builds with **none** of these flags, including the load-bearing `-fno-finite-math-only`, and the resulting `.so` can fail to load on the device.

## Build it in the container

From the repo root (the image itself is a one-time setup — see [the Docker build guide](build-docker.md); this is emulated armhf compilation under QEMU, so the first full build takes a while, incremental builds are much faster):

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder
```

Expected output (trimmed — a full first build compiles every example):

```
-- The C compiler identification is GNU 10.2.1
...
[ 62%] Building CXX object examples/CMakeFiles/tremolo.dir/tremolo/tremolo.cpp.o
[ 64%] Linking CXX shared library tremolo.so
...
-- Installing: /workdir/bins/tremolo.so
-- Installing: /workdir/bins/tests/benchmark/benchmark-plugin
```

Confirm the artifact is a 32-bit ARM shared object:

```bash
file bins/tremolo.so
```

Expected output:

```
bins/tremolo.so: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

**Checkpoint:** `bins/tremolo.so` exists and `file` reports `ELF 32-bit ... ARM`. Building inside the provided bullseye container also pins your glibc requirements to what production units actually ship — building on a random newer distro can produce a `.so` that silently fails to load on-device.

## Smoke-test in the container

Before touching hardware, prove the `.so` loads and runs. The `benchmark-plugin` harness dlopens your effect exactly the way the firmware does (`RTLD_NOW`), checks for `dsp_version` and `create`, then runs `benchmark()`:

```bash
docker run --rm -it --platform linux/arm --entrypoint /bin/bash -v "$(pwd):/workdir" chaos-audio-builder
cd /workdir
./bins/tests/benchmark/benchmark-plugin ./bins/tremolo.so 2 44100
```

Expected output:

```
Loading library: ./bins/tremolo.so
Loading symbol: create
Creating instance
Setting sample rate: 44100
Invoking instanceConstants method
Invoking benchmark method
Starting benchmark
Generating input signal
Computing 2 seconds of data @44100Hz
Processed 2 seconds of signal in 0.31 seconds
6.4 x real-time
Deleting instance
```

> **Warning:** Under QEMU emulation the "x real-time" number is meaningless as an absolute figure. Use this step to confirm *loads-and-runs* and for relative comparisons between two builds of your own effect. Real CPU numbers come from the device — see [verification](verification.md).

Note that `benchmark()` just called your `compute()` once with 88 200 samples. If your effect had a fixed 128-sample internal buffer, this step would have crashed — which is exactly why it's here.

**Checkpoint:** the harness prints the full sequence above with no `Cannot open library` error. If `dlopen` fails here, it will fail on the pedal too — see [troubleshooting](troubleshooting.md).

## Deploy to the pedal

Copy the `.so` to the device's effects directory under the Effect ID name, fix ownership, and restart the audio service. The same hostname and commands apply on Nimbus — the procedure is identical. You'll need the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it).

```bash
scp bins/tremolo.so root@stratus.local:/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
ssh root@stratus.local 'chown update:sftponly /opt/update/sftp/firmware/effects/<EFFECT-ID>.so && systemctl restart bela_startup'
```

`<EFFECT-ID>` — the GUID the firmware knows the effect by; the file must be named exactly `<EFFECT-ID>.so`. For local testing any GUID-shaped name works as long as a preset references it, but the easiest path is the Beta app's **9 KNOB** tester effect, whose ID is `55631e3a-94f7-42f8-8204-f5c6c11c4a21` — name your file `55631e3a-94f7-42f8-8204-f5c6c11c4a21.so` and you can audition the tremolo from the app with nine live 0–10 knobs (Rate = knob 1, Depth = knob 2, Wave = knob 3). The real Effect ID for a released effect is assigned at submission — see [release and submission](release-and-submission.md).

> **Warning:** Never create or modify a `.version` file in `/opt/update/sftp/firmware/` — it triggers a full firmware reinstall.

Full deployment details (and what to do when the effect doesn't appear) are in [deploy to hardware](deploy-to-hardware.md).

## Verify on hardware

Watch the service log while it restarts:

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Look for the effect being found and loaded, and make sure no `dlopen` error or `Underrun detected` lines appear. Then select the effect (9 KNOB in the Beta app), play, and check each control:

1. **Rate** (knob 1): sweep 0→10; pulsing speeds up from a slow ~0.5 Hz throb to a fast 12 Hz flutter.
2. **Depth** (knob 2): at 0 the effect is transparent; at 10 the signal pumps almost to silence.
3. **Wave** (knob 3): three distinct zones — smooth (sine), linear ramp (triangle), hard chop (square). No clicks when moving Depth, thanks to the smoother.
4. **Stomp:** bypassed = clean dry signal (you wrote no bypass code — that's the in-place contract working).

**Checkpoint:** all three knobs behave as mapped and the log stays free of underruns. For real CPU measurement (`/proc/xenomai/sched/stat`, the ~15 %-of-core budget guidance, and the benchmark-numbers-on-device gotchas), continue to [verification](verification.md).

## Toolbox: `common/` helpers

The repo ships a few battle-tested building blocks under `common/`, already on the include path for every example:

- **`common/Biquad.h` / `Biquad.cpp`** — Nigel Redmon's EarLevel biquad (lowpass, highpass, bandpass, notch, peak, low/high shelf; see the `bq_type_*` enum). Double-precision state, inline `float process(float in)`. Note it has a separate `.cpp`: to use it, add your effect to the CMake special-case branch and link `$<TARGET_OBJECTS:Biquad>`, exactly as `equalizer` does.
- **`common/Eq2Bands.hpp` / `Eq5Bands.hpp`** — AIDA-X tone-stack classes built on Biquad (2-band HP/LP; 5-band bass/mid/treble/depth/presence). `examples/equalizer/equalizer.cpp` is a complete worked example of `Eq5Bands` with knob caching. They allocate their internal Biquads with `new` — construct them in your constructor or `instanceConstants()`, never in `compute()`.
- **`common/ValueSmoother.hpp`** — DPF's `ExponentialValueSmoother` and `LinearValueSmoother` (`setSampleRate()`, `setTimeConstant()`, `setTargetValue()`, `next()`). Allocation-free and the recommended replacement for the hand-rolled one-pole smoother in this guide once you have more than one smoothed parameter.

## When to reach for `modules/`

`modules/` holds git submodules for heavier lifting (hence the mandatory `git submodule update --init --recursive`):

- **RTNeural** — real-time neural network inference; the engine behind neural amp/pedal capture-style effects.
- **FFTConvolver** — partitioned FFT convolution; cabinet IRs and convolution reverb.
- **r8brain** — high-quality sample-rate conversion; use for offline/setup-time resampling (e.g. conforming an IR to 44.1 kHz in `instanceConstants()`), not per-block.
- **dr_libs** — single-file decoders (`dr_wav` et al.) for loading WAV data at setup time.
- **`examples/signalsmith-stretch.h`** — Signalsmith's pitch-shift/time-stretch header, vendored directly in `examples/`.
- **JUCE** — via the self-contained `examples/juce_effect` template; see the porting section below.

## Porting existing DSP code

If you already have working DSP in another framework, the translation is usually mechanical:

- `prepareToPlay` / `init(sampleRate)` → constructor (allocations) + `instanceConstants()` (state reset). Remember `instanceConstants()` re-runs on every chain assembly.
- `processBlock` → `compute(count, in, out)` — mono, float32, in-place, any `count` from 1 to 1024. If your code assumes stereo, sum or process one channel; if it assumes a fixed block size, make it loop.
- Parameter objects → `knobs[0..9]`, raw 0–10 floats you must sanitize and map yourself (Step 4).
- Delete anything that allocates, locks, logs, or touches files per block. It will run — until it audibly glitches under Xenomai.
- Never create threads (`std::thread`, JUCE `Timer`/`ThreadPool`, async loaders) — there is one core, the audio thread owns it, and a pooled instance's thread can never be shut down (destructors never run). Do everything in the constructor / `instanceConstants()` on the controller thread.
- Denormal guards written for x86 (`_mm_setcsr`, SSE intrinsics) are no-ops on this ARM target; rely on the flag setup described in [RT safety](rt-safety.md) instead.

**JUCE:** `examples/juce_effect` is a complete template (own `Dockerfile`, own CMake toolchain files, pinned to JUCE 7.0.6) showing a `juce_dsp` low-pass filter wrapped as a Stratus effect, with `juce_core`, `juce_audio_basics`, `juce_audio_formats`, and `juce_dsp` modules. Treat it as an advanced, at-your-own-risk path: its README documents that older stock pedals (Debian Stretch era) required a forced `libstdc++` upgrade to run JUCE-built effects — current firmware units ship a newer runtime, but read `examples/juce_effect/README.md` in full before committing to JUCE, and prefer plain C++ (this guide) or [FAUST](guide-faust.md) when you don't specifically need JUCE classes. One build-time note: JUCE's large translation units are memory-hungry under QEMU emulation — if the build dies with `g++: internal compiler error: Killed (program cc1plus)` or container exit code 137, raise Docker Desktop's memory allowance (Settings → Resources, 6–8 GB) and retry; see [the Docker build guide](build-docker.md)'s troubleshooting table.

Next: [FAUST guide](guide-faust.md)
