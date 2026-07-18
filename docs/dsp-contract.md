# The DSP Binary Contract

This page is the authoritative reference for the binary interface every Stratus and Nimbus effect must implement: the one header you may include, the `struct dsp` you subclass, the `extern "C"` symbols the firmware loads, and the lifecycle rules the host imposes. Read it before writing your first hand-written C++ effect, and come back to it whenever an effect loads but misbehaves — most "my knob does nothing" and "my effect never appears" problems are violations of something on this page.

If you want a guided walkthrough instead of a reference, start with [quickstart.md](quickstart.md) and [guide-native-cpp.md](guide-native-cpp.md). FAUST developers get most of this contract generated for them — see [guide-faust.md](guide-faust.md) — but should still read the [dsp_version](#dsp_version-semantics) and [getExtensions](#the-getextensions-rule) sections to understand what their generated code is doing.

---

## The one header: `resources/dsp.hpp`

There is exactly one authoritative ABI header in this repository: **`resources/dsp.hpp`**. It is kept byte-identical to the header the firmware itself is compiled against. Your effect includes it, subclasses its `struct dsp`, and is loaded by a host that was built from the same file. Any divergence between your copy and the firmware's copy is an ABI break.

```cpp
#include "dsp.hpp"   // resources/dsp.hpp — the only header your effect needs
```

Compile with `-I <path-to>/resources` so this include resolves to `resources/dsp.hpp`.

Key constants defined there (`resources/dsp.hpp`):

```cpp
#define DSP_VERSION "2.0.0"

#define MAXKNOBS 10
#define MAXSWITCHES 5
#define MAXFILES 5

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif
```

`FAUSTFLOAT` is always `float` on this platform. All audio is 44 100 Hz mono float32 — see [runtime-environment.md](runtime-environment.md).

> **Gotcha:** the repository historically shipped a *second* file called `examples/dsp.hpp`. It was a stale v1-era header — no `DSP_VERSION`, no `filePaths`/trails members, a different object layout — and an effect built against it had `knobs[]` and `switches[]` at the wrong byte offsets relative to what the firmware expects. It now survives only as a compatibility shim that includes the real header so old example code keeps compiling. Never include it in new code, never copy it into your project. If your include path picks up anything other than `resources/dsp.hpp`, fix the include path.

---

## Anatomy of `struct dsp`

`resources/dsp.hpp` defines the base class. The data members are the shared state between your code and the host — the host reads and writes them through the virtual setters/getters, and your `compute()` reads them directly.

```cpp
struct dsp {
    enum SWITCH_STATE { UP = 0, DOWN = 1, MIDDLE = 2 };

    private:
        int fSampleRate = 44100;
        std::string filePaths[MAXFILES];
        bool trailsSetting = false;

    protected:
        std::string version;

    public:
        float knobs[MAXKNOBS];
        SWITCH_STATE switches[MAXSWITCHES];
        SWITCH_STATE stompSwitch;
        std::string name;
        bool trailsAvailable = false;
    ...
};
```

Who touches what, and when:

| Member | Written by | Read by | Notes |
|---|---|---|---|
| `knobs[10]` | HOST, via `setKnob(int, float)` — at chain assembly (right after `instanceConstants()`) and live from the app | YOUR `compute()` | Values arrive **raw and unclamped**. The app convention is 0–10 with 5 = noon, but real presets have contained 128 where 0–10 was expected. Validate every read. Constructor default is 0.5 for all ten. |
| `switches[5]` | HOST, via `setSwitch(int, SWITCH_STATE)` | YOUR `compute()` | 2- or 3-position toggles. Constructor default: all `DOWN`. |
| `stompSwitch` | HOST, via `setStompSwitch()` | Base-class `stompSwitchPressed()` | **`UP = 0` means BYPASSED; `DOWN = 1` means engaged.** See [Bypass](#bypass-stompswitchpressed) below. |
| `name`, `version` | YOU (constructor or `instanceConstants()`) | Host, via non-virtual `getName()`/`getVersion()` | Informational. `version` is `protected`, so set it from inside your class. |
| `fSampleRate` | Nobody in production — the firmware never calls the non-virtual member `setSampleRate(int)`; offline harnesses like benchmark-plugin do | You, via `getSampleRate()` / `getNyquist()` | Stays at its default of 44100, which is also the only value production hardware uses. Distinct from the *module-level* `extern "C" setSampleRate` export described below, which the firmware **does** call. |
| `filePaths[5]` | Host, via `setFilePath()` | You, via `getFilePath()` / `getFileName()` | Used by file-backed effects (IR/model loaders). Most effects never touch it. |
| `trailsAvailable`, `trailsSetting` | You declare availability; host toggles the setting via `setTrailsVal()` | You, via `getTrailsVal()` | Set `trailsAvailable = true` only if your effect meaningfully supports trails (delay/reverb tails that keep ringing after a preset switch). |

Two hazards worth calling out before you write a line of DSP:

> **Warning:** `setKnob()` runs on the controller (Bluetooth) thread **while** `compute()` runs on the audio thread. Single aligned float reads/writes are atomic-enough on ARM, so you won't see torn floats — but a knob can change mid-buffer, at any time, to any value. Validate every knob (clamp, or fall back to a sane default on NaN/out-of-range) and re-derive expensive coefficients only when the value actually changed — never per sample. See [rt-safety.md](rt-safety.md).

> **Gotcha:** `dsp.hpp` contains a debug helper `getTextForEnum()` that prints `1 → "MIDDLE"` and `2 → "DOWN"` — the opposite of the enum it sits next to (`DOWN = 1, MIDDLE = 2`). Trust the enum, not the debug printer. A 3-way switch wired from the printer's output will be backwards.

### Bypass: `stompSwitchPressed()`

The host never calls your `compute()` directly. It always calls `stompSwitchPressed()`, whose base-class implementation is:

```cpp
virtual void stompSwitchPressed(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs){
    if(stompSwitch){
        compute(count, inputs, outputs);
    }
    return;
}
```

Because `UP == 0`, an effect whose stomp switch is UP simply doesn't run. And because processing is **in-place** (`inputs == outputs`, buffer pre-filled with the dry signal — see [runtime-environment.md](runtime-environment.md)), doing nothing *is* bypass. If you override `stompSwitchPressed()`, you must preserve exactly these semantics: touch the buffer only when engaged.

### `benchmark()`

The last virtual, `benchmark(int seconds)`, is a host-side convenience that generates noise, calls `compute()` once with `seconds * 44100` samples, and prints throughput. Two things follow: never call it on the device (it heap-allocates and `printf`s), and never size internal buffers assuming a small `count` — anything that stack-buffers `count` samples into a fixed array will be smashed by it. For real measurement, use the harness described in [verification.md](verification.md).

---

## The four `extern "C"` exports

The firmware loads your effect with `dlopen(RTLD_NOW)` from `/opt/update/sftp/firmware/effects/<EFFECT-ID>.so` and resolves symbols with `dlsym`. These are the four symbols it looks for, with exact signatures:

| Symbol | Exact declaration | Required? | What the firmware does with it |
|---|---|---|---|
| `create` | `extern "C" dsp* create()` | **MANDATORY** | Constructs your effect. If `dlsym("create")` fails, **the load fails** and your effect silently vanishes from the chain (the error appears only in `journalctl -u bela_startup`). |
| `dsp_version` | `extern "C" const char* dsp_version = DSP_VERSION;` | Strongly recommended | A **data symbol**, read via `dlsym` and dereferenced as `const char**`. Missing ⇒ legacy 16-sample chunking. See [dsp_version semantics](#dsp_version-semantics). |
| `setSampleRate` | `extern "C" void setSampleRate(int)` | Optional | Called **once per .so** (not per instance) with `44100` at load time — and only when `dsp_version` is exactly `"2.0.0"`. |
| `getExtensions` | *(FAUST pipeline only)* | **Must NOT exist** in hand-written effects | Its mere presence marks the .so as FAUST-generated and changes host behavior. See [the getExtensions rule](#the-getextensions-rule). |

You will also see `extern "C" void destroy(dsp*)` in the examples (e.g. the tail of `examples/equalizer/equalizer.cpp`). Export it as a courtesy convention — but know that the firmware **never calls it**: instances are pooled and never destroyed (see [Lifecycle](#lifecycle-pooling-never-run-destructors-and-instanceconstants)).

> **Warning:** `dsp_version` is a pointer *variable*, not a function. The classic mistake is writing `extern "C" const char* dsp_version() { return "2.0.0"; }` — that exports a function symbol, `dlsym` resolves it to code bytes, and the firmware dereferences those bytes as a string pointer and reads garbage. Copy the declaration verbatim: `extern "C" const char* dsp_version = DSP_VERSION;`

---

## The canonical skeleton: a complete gain pedal

This is the minimal, complete, compilable hand-written effect. It demonstrates every rule on this page: one validated knob, coefficient caching, in-place processing that works at any block size, a full-reset `instanceConstants()`, and the correct export block. Use it as the starting template for any hand-written effect.

```cpp
// gain.cpp — minimal Stratus/Nimbus effect (canonical hand-written template)
#include "dsp.hpp"      // resources/dsp.hpp — the ONLY ABI header
#include <cmath>

class Gain : public dsp {
private:
    float gainCoeff;    // linear gain derived from knobs[0]
    float lastKnob;     // last accepted knob value, to skip recompute

    // Knobs arrive raw and unclamped from presets/BLE (real presets have
    // contained 128 where 0-10 was expected). Never trust knobs[] directly.
    static float validated(float raw) {
        if (std::isnan(raw) || raw < 0.0f || raw > 10.0f)
            return 5.0f;                 // fall back to noon
        return raw;
    }

public:
    Gain() {
        name    = "Gain";
        version = "1.0.0";
        instanceConstants();             // start from a known state
    }

    void instanceConstants() override {
        // FULL DSP-state reset. Called at every chain assembly on a pooled,
        // possibly-dirty instance (your destructor never runs). Reset ALL
        // internal state here: filters, delay lines, envelopes, counters.
        // Do NOT treat this as "reset parameters" — the host sets knobs[]
        // immediately after this call.
        lastKnob  = -1.0f;               // force coefficient recompute
        gainCoeff = 1.0f;
    }

    void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override {
        // knobs[0]: 0-10, noon = 5. Map to -24..+24 dB, unity at noon.
        const float k = validated(knobs[0]);
        if (k != lastKnob) {             // re-derive coefficients on change only
            lastKnob  = k;
            gainCoeff = DB_CO(MAP(k, 0.0f, 10.0f, -24.0f, 24.0f));
        }
        // In-place: inputs == outputs. Works for ANY count (1..1024).
        for (int i = 0; i < count; ++i)
            outputs[i] = inputs[i] * gainCoeff;
    }
};

extern "C" dsp* create() {               // MANDATORY: load fails without it
    return new Gain();
}

extern "C" void destroy(dsp* p) {        // convention only; firmware never calls it
    delete p;
}

extern "C" const char* dsp_version = DSP_VERSION;   // DATA symbol, not a function

extern "C" void setSampleRate(int sampleRate) {}    // optional; called once with 44100

// NOTE: deliberately NO getExtensions export — that would mark this .so as
// FAUST-generated and instanceConstants() would never be called.
```

Build it inside the build container (see [build-docker.md](build-docker.md)). This is the canonical flag line from [build-flags-reference.md](build-flags-reference.md) — that page is the source of truth for every flag and its rationale:

```bash
g++ -std=c++14 -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only -I resources gain.cpp -o <EFFECT-ID>.so
```

Note the `-std=c++14`: the repo's CMake builds every example as C++14 (`examples/CMakeLists.txt` sets `CMAKE_CXX_STANDARD 14`), so prototype with the same standard — C++17 features that compile standalone will fail the moment you wire the same file into `examples/`.

`<EFFECT-ID>` is the GUID the platform assigns when you create your effect in the FX Builder — you never invent it. To run the compiled `.so` on hardware, upload it to the FX Builder and publish privately (see [deploy-to-hardware.md](deploy-to-hardware.md) and [release-and-submission.md](release-and-submission.md)).

> **Warning:** `-fno-finite-math-only` is load-bearing. Plain `-ffast-math` makes GCC emit `__expf_finite`-style libm references that the device's libm does not export; `dlopen(RTLD_NOW)` then fails at load and your effect silently never appears — the only evidence is a dlerror line in `journalctl -u bela_startup`. This is verified on hardware.

**Checkpoint:** the file above compiles verbatim to a `.so` in the container, loads on a pedal or amp, and behaves as a ±24 dB gain with unity at noon.

---

## Vtable order is the ABI

The firmware dispatches your virtual methods **by vtable slot, not by name**. The slot order is the declaration order in `resources/dsp.hpp`:

| Slot | Method |
|---|---|
| 1 | `setKnob(int, float)` |
| 2 | `getKnob(int)` |
| 3 | `setSwitch(int, SWITCH_STATE)` |
| 4 | `getSwitch(int)` |
| 5 | `setStompSwitch(SWITCH_STATE)` |
| 6 | `getStompSwitch()` |
| 7 | `stompSwitchPressed(int, FAUSTFLOAT*, FAUSTFLOAT*)` |
| 8 | `instanceConstants()` *(pure — you implement it)* |
| 9 | `compute(int, FAUSTFLOAT*, FAUSTFLOAT*)` *(pure — you implement it)* |
| 10 | `benchmark(int)` |

Subclass `struct dsp` from `resources/dsp.hpp` and you inherit this order automatically — this is the entire reason the rule "include the one header, subclass, override the two pure virtuals" exists. If you instead hand-roll a lookalike class (or edit the header, or reorder/insert virtual methods before `benchmark`), the host's call to slot 8 lands on whatever *your* class put in slot 8. The classic failure: firmware calls `instanceConstants()` and your `compute()` runs with garbage arguments — an instant crash, or worse, silent memory corruption. Knob setters landing on the wrong methods is the milder version of the same disease.

Rules that follow:

- Never modify `resources/dsp.hpp`.
- Adding your own `virtual` methods to your subclass is safe — they land in slots *after* the inherited ones. What is NOT safe: reordering, removing, or re-declaring the inherited virtuals, inserting virtuals into the base class, or hand-rolling a lookalike base class.
- Your class layout must come from the real header so `knobs[]`/`switches[]`/`stompSwitch` sit at the byte offsets the host expects — this is exactly what the stale `examples/dsp.hpp` used to break.

---

## `dsp_version` semantics

The version string controls how the host feeds audio to your effect:

| `dsp_version` state | Host behavior |
|---|---|
| Symbol missing | **Legacy path.** The .so is treated as v1: `compute()` is called in 16-sample chunks (the final chunk of a buffer may be shorter). |
| `"2.0.0"` (exact) | **Full-buffer path.** One `compute()` call per audio callback (production: 128 frames), *and* the module-level `extern "C" setSampleRate(int)` is called once with 44100 at load. |

Export `DSP_VERSION` from the header (which is `"2.0.0"`) and both behaviors are what you want.

> **Gotcha:** don't invent version strings. The two rows above are the only supported states: no symbol, or the exact string `"2.0.0"`. Anything else is unsupported — export `dsp_version = DSP_VERSION` verbatim and put your own release number in the `version` member instead.

> **Warning:** the dangerous failure is exporting *neither* on purpose nor by
> accident. If your build system compiles with hidden symbol visibility
> (JUCE and many CMake setups default to `-fvisibility=hidden`), your
> `dsp_version` definition can silently vanish from the dynamic symbol table —
> and a plugin **built against the current `dsp.hpp`** then gets driven through
> the legacy v1 interface. On-device symptoms: the effect can't engage, the
> app's stomp switch flips right back off, or audio is silent/garbled.
> Mark the entry points with default visibility:
>
> ```cpp
> extern "C" __attribute__((visibility("default"))) dsp* create() { return new MyEffect(); }
> extern "C" __attribute__((visibility("default"))) const char* dsp_version = DSP_VERSION;
> ```
>
> and verify before uploading — you want `T create` and `D dsp_version`:
>
> ```bash
> nm -D my_effect.so | grep -E ' (create|dsp_version)$'
> ```
>
> The FX Builder warns at upload when either symbol is missing.

Either way, **never assume a fixed block size**. `count` is usually 128 in production, but your `compute()` must be correct for any value from 1 to 1024: legacy chunking uses 16, other hosts and modes vary, and `benchmark()` passes enormous counts. If your algorithm needs fixed-size frames internally, chunk `count` yourself — never write `count` samples into a fixed-size stack array.

---

## The `getExtensions` rule

`getExtensions` is a marker symbol emitted only by the FAUST production pipeline. Its architecture wrapper historically generated self-contained classes without `instanceConstants()` in the vtable (newer pipeline versions add a no-op stub), so the firmware unconditionally skips the call for any .so exporting `getExtensions`. The firmware probes for it at load:

- **Present** ⇒ the .so is treated as FAUST-generated, and the host **skips calling `instanceConstants()` entirely** (on a vtable without `instanceConstants()`, calling slot 8 would misdispatch into `compute()` with garbage arguments).
- **Absent** ⇒ hand-written effect; `instanceConstants()` is called at every chain assembly.

The rule for you:

- **Hand-written C++: do NOT export any symbol named `getExtensions`.** If you do, your `instanceConstants()` will never run, your effect will start with stale pooled state, and any file/table setup you put there never happens.
- Hand-written C++ **must** do its reset and setup work in `instanceConstants()` (not only in the constructor), because that is the hook the host actually calls on reuse.
- FAUST developers: your generated code exports it correctly; don't touch it. See [guide-faust.md](guide-faust.md).

---

## Lifecycle: pooling, never-run destructors, and `instanceConstants()`

The host's instance lifecycle is unusual, and it dictates how you must structure initialization:

1. **First use of your effect:** the .so is `dlopen`ed (lazily, at first preset use — not at boot) and `create()` constructs an instance. The handle is cached for the lifetime of the firmware process. Avoid file-scope objects with non-trivial constructors — their initializers run inside `dlopen` itself, *before* `create()`, where nothing of yours can catch a failure; keep file-scope data const/POD and do all setup in the constructor or `instanceConstants()`.
2. **Chain assembly** (every preset load that includes your effect): the host takes an instance — **from a per-effect pool if one exists** — then calls, in order: `instanceConstants()` → `setKnob()` for each knob → `setSwitch()` for each switch → `setStompSwitch()`. Then audio starts flowing through `stompSwitchPressed()`.
3. **Preset switch away from your effect:** the instance is returned to the pool. **It is never deleted.** `~dsp` is non-virtual, so the firmware deliberately never destroys instances — your destructor will never run in production.

Consequences:

- **`instanceConstants()` is your only reset hook, and it must be a *full* reset.** The instance you receive may be carrying delay lines full of last preset's audio, envelope followers mid-decay, counters mid-count. Reset every piece of internal DSP state deterministically. (It does not need to reset `knobs[]` — the host overwrites them immediately after — but treat its job as "reset DSP state", not "reset parameters".)
- **Never rely on the destructor for anything.** No flushing, no saving, no freeing you actually depend on.
- **Do all allocation in the constructor / `instanceConstants()`,** never in `compute()` — the audio thread is a Xenomai real-time thread and any allocation, lock, I/O, or syscall there causes mode switches and audible dropouts. Full rules in [rt-safety.md](rt-safety.md).
- **Avoid mutable file-scope/static state.** Pooled instances of the same .so can be computed from more than one thread (the trails worker keeps retired instances running); shared statics between instances have corrupted the heap in the field. Keep all state inside the class.
- `instanceConstants()` is never called concurrently with `compute()` on the same instance — you may safely rebuild large state there.

**Checkpoint:** you can answer "what happens to my effect when the user switches presets twice and comes back?" — answer: same object, no destructor, one `instanceConstants()` call per return, knobs re-set by the host each time.

---

## Helper macros

`resources/dsp.hpp` ships five small macros. Use them instead of reinventing mappings:

```cpp
/* Convert a value in dB's to a coefficent */
#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)
#define CO_DB(v) (20.0f * log10f(v))

/* Define a macro to scale % to coeff */
#define PC_CO(g) ((g) < 100.0f ? (g / 100.0f) : 1.0f)

/* Define a macro to re-maps a number from one range to another  */
#define MAP(x, in_min, in_max, out_min, out_max) (((x - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min)

/* Define a macro to implement a selector out of a knob */
#define SELECTOR(knob, n) ((uint8_t)(knob * n) % n)
```

**`DB_CO` / `CO_DB`** — dB to linear coefficient and back, with a -90 dB floor that snaps to true zero (useful for "off at minimum" volume knobs):

```cpp
float outGain = DB_CO(MAP(validated(knobs[1]), 0.0f, 10.0f, -90.0f, 0.0f));
// knob at 0 -> silence (exact 0.0f), knob at 10 -> unity
```

**`MAP`** — linear range remap, the workhorse for turning the app's 0–10 knob convention into real units:

```cpp
float cutoffHz = MAP(validated(knobs[2]), 0.0f, 10.0f, 200.0f, 8000.0f);
```

(For frequency-like parameters an exponential mapping usually *feels* better; `MAP` into an exponent is a common trick: `powf(2.0f, MAP(k, 0.f, 10.f, log2f(200.f), log2f(8000.f)))`.)

**`PC_CO`** — percent (0–100) to coefficient (0–1), clamped at the top.

**`SELECTOR`** — turn a continuous knob into an `n`-position selector. It expects a **normalized 0–1** input, so divide the 0–10 knob first:

```cpp
// 3-position mode selector from knobs[3] (0-10 convention)
int mode = SELECTOR(std::min(validated(knobs[3]) / 10.0f, 0.999f), 3);
// mode is 0, 1, or 2
```

> **Gotcha:** by construction, `SELECTOR(1.0f, n)` wraps to 0 (`(uint8_t)(1.0*n) % n == 0`) — a knob at exactly full clockwise would select position 0 again. Clamp the normalized input just below 1.0 (as above) so the top of the knob's travel selects the last position.

---

## Contract summary (print this)

- Include `resources/dsp.hpp`, subclass `struct dsp`, override `instanceConstants()` and `compute()`. Never modify the header, never use a lookalike.
- Export exactly: `create()` (mandatory), `dsp_version = DSP_VERSION` (data symbol), optionally `setSampleRate(int)`; `destroy()` by convention. Never export `getExtensions` from hand-written code.
- `compute()` is in-place, mono, 44.1 kHz, any `count` from 1 to 1024. Doing nothing is bypass.
- `UP = 0` is bypassed, `DOWN = 1` is engaged. Trust the enum, not `getTextForEnum()`.
- Validate every knob; recompute coefficients on change only.
- `instanceConstants()` = full DSP-state reset on a pooled, dirty instance; destructors never run.
- Build with the repo's flag set, including `-fno-finite-math-only`, inside the container.

Next: [runtime-environment.md](runtime-environment.md)
