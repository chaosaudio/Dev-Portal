# Verification & Ship-Readiness

This page is the test plan you run between "it compiled" and "I'm submitting it." It proves four things, in order: your `.so` **loads**, it **sounds right**, it **fits the CPU budget** on real hardware, and it **survives abuse** (garbage knob values, preset churn, odd block sizes, silence). It applies identically to Stratus and Nimbus — same CPU, same firmware, same effect format; every command shown for `stratus.local` works the same way on a Nimbus.

You should already have a built binary in `bins/` (see [build-docker.md](build-docker.md)) and know how to copy it to the device (see [deploy-to-hardware.md](deploy-to-hardware.md)).

---

## 1. Does it load?

The firmware loads effects with `dlopen(RTLD_NOW)` from `/opt/update/sftp/firmware/effects/<EFFECT-ID>.so`. `RTLD_NOW` means **every** undefined symbol must resolve at load time — one missing symbol and the effect silently vanishes from the chain. The only evidence is a line in the device journal. So check symbols *before* deploying, then confirm the load *on* the device.

### 1a. Symbol sanity with `nm -D`

Run this inside the build container (the `.so` is an ARM binary; the container's GNU `nm` reads it natively):

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
```

```bash
nm -D bins/<YOUR-EFFECT>.so | grep -wE 'create|dsp_version|getExtensions'
```

- `<YOUR-EFFECT>.so` — the binary your build produced in `bins/` (e.g. `equalizer.so`).

**Expected output** (addresses will differ):

```
000108a4 T create
00021048 D dsp_version
```

Read it like this:

| Symbol | Required type | Meaning |
|---|---|---|
| `create` | `T` (code) | Mandatory factory. **Load fails without it.** |
| `dsp_version` | `D` (data) | Must be a *data* symbol — an exported `const char*` variable set to `"2.0.0"`. Missing → firmware treats your effect as legacy v1 and calls `compute()` in 16-sample chunks. |
| `getExtensions` | absent (hand-written C++) | This symbol is the FAUST-pipeline marker. If a hand-written effect exports it, the firmware **skips calling `instanceConstants()`** and your reset/setup code never runs. |

> **Gotcha:** if `dsp_version` shows up as `T`, you wrote `const char* dsp_version()` (a function) instead of `extern "C" const char* dsp_version = DSP_VERSION;` (a variable). The firmware reads the symbol as a `const char**` and dereferences it — a function there means it reads code bytes as a string and your effect gets misclassified. The correct form is in the ABI page: [dsp-contract.md](dsp-contract.md).

While you're in there, check for unresolved math symbols — the classic loader killer:

```bash
nm -D bins/<YOUR-EFFECT>.so | grep -i finite
```

**Expected output:** nothing. If you see undefined `__expf_finite` / `__logf_finite`-style references, you built with `-ffast-math` but without `-fno-finite-math-only`; the device's libm doesn't export those and `dlopen(RTLD_NOW)` will fail. See [build-flags-reference.md](build-flags-reference.md).

Finally, check that you haven't linked any shared library the device doesn't ship:

```bash
readelf -d bins/<YOUR-EFFECT>.so | grep NEEDED
```

**Expected output:** only `libm.so.6`, `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1` (plus the dynamic loader). Anything else — `libgomp`, `libfftw3`, an extra library pulled in by a framework — must be statically linked into your `.so` or removed, or `dlopen(RTLD_NOW)` fails on the device. The full dependency and symbol-version checks (GLIBC/GLIBCXX/CXXABI ceilings) are in [build-flags-reference.md](build-flags-reference.md).

**Checkpoint:** `nm -D` shows `T create` and `D dsp_version`, no `getExtensions` (for hand-written C++), no `*_finite` undefineds, and no unexpected `NEEDED` libraries.

### 1b. On-device load check via the journal

Deploy the effect ([deploy-to-hardware.md](deploy-to-hardware.md) has the full `scp`/`chown`/restart sequence), then watch the loader in real time. SSH uses the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it).

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Now select a preset containing your effect from the app. Watch for:

- **Good:** a line referencing your effect / "Effect found" as the chain assembles, and audio passes.
- **Bad:** a `dlerror` message (undefined symbol, GLIBC version mismatch) followed by `Effect does not exist.` — the chain simply skips your effect, the app shows no error, and bypassed-sounding audio is your only other clue.

> **Gotcha:** a GLIBC version complaint in `dlerror` (e.g. `GLIBC_2.3x not found`) means you built outside the provided container on a newer distro. Build inside the container — it pins the glibc floor that production units ship. See [build-docker.md](build-docker.md).

**Checkpoint:** the journal shows your effect loading with no `dlerror`, and you hear it processing.

---

## 2. Does it sound right? (offline correctness)

### 2a. `benchmark-plugin` in the container

The repo ships a `dlopen` harness at `tests/benchmark_plugin.cpp`. The Docker build installs it to `bins/tests/benchmark/benchmark-plugin`. It loads your `.so` the same way the firmware does (`dlopen(RTLD_NOW)`, `dsp_version` probe, `dlsym("create")`), then calls `setSampleRate()`, `instanceConstants()`, and finally `benchmark(seconds)`, which feeds uniform random noise through `compute()` and reports a ×-real-time figure. Two deliberate differences from the firmware: the harness calls the instance member `setSampleRate()` (the firmware calls the module-level `extern "C"` export instead), and where the firmware falls back to legacy v1 chunking on a missing `dsp_version`, the harness refuses to run and exits with an error.

Run it inside the build container:

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
```

```bash
cd bins
./tests/benchmark/benchmark-plugin /workdir/bins/<YOUR-EFFECT>.so 2 44100
```

- `<YOUR-EFFECT>.so` — your built plugin. The arguments are `<path_to_shared_object> <seconds> <sample_rate>`.

**Expected output:**

```
Loading library: /workdir/bins/<YOUR-EFFECT>.so
Loading symbol: create
Creating instance
Setting sample rate: 44100
Invoking instanceConstants method
Invoking benchmark method
Starting benchmark
Generating input signal
Computing 2 seconds of data @44100Hz
Processed 2 seconds of signal in 0.038303 seconds
52.214863 x real-time
Deleting instance
```

What this run actually proves:

- **Loads and runs** — it exercises the same `RTLD_NOW` + `dlsym("create")` + `dsp_version` path the firmware uses.
- **Relative cost** — run it on two builds of your effect (before/after an optimization) and compare the ×-real-time numbers.

> **Warning:** the container runs under QEMU emulation, not on a Cortex-A8. The absolute ×-real-time number is **meaningless** for the device — treat it strictly as pass/fail plus a relative comparison between builds. `52x real-time` under QEMU tells you nothing about whether you fit the 2.9 ms budget on hardware. Real numbers come from Section 3.

> **Gotcha:** `benchmark-plugin` assumes a `resources/dsp.hpp` subclass. The base-class `benchmark()` (see `resources/dsp.hpp`) calls `compute()` **once** with `sampleRate × seconds` samples — 88 200 samples for a 2-second run. That is far beyond anything the firmware ever sends, so a crash here usually means a fixed-size internal buffer that also can't handle odd block sizes. FAUST-architecture binaries built by the production pipeline don't implement `benchmark` (it's the last vtable slot); don't point this tool at them — verify FAUST effects with the WAV harness below or on-device.

**Checkpoint:** `benchmark-plugin` completes with a ×-real-time line and no crash.

### 2b. Offline WAV regression harness

For real correctness you want to process actual audio and compare against a known-good render. Write a small host harness — the same `dlopen` pattern as `tests/benchmark_plugin.cpp`, but driving `compute()` with a WAV file in production-sized blocks. The repo vendors `dr_wav` at `modules/dr_libs` (a submodule — run `git submodule update --init --recursive` if it's empty).

Pattern sketch (`tests/wav_regress.cpp` — adapt as needed):

```cpp
#define DR_WAV_IMPLEMENTATION
#include "../modules/dr_libs/dr_wav.h"
#include "dsp.hpp"          // resources/dsp.hpp
#include <dlfcn.h>

int main(int argc, char** argv) {
    // usage: wav_regress <plugin.so> <in.wav> <out.wav>
    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    auto create = reinterpret_cast<dsp_creator_t>(dlsym(h, "create"));
    dsp* fx = create();
    fx->setSampleRate(44100);
    fx->instanceConstants();
    for (int k = 0; k < MAXKNOBS; ++k) fx->setKnob(k, 5.0f);  // noon
    fx->setStompSwitch(dsp::DOWN);                            // engaged

    unsigned int ch, sr; drwav_uint64 n;
    float* buf = drwav_open_file_and_read_pcm_frames_f32(argv[2], &ch, &sr, &n, nullptr);
    // Host contract: mono, 44.1 kHz, IN-PLACE — pass the same pointer as in and out.
    const int BS = 128;                       // production block size
    for (drwav_uint64 i = 0; i < n; i += BS) {
        int count = (int)((n - i) < BS ? (n - i) : BS);
        fx->compute(count, buf + i, buf + i); // in == out, like the firmware
    }
    // ... write buf to argv[3] with drwav, then diff against a golden render
}
```

Key points the sketch encodes (all from the host contract, [dsp-contract.md](dsp-contract.md)):

- **In-place:** the firmware passes the same buffer as input and output. Test that way, or you'll miss aliasing bugs.
- **Mono, 44 100 Hz, float32** — convert your test WAV first if needed.
- **Block loop, not one giant call** — 128 is production; Section 4c reruns this loop at other sizes.

Workflow: render once with a build you've listened to and trust, save that output as `golden.wav`, and have CI (or you) diff every future render against it sample-by-sample within a small tolerance (e.g. 1e-6, or exact if you change nothing). Any DSP change re-baselines the golden deliberately, never accidentally.

**Checkpoint:** your effect renders a WAV offline, output matches your golden render, and it sounds right when you listen to it.

---

## 3. What does it cost? (CPU on real hardware)

One Cortex-A8 core runs your effect, every other effect in the chain, Bluetooth, LEDs, and the OS. The audio callback gets 128 frames every 2.9 ms (128 / 44 100 Hz), and the **whole chain** shares that 2.9 ms. QEMU numbers don't transfer; measure on the device.

### 3a. Read `bela-audio` CPU from `/proc/xenomai/sched/stat`

The real-time audio thread's CPU usage is visible in Xenomai's scheduler stats. Measure the *delta* your effect adds:

1. Load a preset **without** your effect, play a signal through it, and read:

```bash
ssh root@stratus.local 'cat /proc/xenomai/sched/stat'
```

Find the row whose NAME is `bela-audio` and note its `%CPU` column.

2. Load the same preset **with** your effect added (or your effect alone vs. an empty chain), same signal, read again.
3. Your effect's cost = the difference between the two `%CPU` readings. Take each reading a few times and average — it fluctuates a little with what the control threads are doing.

**Budget guidance:**

- A single effect should stay under **~15% of the core**. Chains stack: users run 4-6 effects, and the heaviest first-party effect (NAM amp modeling) already uses ~40% and dominates any preset it's in. An effect that costs 25% on its own will underrun the moment it shares a chain with an amp model.
- Sanity-check the math: your `%CPU` delta ≈ (your per-block cost / 2.9 ms). A measured reference point from an internal effect: +4.2% CPU added to a ~50% chain, worst-case block cost at 5.5% of the 2.9 ms budget, and zero underruns over a 5-minute soak — comfortably shippable.

### 3b. The stop-`bela_startup`-first rule

If you run **any** userspace benchmark on the device (e.g. `benchmark-plugin` copied over, or your own timing harness), the Xenomai audio thread preempts it constantly and your numbers read **2-3× too high** (verified on hardware). Stop the audio service first:

```bash
ssh root@stratus.local 'systemctl stop bela_startup'
```

Run your benchmark, then restore:

```bash
ssh root@stratus.local 'systemctl start bela_startup'
```

> **Warning:** stopping `bela_startup` kills audio until you restart it. Don't forget the second command, and never benchmark this way during a session where you also need to hear the pedal.

### 3c. Watch for underruns under load

CPU averages hide spikes. The failure you actually care about is a missed 2.9 ms deadline, which the firmware logs. While playing through your effect — worst-case settings, hard input, plus knob twisting — watch:

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f' | grep -i underrun
```

**Expected output:** nothing. Any `Underrun detected` line while your effect is in the chain at realistic settings is a fail — profile the spike (denormals and per-block allocation are the two historical causes; see [rt-safety.md](rt-safety.md)).

**Checkpoint:** you have a measured `%CPU` delta for your effect (< ~15%), and a play session at worst-case settings produced zero underrun lines.

---

## 4. Robustness drills

Each drill targets a real failure mode observed in the field. Run all five before submitting.

### 4a. Knob sweeps — including garbage values

Knob values arrive **raw and unclamped** from presets and BLE. Real production presets have contained `128` where 0-10 was expected. `setKnob()` also runs on the controller thread *while* `compute()` runs on the audio thread, so values change mid-buffer.

- **On device:** with the 9 KNOB tester ([deploy-to-hardware.md](deploy-to-hardware.md)), sweep every knob slowly end-to-end, then wiggle them fast, while playing. Listen for zipper noise, clicks, NaN blowups (sudden full-scale noise or dead silence that doesn't recover).
- **In the harness:** extend the Section 2b loop to hammer `setKnob` between blocks with hostile values:

```cpp
const float evil[] = { 0.f, 10.f, 128.f, -3.f, 1e9f,
                       std::numeric_limits<float>::quiet_NaN() };
for (float v : evil)
    for (int k = 0; k < MAXKNOBS; ++k) {
        fx->setKnob(k, v);
        fx->compute(128, buf, buf);   // must not crash, hang, or emit NaN/inf
    }
```

Pass bar: no crash, no hang, and output stays finite (scan the output buffer for NaN/inf after each block). Your effect should clamp or default every knob read — see [dsp-contract.md](dsp-contract.md).

### 4b. Rapid preset switches (pooled-instance reset)

The firmware pools effect instances per effect ID and **never destroys them** — your destructor never runs. At every chain assembly, `instanceConstants()` is called on a possibly-dirty pooled instance and must fully reset all DSP state (filters, delay lines, envelopes).

- **On device:** build two presets that both contain your effect (different knob settings). Flip between them ~20 times in quick succession while playing. Listen for stale tails, leftover filter state, parameter "memory" from the previous preset, or a crash in the journal.
- **In the harness:** render your WAV, call `fx->instanceConstants()` again (then re-set knobs), render again, and byte-compare the two outputs. Any difference means state leaked through your reset.

### 4c. Block-size tolerance

`count` is usually 128 in production, but legacy chunking uses 16, and other hosts and modes vary — your `compute()` must handle **any** count from 1 to 1024 without overruns. Rerun the Section 2b harness with the block-size loop parameterized:

```cpp
for (int bs : {16, 64, 128, 256}) {
    fx->instanceConstants();
    /* ... re-set knobs, then the same compute loop with BS = bs ... */
}
```

Add a couple of hostile sizes (`1`, `17`, `1024`) for good measure. Pass bar: no crash under ASan/valgrind on your host build, and each block size produces sane audio. (If your DSP is time-variant the renders won't be bit-identical across block sizes — that's fine; crashes and obvious artifacts are not.)

### 4d. Silence and decay tails (denormals)

Decaying filter/reverb/delay tails drift into subnormal floats, which cost 10-100× per operation on a Cortex-A8 — the historical symptom is "CPU randomly spikes when the guitar goes quiet." The firmware sets flush-to-zero on the audio thread and the standard build flags include `-ffast-math`, so on-device you're mostly protected — but don't rely on it if you target other hosts, and verify anyway:

- **On device:** play a loud burst, mute the input, and watch `bela-audio %CPU` (Section 3a) for the next 30-60 seconds. It must not climb as the tail decays.
- **In the harness:** feed a burst followed by 30 seconds of zeros and time the `compute()` calls per block — per-block time must stay flat through the tail.

If it spikes: add a tiny DC offset (~1e-7) to feedback paths or snap values below ~1e-6 to zero on long decays. See [rt-safety.md](rt-safety.md).

### 4e. Long soak

Leave the effect engaged on the device with a continuous signal (a looper phrase works well) for **at least 30 minutes**, journal streaming in another terminal:

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Pass bar: zero `Underrun detected` lines, no restarts of the service, no drift in `bela-audio %CPU` between the start and end of the soak. This catches slow leaks, accumulating state, and rare-path spikes that a 2-minute test never sees.

**Checkpoint:** all five drills pass with no crashes, no underruns, no NaN output, and no state leakage across resets.

---

## 5. Ship-readiness checklist

Run down this table before you submit. Every row should be a yes.

| # | Check | How | Pass bar |
|---|---|---|---|
| 1 | Required symbols exported | `nm -D` + `readelf -d` in container (§1a) | `T create`, `D dsp_version`; no `getExtensions` from hand-written C++; no `*_finite` undefineds; no unexpected `NEEDED` libraries |
| 2 | `dsp_version` is `"2.0.0"` exactly | Source + `benchmark-plugin` doesn't warn | Full-buffer path active; `setSampleRate` called at load |
| 3 | Loads on device | `journalctl -u bela_startup -f` during preset load (§1b) | Effect loads, no `dlerror`, audio passes |
| 4 | Runs under harness | `benchmark-plugin <so> 2 44100` in container (§2a) | Completes, no crash (QEMU number = pass/fail only) |
| 5 | Correct output | WAV regression vs golden render (§2b) | Matches golden within tolerance; sounds right |
| 6 | Block-size tolerant | Harness at 16/64/128/256 (+1/17/1024) (§4c) | No crash/overrun at any count 1-1024 |
| 7 | Garbage-knob safe | Sweep incl. `128`, negative, `NaN` (§4a) | No crash, no hang, output stays finite |
| 8 | Reset is complete | Double-render + `instanceConstants()` compare; 20× preset flips (§4b) | Identical renders; no stale state on device |
| 9 | Denormal-safe tails | Burst-then-silence CPU watch (§4d) | Flat CPU through the decay |
| 10 | CPU budget | `/proc/xenomai/sched/stat` delta (§3a) | Effect adds < ~15% of the core |
| 11 | No deadline misses | Journal grep during worst-case play + knob twisting (§3c) | Zero `Underrun detected` |
| 12 | Soak clean | 30+ min engaged with signal (§4e) | Zero underruns, no CPU drift, no restarts |
| 13 | Bypass intact | Toggle stomp while playing | Bypassed = clean dry signal (buffer untouched) |

When every row passes, your effect is ready for submission.

Next: [troubleshooting.md](troubleshooting.md)
