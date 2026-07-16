# The Runtime Environment

This page describes what the host firmware does *around* your effect: where your
code sits in the signal path, when each of your methods gets called, which thread
calls it, and how much CPU time you actually have. Read it after
[the DSP contract](dsp-contract.md) — that page defines the ABI your `.so` must
match; this one defines the behavior your code must survive.

Everything here applies identically to **Stratus** (the multi-effects pedal) and
**Nimbus** (the 100 W smart amp). Both run the same firmware on the same TI
AM335x single-core ARM Cortex-A8 @ 1 GHz, and both load the same `.so` files.
Build once, runs on both.

## The signal path: a mono, in-place chain

Audio on the device is **mono, float32, 44 100 Hz** — always. The firmware folds
the stereo codec input down to a single mono buffer, copies that dry signal into
the output buffer, and then runs every effect in the active preset **in preset
order**, each one processing the same buffer **in place**:

```
guitar → [mono fold] → [effect 0] → [effect 1] → ... → [effect N] → amp/output
                            ↑ all operating on ONE shared buffer, in place
```

Concretely, **on the device** your `compute()` is called with
`inputs == outputs` — they are the same pointer. Test harnesses may pass
distinct buffers: the base class's own `benchmark()` (`resources/dsp.hpp`,
lines 206–218) calls `compute()` with two separate vectors and a zero-filled
output. The portable rule that is correct in both worlds: **always read from
`inputs[i]`, always write to `outputs[i]`, and never read `outputs[i]` before
you have written it.** Two consequences:

1. **Read `inputs[i]` before you write `outputs[i]`.** On the device they are
   the same memory, so an algorithm that writes its output first and then
   re-reads its "input" is reading its own output — and in a harness with
   distinct buffers, anything read via `outputs` is silently processing zeros.
2. **An effect that touches nothing IS bypass.** Because the host pre-fills the
   buffer with the dry signal, leaving the buffer alone passes dry audio
   through. This is exactly how bypass works (see
   [Bypass and the stomp switch](#bypass-and-the-stomp-switch) below) — the host
   simply skips your processing and the pre-filled dry buffer flows on to the
   next effect.

> **Gotcha:** Do not zero the output buffer "to be safe" at the top of
> `compute()`. If your algorithm then fails to write every sample (early return,
> partial processing), you have replaced the dry signal with silence — and if
> you ever return early on purpose (e.g. a not-ready state), returning without
> touching the buffer is the correct dry-passthrough behavior.

The sample rate is fixed. `getSampleRate()` on the `dsp` base class returns
44100 (the default set in `resources/dsp.hpp`, line 48: `int fSampleRate =
44100;`), and for v2 plugins the firmware calls the module-level
`setSampleRate(int)` export exactly once at load time, always with `44100`.
Precompute your coefficients for 44.1 kHz and don't build a resampler you don't
need.

## Block size: 128 in production — but never assume it

The production audio callback runs at a period of **128 frames**, which at
44 100 Hz is one callback every **2.9 ms**. That is the number to keep in your
head for budgeting.

It is **not** a number you may hard-code. The `count` argument to
`compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs)` varies:

- **128** — the production block size.
- **16** — legacy chunking: if your `.so` does not export a `dsp_version` data
  symbol (or exports `"1.0.0"`), the firmware slices every block into 16-sample
  chunks, and the *final* chunk of a block may be smaller than 16.
- **Other values** — the period size is a firmware launch parameter, the trails
  renderer and offline tools call with their own sizes, and the base class's
  `benchmark()` method (`resources/dsp.hpp`, lines 200–222) calls `compute()`
  with `sampleRate * seconds` samples — tens of thousands in one call.

Write every loop count-agnostically. The correct idiom:

```cpp
void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override {
    for (int i = 0; i < count; ++i) {
        const float x = inputs[i];   // read first: inputs == outputs
        outputs[i] = processSample(x);
    }
}
```

If your algorithm genuinely needs a fixed internal frame size (an FFT hop, a
fixed SIMD width), chunk `count` against it instead of assuming they match:

```cpp
static constexpr int kFrame = 16;

void compute(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs) override {
    for (int offset = 0; offset < count; offset += kFrame) {
        const int n = std::min(kFrame, count - offset);
        processFrame(inputs + offset, outputs + offset, n);  // handles n < kFrame
    }
}
```

> **Warning:** A fixed-size stack buffer filled with `count` samples is a stack
> smash waiting to happen — it survives testing at one block size and crashes at
> another (or the moment anything calls `benchmark()`). Size buffers to a real
> maximum and chunk, or index directly off the passed-in pointers.

## The 2.9 ms budget — and who you share it with

There is **one** CPU core. It runs, at minimum:

- **Your effect** — plus *every other effect in the chain*. A preset is commonly
  4–6 effects; the whole chain must finish inside the same 2.9 ms callback.
- **The audio host itself** (a Xenomai real-time thread — see
  [RT safety](rt-safety.md) for why that makes syscalls in `compute()` fatal).
- **The controller/Bluetooth task** — the phone app talks to the device while
  audio runs; this is the thread that calls your `setKnob()` (next section).
- **The trails worker** — when an effect with trails support is swapped out, its
  retired instance keeps getting `compute()` calls on a low-priority thread so
  reverb/delay tails ring out.
- **LEDs, the looper, and the rest of Linux.**

Budget guidance: keep a single effect under roughly **15 % of the core**, and
measure the worst case, not the average — one over-budget block is an audible
dropout ("underrun"). Chains stack: the heaviest first-party effect (the NAM amp
model) takes ~40 % on its own, which is why it dominates any preset it's in.
How to actually measure on-device (and why naive benchmarks read 2–3× high) is
covered in [Verification](verification.md).

## How parameters arrive: the controller thread

Your effect's parameters live in public arrays on the `dsp` base class
(`resources/dsp.hpp`, lines 56–58):

```cpp
float knobs[MAXKNOBS];            // MAXKNOBS = 10
SWITCH_STATE switches[MAXSWITCHES]; // MAXSWITCHES = 5
SWITCH_STATE stompSwitch;
```

The constructor defaults every knob to `0.5` and every switch to `DOWN`. After
that, values come from the app (over Bluetooth) and from saved presets, via
`setKnob()` / `setSwitch()` / `setStompSwitch()`. Three facts about that path
matter enormously:

**1. Values are raw and unclamped.** The firmware passes whatever the app or
preset file contained straight into `setKnob()` — no range check, no NaN check.
The app's convention is **0–10, with 5 as "noon"**, but real-world presets have
delivered values like `128` where 0–10 was expected. The base-class `setKnob()`
(`resources/dsp.hpp`, lines 165–167) does no validation either:

```cpp
virtual void setKnob(int num, float knobVal){
    this->knobs[num] = knobVal;
}
```

**Validate every knob you read.** Clamp to your expected range (or fall back to
a sane default on NaN/out-of-range) before using the value in any coefficient
math. A `128` fed into a gain computed as `powf(10, knob)` is not a bug report,
it's a blown speaker.

**2. They arrive while audio is running.** `setKnob()` executes on the
controller thread *concurrently* with `compute()` on the audio thread. There are
no locks. A single aligned float store is atomic-enough on this ARM core, so you
won't see torn values — but a knob **can change between any two samples of a
block**. Your code must tolerate any knob value appearing at any time,
mid-buffer.

**3. Re-derive expensive state only on change.** Never recompute filter
coefficients per sample from `knobs[]`. Cache the last-seen value and recompute
only when it moved — the shipped equalizer example
(`examples/equalizer/equalizer.cpp`) shows the pattern:

```cpp
if (knobs[0] != knobs_old[0]) {
    knobs_old[0] = knobs[0];
    eq5Bands->bass_boost_db = MAP(knobs_old[0], 0.0f, 10.0f, -8.0f, 8.0f);
    bass_has_changed++;
}
if (bass_has_changed) {
    eq5Bands->updateBass();
}
```

(Add clamping on top of this pattern — the example trusts its input more than
you should.) If a stepped change clicks audibly, smooth the *derived* parameter
over a few ms; see [RT safety](rt-safety.md) for allocation-free smoothing.

## Bypass and the stomp switch

The host never calls your `compute()` directly during normal playback. It calls
`stompSwitchPressed()`, whose base-class implementation
(`resources/dsp.hpp`, lines 189–194) is the entire bypass mechanism:

```cpp
virtual void stompSwitchPressed(int count, FAUSTFLOAT* inputs, FAUSTFLOAT* outputs){
    if(stompSwitch){
        compute(count, inputs, outputs);
    }
    return;
}
```

Combined with the pre-filled in-place buffer, this means: when `stompSwitch` is
`UP` (bypassed), your `compute()` is simply skipped, nothing touches the buffer,
and the dry signal passes through. A bypassed effect costs effectively zero CPU.
It also means bypass state can flip from the controller thread at any moment,
between blocks — your effect gets no "you were just bypassed" callback, so
long-lived state (delay lines, envelopes) keeps whatever it had and resumes when
re-engaged.

The `SWITCH_STATE` enum (`resources/dsp.hpp`, lines 42–46) is used for the stomp
switch *and* the five toggle switches:

```cpp
enum SWITCH_STATE{
    UP = 0,       // stomp: BYPASSED
    DOWN = 1,     // stomp: ENGAGED
    MIDDLE = 2    // 3-position toggles only
};
```

Note the non-intuitive order: **UP = 0 = bypassed, DOWN = 1 = engaged.**

> **Warning:** `getTextForEnum()` in the same header (lines 79–89) prints these
> *wrong* — it maps `1 → "MIDDLE"` and `2 → "DOWN"`, the opposite of the enum
> it sits next to. Trust the enum values, never that debug helper, or you will
> wire a 3-way switch backwards and "confirm" it with its own misprint.

If you override `stompSwitchPressed()` (for example, to keep a tail ringing
after bypass), you **must** preserve the semantics: when `stompSwitch == UP`,
the buffer either stays untouched or ends up carrying what the user should hear
in bypass. Writing to the buffer before checking `stompSwitch` breaks bypass for
every preset containing your effect.

## Chain assembly: your lifecycle, in order

Whenever the user loads a preset (or edits the chain), the firmware assembles a
new effect chain on the controller thread and then swaps it in atomically under
the audio thread. Here is exactly what happens to your effect, in order:

1. **First use only — the `.so` is loaded.** `dlopen(RTLD_NOW)` from
   `/opt/update/sftp/firmware/effects/<EFFECT-ID>.so`, then `create()` is
   called to construct an instance. `<EFFECT-ID>` is the GUID the FX Builder
   assigns your effect — installing your published effect from the Chaos Audio
   app places the binary under that filename; see
   [Test on hardware](deploy-to-hardware.md).
   If `dsp_version` is exactly `"2.0.0"`, the module-level `setSampleRate(int)`
   export is called once, with `44100`. The library handle is cached forever.
2. **Every assembly — an instance is fetched from the pool.** Instances are
   **pooled per effect and never destroyed** (`~dsp` is non-virtual and never
   runs). The instance you receive may be carrying live state — filter
   histories, delay-line contents, envelopes — from the last preset that used
   it.
3. **`instanceConstants()` is called** (unless your `.so` exports
   `getExtensions`, the FAUST-pipeline marker — see
   [the DSP contract](dsp-contract.md)). This is your **only reset hook**.
   Because of step 2, it must deterministically reinitialize *all* internal DSP
   state. Treat it as "reset DSP state", not "reset parameters" — do not
   carefully preserve `knobs[]` logic here, because:
4. **`setKnob()` is called for every knob the preset carries, then
   `setSwitch()` for every switch, then `setStompSwitch()`** — the host
   overwrites parameters *after* `instanceConstants()`. (This is also why
   resetting `knobs[]` inside `instanceConstants()` happens to be harmless —
   but don't rely on it.)
5. **The chain is published** to the audio thread in one atomic swap, and
   `stompSwitchPressed()` starts arriving every block, in your effect's preset
   position.

> **Gotcha:** `instanceConstants()` runs on the controller thread at *every*
> chain assembly — every preset change, on a possibly-dirty pooled instance. An
> effect that only initializes "the first time" (guarded by a `bool`) will
> carry a previous preset's reverb tail or filter state into the new one.

**Checkpoint:** you can now answer, for any line of your code: *which thread
calls this, when, with what buffer, and what state can it trust?* If any answer
is "not sure", the two pages below are the ones to read.

## What this means for your code

| Host behavior | Your obligation |
|---|---|
| In-place buffer, pre-filled dry | Read before write; never blind-zero the buffer |
| `count` varies (128 prod, 16 legacy, huge in `benchmark()`) | Count-agnostic loops; chunk fixed frames |
| One core, 2.9 ms for the whole chain | Stay ~15 % of core worst-case; measure per [Verification](verification.md) |
| Knobs raw/unclamped from controller thread, mid-buffer | Clamp + NaN-check every read; recompute coefficients on change only |
| `UP = 0` = bypass; compute skipped while bypassed | Preserve semantics if overriding `stompSwitchPressed()` |
| Pooled instances, destructor never runs | `instanceConstants()` fully resets all DSP state |
| Audio thread is Xenomai RT | No allocation/locks/I/O in `compute()` — see [RT safety](rt-safety.md) |

Next: [RT safety](rt-safety.md)
