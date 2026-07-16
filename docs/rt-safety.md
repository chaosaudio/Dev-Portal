# Real-Time Safety

This page is the rulebook for writing `compute()` code that never glitches, never drops out, and never crashes the audio engine on Stratus or Nimbus. It explains **why** each rule exists — the Xenomai audio thread, mode switches, denormals, and the concurrency model — so you can reason about cases the rules don't spell out. Read it before you write your first line of DSP; come back to the [pre-flight checklist](#pre-flight-checklist) before every release.

If you haven't read [the DSP contract](dsp-contract.md) and [the runtime environment](runtime-environment.md) yet, start there — this page assumes you know what `compute()`, `instanceConstants()`, and the `dsp` base class are.

---

## 1. Where your code runs: the Xenomai audio thread

Stratus and Nimbus run 32-bit armhf Linux with the **Xenomai** real-time framework on a single ARM Cortex-A8 core. Xenomai is a *dual-kernel* system: a small real-time co-kernel runs alongside Linux and schedules real-time threads with hard priority over everything Linux does. The audio callback thread (`bela-audio`) is one of those real-time threads.

A Xenomai thread can be in one of two modes:

- **Primary mode** — scheduled by the real-time co-kernel. Deterministic, microsecond-level latency. This is where audio must stay.
- **Secondary mode** — the thread made a regular Linux syscall (allocated memory that page-faulted, printed to the console, touched a file, took a contended lock…), so it got demoted to the ordinary Linux scheduler until the call finishes. Latency is now whatever Linux feels like — milliseconds, sometimes worse.

Every demotion is called a **mode switch**. The audio thread has **2.9 ms** to process each 128-frame block at 44 100 Hz — and that 2.9 ms is shared by *every* effect in the chain, plus the chain plumbing itself. A single mode switch in your `compute()` can eat the entire budget. The result is an audible dropout (an "underrun"). Do it every block and the pedal crackles constantly. Page-fault into an unmapped page at the wrong moment and you can crash the whole audio engine — taking every effect down with you, on stage.

The firmware's own history is the cautionary tale: an early logging call (`spdlog`) from the audio path caused a hard SIGSEGV, and a heap allocation from the audio callback is documented in the firmware source as "a guaranteed Xenomai mode switch." These are not theoretical failure modes.

**Which of your methods run on the audio thread?**

| Method | Thread | RT rules apply? |
|---|---|---|
| `compute()` | Audio (Xenomai primary) | **YES — all of them** |
| `stompSwitchPressed()` (if you override it) | Audio | **YES** |
| Constructor | Controller (first load of your `.so`) | No — allocate freely |
| `instanceConstants()` | Controller (every chain assembly) | No — but keep it fast and idempotent |
| `setKnob()` / `setSwitch()` / `setStompSwitch()` | Controller (BLE), concurrent with `compute()` | Keep them trivial — the base-class versions just store a value; don't add work here |

`instanceConstants()` is never called while your `compute()` is running (the host assembles the new chain before publishing it to the audio thread), but `setKnob()` **is** called while `compute()` runs — see [torn reads](#6-knobs-are-written-while-compute-runs-torn-read-tolerance).

> **Warning:** Everything below about `compute()` applies equally to `stompSwitchPressed()` if you override it, and to any function either of them calls. The audio thread doesn't care which of your functions committed the crime.

---

## 2. Forbidden in `compute()`

Each entry says *what*, then *why*. "Why" is always one of: mode switch (dropout), unbounded time (dropout), or crash.

### 2.1 Heap allocation — including the hidden ones

`new`, `delete`, `malloc`, `free` — any of them can take the allocator's internal lock, make an `mmap`/`brk` syscall, or page-fault on fresh memory. All three are mode switches; the lock can additionally block you behind a non-real-time thread. Worse, allocation cost is *unbounded*: it's fast a thousand times in a row and then stalls for milliseconds once.

The direct calls are easy to avoid. The hidden ones ship dropouts to production:

| Hidden allocation | What actually happens |
|---|---|
| `std::vector::push_back` / `resize` past capacity | reallocates and copies the whole buffer |
| `std::string` construction, concatenation, `+`, `substr` | almost every operation allocates |
| `std::function` assigned from a capturing lambda | captures beyond a pointer or two are heap-allocated |
| `std::map` / `std::unordered_map` / `std::deque` insert | node or block allocation per insert |
| `std::shared_ptr` / `make_shared` copies | control-block traffic; destruction can free |
| Throwing an exception | the C++ runtime heap-allocates the exception object |
| `printf` / `std::cout` / logging | buffers, locks, *and* a `write()` syscall |

A real counter-example: an early in-house phase-vocoder prototype called `peakIndices.push_back(...)` and `outputQueue.push_back(...)` (a `std::deque`) inside its per-block processing path. It worked on a desktop. On the pedal, that pattern is a per-block mode-switch lottery. Don't copy it.

**Rule:** by the time `compute()` runs, every byte you will ever touch is already allocated. See [§3](#3-pre-allocate-everything-constructor-vs-instanceconstants).

### 2.2 Locks

`std::mutex`, `pthread_mutex_t`, spinlocks around anything non-trivial, condition variables. Taking a contended lock is a syscall (mode switch); worse, the thread holding it may be a *non*-real-time thread that Linux has preempted — the audio thread then waits on it at Linux's leisure (priority inversion). The firmware itself goes to great lengths to avoid this: the audio thread reads the effect chain through a lock-free snapshot, and the controller-side mutex is deliberately "invisible to the audio thread." Your effect must not reintroduce what the host engineered out.

If you need to hand data between `compute()` and anything else, use a single `float`/`int32_t` write (naturally atomic on ARMv7, see [§6](#6-knobs-are-written-while-compute-runs-torn-read-tolerance)) or a preallocated single-producer single-consumer ring buffer with atomic indices. If you think you need more than that, redesign.

### 2.3 File and console I/O

`printf`, `fprintf`, `std::cout`, `fopen`/`fread`, logging frameworks. Every one is a syscall → mode switch, plus internal locking, plus unbounded device latency. The pedal's own logger crashed the audio thread once (`spdlog` SIGSEGV, now memorialized in a firmware comment); yours will not do better. If you need files (IRs, wavetables), load them in `instanceConstants()` on the controller thread — that's the established pattern in first-party effects.

Debugging without printf: precompute values into a member array inside `compute()`, print it from `benchmark()` or from a host-side test harness offline. See [verification.md](verification.md) for offline test tooling.

### 2.4 Syscalls in general

`sleep`/`usleep`/`nanosleep`, `clock_gettime` variants that aren't VDSO-backed, `gettimeofday`, socket calls, `ioctl`, anything from `<thread>`. Any syscall demotes you to secondary mode. You don't need the time of day inside `compute()` — you have a perfect clock already: the sample counter. `count` samples pass per call; 44 100 pass per second.

### 2.5 Exceptions

Throwing allocates the exception object on the heap and runs the unwinder — table lookups, frame walking, unbounded time. Nothing above you on the audio thread catches: an uncaught throw from `compute()` terminates the audio process. And if you build with `-fno-exceptions` (common for audio plugins), a `throw` is an instant `std::terminate` — exceptions are fatal on the audio thread regardless of build flags.

Design errors out instead of throwing them: clamp bad inputs, fall back to a passthrough (`return;` — the in-place buffer already holds the dry signal, so doing nothing *is* bypass), or set a member flag that `instanceConstants()`/the app can react to. The equalizer example does exactly this:

```cpp
// examples/equalizer/equalizer.cpp — compute()
if (eq5Bands == nullptr) {
    return;   // not initialized: buffer already holds dry signal = clean bypass
}
```

---

## 3. Pre-allocate everything: constructor vs `instanceConstants()`

You have two non-real-time hooks, and they run at different times:

- **Constructor** — runs **once, ever**, per pooled instance, on the controller thread the first time the host `create()`s you. Instances are pooled and never destroyed (the base class destructor is non-virtual and the host never calls `delete` — see [dsp-contract.md](dsp-contract.md)), so whatever you allocate here lives for the lifetime of the process. This is the place for your big fixed buffers.
- **`instanceConstants()`** — runs on the controller thread at **every chain assembly** (every preset change that includes your effect), on a possibly *dirty* pooled instance that still holds state from the last preset. Its job is to **reset all DSP state** — filters, delay lines, envelopes, smoothers — to a deterministic starting point. Allocation is *legal* here (controller thread), but if you allocate, do it idempotently, because this method runs many times on the same object.

The equalizer example shows the idempotent-allocation pattern:

```cpp
// examples/equalizer/equalizer.cpp — instanceConstants()
if (eq5Bands != nullptr) {
    delete eq5Bands;
}
eq5Bands = new Eq5Bands(getSampleRate());
```

The simpler and better pattern for most effects: make buffers plain fixed-size members, allocate nothing at all, and have `instanceConstants()` just zero them:

```cpp
class MyEffect : public dsp {
    // Sized in the CLASS, allocated when the host news the instance (controller thread).
    float delayLine[44100];        // 1 s at the fixed 44.1 kHz rate
    float scratch[1024];           // fixed scratch — NEVER indexed by raw count; chunk count against
                                   // its size (count is 1..1024 from hosts, tens of thousands from benchmark())
    int   writeIdx = 0;

public:
    void instanceConstants() override {
        // Pooled instance may carry a previous preset's audio — silence it ALL.
        std::fill(std::begin(delayLine), std::end(delayLine), 0.0f);
        writeIdx = 0;
        // ...reset filters, envelopes, smoothers too.
    }
};
```

> **Warning:** Never write `count` samples into any fixed-size array — **chunk instead**. Hosts call `compute()` with *any* value from 1 to 1024 (production uses 128; legacy chunking uses 16), and the base class `benchmark()` calls it **once with tens of thousands of samples** — 88 200 for the 2-second run [verification.md](verification.md) has you perform — so no fixed buffer is "big enough". A fixed buffer indexed by `count` is only safe if you process `count` internally in slices no larger than the buffer (the fixed-chunk idiom in [runtime-environment.md](runtime-environment.md)). A `float buf[128]` written with `count` samples is a stack smash waiting in your own documented smoke-test step. (An early in-house prototype had exactly this bug: a 16-float frame buffer written with `count` samples.)

> **Gotcha:** Because instances are pooled, **your destructor never runs** and state persists across preset loads. Anything `instanceConstants()` forgets to reset — a filter's `z⁻¹`, a delay line, an envelope follower — becomes an audible ghost of the previous preset the next time yours loads. Reset *everything* except `knobs[]`/`switches[]` (the host sets those immediately after).

---

## 4. Parameter smoothing: killing zipper noise

Knob values jump. The user sweeps a knob and the app streams discrete values over BLE; each `setKnob()` lands as a step change. If you apply that value directly to a gain (or worse, a filter coefficient), each step is a discontinuity in the output waveform — audible as "zipper noise," a crunchy stair-step artifact.

The fix is to smooth the *applied* value toward the *target* value over a few milliseconds. Don't write your own — the repo ships an allocation-free, RT-safe smoother at [`common/ValueSmoother.hpp`](../common/ValueSmoother.hpp) (from the DISTRHO Plugin Framework). It provides two classes:

```cpp
// common/ValueSmoother.hpp
/**
 * @brief An exponential smoother for control values
 *
 * This continually smooths a value towards a defined target,
 * using a low-pass filter of the 1st order, which creates an exponential curve.
 */
class ExponentialValueSmoother { /* ... */
    inline float next() noexcept
    {
        return (mem = mem * coef + target * (1.f - coef));
    }
};

/**
 * @brief A linear smoother for control values
 *
 * This continually smooths a value towards a defined target, using linear segments.
 */
class LinearValueSmoother { /* ... */ };
```

Use the exponential one for gains and mixes (sounds natural), the linear one when you need guaranteed convergence in a fixed time. Wiring it up:

```cpp
#include "ValueSmoother.hpp"

class MyEffect : public dsp {
    ExponentialValueSmoother gainSmooth;

public:
    void instanceConstants() override {
        gainSmooth.setSampleRate((float)getSampleRate());
        gainSmooth.setTimeConstant(0.02f);        // 20 ms T60 — inaudible lag, no zipper
        gainSmooth.setTargetValue(currentGain());
        gainSmooth.clearToTargetValue();          // don't fade in from a stale pooled value
    }

    void compute(int count, float* in, float* out) override {
        // Snapshot + validate the knob ONCE per block (see §6), then retarget:
        gainSmooth.setTargetValue(currentGain());
        for (int i = 0; i < count; ++i)
            out[i] = in[i] * gainSmooth.next();   // per-sample smoothed value, zero allocations
    }

private:
    float currentGain() {
        float k = knobs[0];
        if (!(k >= 0.0f && k <= 10.0f)) k = 5.0f; // clamp/NaN-reject; see §6
        return k * 0.1f;
    }
};
```

Smooth anything that multiplies or offsets audio: gains, mix/blend, feedback amounts, pan. Filter *coefficients* generally shouldn't be recomputed per sample (too expensive — see [§8](#8-cpu-discipline)); instead smooth the underlying parameter and recompute coefficients only when the smoothed value has moved meaningfully, or crossfade between two filter states.

---

## 5. Denormals on Cortex-A8: the FTZ story

When a float decays toward zero — a reverb tail, a feedback delay fading out, a one-pole filter after the input goes silent — it eventually enters the **subnormal** (denormal) range, roughly below 1.2 × 10⁻³⁸. The Cortex-A8's VFP unit doesn't handle subnormals in hardware at full speed: each subnormal operation costs **10–100×** a normal one. The classic symptom, quoted from the firmware's own comments: *"CPU randomly spikes when the guitar goes quiet."* Your effect passes every bench test at full signal, then eats the whole 2.9 ms budget four seconds after the player mutes.

**What the platform already does for you:** the firmware sets the FPSCR **FZ** (flush-to-zero) and **DN** (default-NaN) bits on the audio thread before the first block, and again on the trail-rendering worker thread. FPSCR is per-thread CPU state, so your `.so`, running on those threads, inherits flush-to-zero: scalar VFP results that would be subnormal become 0.0 for free. Additionally, all builds use `-ffast-math` (see [build-flags-reference.md](build-flags-reference.md)), which on NEON-vectorized code keeps you in NEON's always-flush-to-zero arithmetic.

**Why you should still be defensive:** FTZ is a property of the *thread that happens to call you*, not of your code. Offline test harnesses, the `benchmark-plugin` tool under QEMU, future host modes, and other platforms you port to will not necessarily set it. Two cheap idioms make an algorithm denormal-proof everywhere:

```cpp
// 1) Tiny DC bias injected into recirculating paths — keeps values out of the
//    subnormal range entirely; -140 dB is far below audibility.
feedback = delayLine[readIdx] * fbAmount + 1.0e-7f;

// 2) Snap-to-zero on decaying state once it's inaudible:
env *= decayCoef;
if (env < 1.0e-6f) env = 0.0f;   // an envelope at -120 dB is done
```

Put one of these in every feedback loop, envelope, and IIR filter state that can decay freely. They cost one add or one compare per sample and remove an entire class of field bug.

---

## 6. Knobs are written WHILE compute() runs: torn-read tolerance

The controller (BLE) thread calls `setKnob()` / `setSwitch()` / `setStompSwitch()` on your live instance **while the audio thread is inside `compute()`**. There is no lock between them, by design (a lock would be an RT hazard, [§2.2](#22-locks)). The base class implementation is a bare store:

```cpp
// resources/dsp.hpp
virtual void setKnob(int num, float knobVal){
    this->knobs[num] = knobVal;
}
```

What this means for you:

- **You will never see half a float.** Aligned 32-bit loads and stores are single-copy atomic on ARMv7, so a concurrent `knobs[i]` read yields either the old or the new value, never a torn hybrid. That's the entire guarantee.
- **The value can change between any two reads.** If your loop reads `knobs[0]` twice, it may get two different values within one block. **Snapshot each knob into a local at the top of `compute()`** and use only the local:

```cpp
void compute(int count, float* in, float* out) override {
    const float mixKnob = knobs[1];   // ONE read per block — immune to mid-block changes
    // ... use mixKnob everywhere below; never touch knobs[1] again this block
}
```

- **Validate every value.** The firmware does **not** clamp knob values — they arrive raw from presets and BLE, and real-world presets have contained `128` where 0–10 was expected. The app convention is 0–10 with 5 ≈ noon, but treat that as a convention, not a guarantee. Clamp to your expected range and reject NaN before using anything:

```cpp
float k = knobs[0];
if (!(k >= 0.0f && k <= 10.0f))   // false for NaN too — NaN fails every comparison
    k = 5.0f;                     // or clamp: std::min(std::max(k, 0.0f), 10.0f)
```

(The `!(k >= lo && k <= hi)` form catches NaN because NaN compares false to everything. This works here because the toolchain builds with `-fno-finite-math-only`, which keeps NaN semantics honest despite `-ffast-math` — one more reason never to drop that flag; see [build-flags-reference.md](build-flags-reference.md).)

- **Don't add work to `setKnob()` overrides.** It runs on the controller thread, but keep it to storing values; do the derived-parameter math at the top of `compute()`, change-gated ([§8](#8-cpu-discipline)). Doing DSP reconfiguration inside `setKnob()` while `compute()` runs on the same state is a data race the float-atomicity guarantee does not cover.

---

## 7. No mutable file-scope or static state — ever

This rule is absolute and the reason is not stylistic. **The firmware can run two instances of your `.so` concurrently on different threads.** When "trails" are enabled (delay/reverb tails ringing out across a preset change), the retired chain's instance of your effect keeps getting `compute()`d on a low-priority trail-rendering worker thread *while* a fresh pooled instance of the same `.so` may be processing live audio on the audio thread.

Instance members are fine: each instance owns its own. But every `static` local, file-scope global, or class-`static` mutable variable is **shared between those two instances across two threads with no synchronization**. This is not hypothetical — concurrent computation touching plugin file-scope statics corrupted the heap and crashed the firmware in the field (the June-2026 trails SIGSEGV; the firmware now carries a same-effect exclusivity guard commemorating it). Don't be the next comment in that file.

```cpp
// WRONG — shared across every instance of your .so, raced by audio + trail threads:
static float lookupTable[4096];
static bool  tableInit = false;

void MyEffect::compute(int count, float* in, float* out) {
    if (!tableInit) { buildTable(lookupTable); tableInit = true; }   // data race
    ...
}

// RIGHT — per-instance member, built in a non-RT hook:
class MyEffect : public dsp {
    float lookupTable[4096];                 // each instance owns its own
public:
    MyEffect() { buildTable(lookupTable); }  // constructor: controller thread, runs once
};
```

`const` file-scope tables (`static const float coeffs[] = {...}`) are fine — immutable data can be shared freely. The rule is about *mutable* state. If a shared table is genuinely too large to duplicate per instance, build it in the constructor guarded so only initialization-time (controller-thread) code touches it — but the per-instance member is almost always the right answer at pedal-scale table sizes.

The same reasoning bans a related desktop habit: **never create threads** (`std::thread`, JUCE `Timer`/`ThreadPool`, async loaders). There is one core, the audio thread owns it, and a pooled instance's thread can never be shut down — destructors never run, so the thread contends with audio forever, once per pooled instance. Do everything in the constructor / `instanceConstants()` on the controller thread.

---

## 8. CPU discipline

The budget: one Cortex-A8 core at 1 GHz runs your effect, every other effect in the chain, Bluetooth, LEDs, and the OS. A single effect should stay under roughly **15% of the core**; the heaviest first-party effect (NAM, a neural amp model) uses ~40% and dominates any preset it's in. Measure on the device — see [verification.md](verification.md) for the `/proc/xenomai/sched/stat` procedure (and its gotchas).

### Recompute coefficients on change, not per sample

Transcendental functions (`powf`, `expf`, `tanf`) cost hundreds of cycles on the A8. A biquad coefficient update involves several. Doing that per *sample* multiplies it by 44 100; doing it per *block only when a knob actually moved* makes it free in the steady state. The equalizer example is the reference pattern — it caches the previous knob values and gates the recompute:

```cpp
// examples/equalizer/equalizer.cpp — compute()
if (knobs[0] != knobs_old[0]) {
    knobs_old[0] = knobs[0];
    eq5Bands->bass_boost_db = MAP(knobs_old[0], 0.0f, 10.0f, -8.0f, 8.0f);
    bass_has_changed++;
}
if (bass_has_changed) {
    eq5Bands->updateBass();     // expensive coefficient math ONLY on change
}
```

### Write loops the auto-vectorizer can eat

The release flags (already applied by the repo's CMake — see [build-flags-reference.md](build-flags-reference.md)) include `-mfpu=neon -ftree-vectorize -ffast-math -fno-finite-math-only`: GCC will turn clean loops into NEON SIMD (4 floats per instruction) with zero effort from you — *if* you write loops it can prove safe. The recipe:

- **Unit stride:** iterate `buf[i]`, `i++`, front to back. Strided or index-chasing access (`buf[idx[i]]`, `buf[i*3]`) defeats vectorization and thrashes the A8's cache.
- **Simple, countable loops:** `for (int i = 0; i < count; ++i)` with no `break`, no early `return`, no I/O, and no per-iteration branching on data if you can avoid it (use arithmetic selects: `y = a * mask + b * (1.0f - mask)`).
- **Hoist member and pointer loads into locals:** the compiler often can't prove `this->gain` or a member pointer doesn't alias the audio buffer, so it reloads them every iteration and gives up on SIMD. Copy to locals before the loop:

```cpp
void compute(int count, float* in, float* out) override {
    const float g        = gain_;    // hoisted: no re-load, no aliasing doubt
    const float* const x = in;
    float* const o       = out;
    for (int i = 0; i < count; ++i)  // unit stride, countable, branch-free:
        o[i] = x[i] * g;             // -> one NEON vmul per 4 samples
}
```

- **In-place is fine — but follow the portable rule:** on the device `in == out` (the host processes in place), and elementwise loops like the one above vectorize regardless. Test harnesses may pass *distinct* buffers, though, so always **read from `in[i]`, write to `out[i]`, and never read `out` before writing it** — code that reads through the output pointer silently processes zeros in a harness. And never assume the buffers are different, either.
- **Prefer per-block chunks of work** over per-sample everything: e.g., run your envelope follower at block rate if the algorithm tolerates it.
- **`-ffast-math` is already on** — don't re-derive slow "IEEE-exact" workarounds, and don't add your own `-ffast-math`-hostile tricks (bit-twiddling float hacks that assume strict IEEE ordering). Also remember its partner `-fno-finite-math-only` is load-bearing for *loading at all*, not just NaN checks: `-ffast-math` alone emits `__expf_finite`-style libm symbols the device lacks, and the effect then silently never appears (details in [troubleshooting.md](troubleshooting.md)).

### Verify on the device, not by feel

After deploying (see [deploy-to-hardware.md](deploy-to-hardware.md)), watch for underruns while you play:

```
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

(The same hostname and commands apply on Nimbus.) Expected output while healthy: routine startup lines and **no** occurrences of:

```
Underrun detected
```

If underruns appear only when you stop playing, suspect denormals ([§5](#5-denormals-on-cortex-a8-the-ftz-story)). If they appear when you twist a knob, suspect per-change work that's too heavy ([§8](#8-cpu-discipline)) or a hidden allocation on the knob path. Full measurement methodology: [verification.md](verification.md).

**Checkpoint:** you can articulate, for each rule above, which failure mode it prevents — mode switch, unbounded time, data race, or CPU spike. If a rule feels arbitrary, re-read its section; you'll need the *why* the first time you hit a case the rules don't literally cover.

---

## Pre-flight checklist

Run every row before you ship. "Where" tells you which section explains the why.

| # | Check | Where |
|---|---|---|
| 1 | No `new`/`malloc`/`delete`/`free` reachable from `compute()` — including `std::vector` growth, `std::string`, `std::function`, container inserts | §2.1 |
| 2 | No mutexes, condition variables, or blocking synchronization anywhere on the audio path | §2.2 |
| 3 | No `printf`/logging/file/socket I/O in `compute()`; file loading lives in `instanceConstants()` | §2.3 |
| 4 | No syscalls: no `sleep`, no clock/time calls, nothing from `<thread>` in the audio path | §2.4 |
| 5 | No `throw` reachable from `compute()`; bad states degrade to bypass (`return;`) or clamped fallbacks | §2.5 |
| 6 | Every buffer pre-allocated (constructor or fixed-size member); **never write `count` samples into any fixed array — chunk** (`count` is 1..1024 from hosts and tens of thousands from `benchmark()`) | §3 |
| 7 | `instanceConstants()` fully resets ALL DSP state (delays, filters, envelopes, smoothers) — pooled instances arrive dirty and destructors never run | §3 |
| 8 | Audible parameters (gain, mix, feedback) smoothed — `common/ValueSmoother.hpp`, targets cleared in `instanceConstants()` | §4 |
| 9 | Every feedback loop / decaying state is denormal-proof (DC bias or snap-to-zero) — don't rely solely on host FTZ | §5 |
| 10 | Each knob read **once per block** into a local, clamped to range, NaN-rejected — firmware never clamps for you | §6 |
| 11 | Zero mutable `static`/file-scope/global state, and no threads created anywhere — trails can run a second instance of your `.so` concurrently | §7 |
| 12 | Coefficients recomputed only on parameter change; hot loops unit-stride and vectorizer-friendly; measured under ~15% of the core on real hardware | §8 |

---

Next: [guide-native-cpp.md](guide-native-cpp.md)
