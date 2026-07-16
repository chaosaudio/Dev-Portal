# FAUST Effects Guide

This page covers building effects for Stratus (and Nimbus — same CPU, same firmware, same `.so` format; every command shown for Stratus applies to Nimbus identically) in [FAUST](https://faust.grame.fr/), the recommended zero-toolchain starting path for effect development. It is written in two tiers: **Tier 1** gets you from zero to an effect running on your pedal with no toolchain installed, using only the online FAUST IDE. **Tier 2** explains the production compile contract — what you must know before your effect's knobs actually work on hardware and before you submit to Tone Shop.

If you have never built an effect at all, do [quickstart.md](quickstart.md) first. If you want hand-written C++ instead, see [guide-native-cpp.md](guide-native-cpp.md).

## Why FAUST

You write your DSP in FAUST's declarative language; the Stratus architecture wrapper turns it into a C++ class matching the firmware's plugin ABI (see [dsp-contract.md](dsp-contract.md)), and the toolchain compiles it into the armhf `.so` the firmware loads with `dlopen()`. You never touch the ABI by hand — the wrapper handles the class layout, the exported symbols, and the version marker for you. The repo's `README.md` and the `examples/pluto` / `examples/spectrometer` directories (each has a `.dsp` source next to its generated `.cpp`) show this path in miniature.

Your `process` must be **mono: exactly one input and one output**. Stratus runs a mono, in-place, 44 100 Hz float32 chain — see [runtime-environment.md](runtime-environment.md).

---

## Tier 1 — Zero toolchain: the online FAUST IDE

The online FAUST IDE has integrated Stratus support and can produce an installer that puts your effect on the pedal — no compiler, no Docker, nothing installed locally. This flow is covered step-by-step in the official wiki tutorial: **[Faust and the Stratus — a basic tutorial](https://github.com/chaosaudio/Dev-Portal/wiki/Faust-and-the-Stratus-%E2%80%90-a-basic-tutorial)**. Summary:

1. Open the online IDE at <https://faustide.grame.fr>.
2. Write your effect and audition it in the browser — the IDE runs your FAUST code live against your audio input or a test file, with every slider on screen. Iterate here until it sounds right.
3. Add the `[stratus:N]` metadata to every control you want mapped to a hardware knob or switch. **This applies to the IDE path too**, not just production submission — read the binding rules in Tier 2 below before you export. A control without valid `[stratus:N]` metadata will install fine and then do nothing on the pedal.
4. Open the IDE's export dialog (the truck icon), and select platform **`chaos-stratus`**, architecture **`effect-installer`**.
5. Download the generated `.zip`.
6. With your Stratus (or Nimbus) powered on and connected to your computer with a USB cable (the devices have no Wi-Fi — SSH runs over the USB link; see [deploy-to-hardware.md](deploy-to-hardware.md)), follow the tutorial's install steps to run the installer from the zip. It builds and installs the effect into the device's effects directory for you. You will need SSH access to the device: the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it).

**Checkpoint:** the effect is installed on the device; audition it from the Beta version of the Chaos Audio app (see the "9 KNOB" tester workflow in [deploy-to-hardware.md](deploy-to-hardware.md)). To watch it load, run this on the device:

```bash
journalctl -f -u bela_startup.service
```

You should see the effect found and loaded with no `dlopen` errors (see [troubleshooting.md](troubleshooting.md) if not).

> **Warning:** in the Beta app's "9 KNOB" tester, do **not** tap "install" — it overwrites your binary with an unrelated one (`README.md`, Testing section).

The "9 KNOB" tester sends knob values in the range **0–10 with step 0.1** (`README.md`, Testing section). Design your FAUST sliders on a 0–10 range and map to real units (Hz, dB) inside your code — see the annotated example below.

---

## Tier 2 — The production contract

Everything below is what the production pipeline (the same one that compiles Tone Shop submissions) actually does with your FAUST source. Knowing it is the difference between "compiles" and "works on hardware".

### What the pipeline does with your code

- Your FAUST source is compiled with **FAUST 2.83.x** against the Stratus architecture wrapper (`stratus.cpp`), with these FAUST flags:

  ```
  -lang cpp -i -light -scn FaustDSP -single -ftz 0 -nvi -exp10 -inpl -ct 1 -es 1 -mcd 0 -mdd 1024 -mdy 33
  ```

  Practical consequences: `-single` means all DSP is **float32**; `-inpl` makes the generated code safe for the firmware's in-place buffers; the wrapper initializes FAUST at the fixed **44 100 Hz** rate, so `ma.SR` is always 44100 on device.

- The generated C++ is compiled by `arm-linux-gnueabihf-g++` (Debian bullseye) with:

  ```
  -fPIC -shared -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only
  ```

  See [build-flags-reference.md](build-flags-reference.md) for what each flag does and why `-fno-finite-math-only` is load-bearing.

- The output is named `<EFFECT-ID>.so`, where `EFFECT-ID` is a GUID assigned by the platform — the file name **is** the effect's identity on device (see [release-and-submission.md](release-and-submission.md)).

- The wrapper emits a class literally named `dsp` with the frozen vtable order the firmware expects, and exports `create()`, `getExtensions()`, `dsp_version = "2.0.0"`, and `setSampleRate(int)`. The `getExtensions` symbol is how the firmware recognizes a FAUST-built effect. You get all of this for free; the full ABI is documented in [dsp-contract.md](dsp-contract.md).

### `[stratus:N]` binding rules — read this twice

Hardware knobs and switches bind to your FAUST widgets **only** through metadata in the widget's **label**, of the exact form `[stratus:N]`. The rules, precisely:

- `N` must be a **single ASCII digit, `0` through `9`**. Nothing else is accepted: the architecture registers a control only when the metadata key is exactly `stratus` and the value is exactly one character in `'0'..'9'`.
- The **widget type** decides whether `N` indexes a knob or a switch — knob indices and switch indices are separate spaces (a knob `[stratus:0]` and a switch `[stratus:0]` can coexist):

| Widget | Binds to | Valid `N` |
|---|---|---|
| `hslider` / `vslider` | Knob `N` | 0–9 (max **10 knobs**) |
| `button` / `checkbox` | Switch `N`, 2-state | 0–4 (max **5 switches**) |
| `nentry` with `min=0`, `max=1`, `step=1` | Switch `N`, 2-position | 0–4 |
| `nentry` with `min=0`, `max=2`, `step=1` | Switch `N`, 3-position | 0–4 |
| `nentry` with any other min/max/step | **not bound** | — |
| `hbargraph` / `vbargraph` | never bound | — |

- Switch values on hardware are `UP = 0`, `DOWN = 1`, `MIDDLE = 2` — that is the `SWITCH_STATE` enum in `resources/dsp.hpp` (lines 42–46). Note the order: **middle is 2, not 1**. Your FAUST zone receives that raw number.
- Duplicate index: if two widgets claim the same slot, the **first registered wins**; the second is silently unbound.
- Switch indices 5–9 pass the single-digit check but exceed `MAXSWITCHES 5` (`resources/dsp.hpp:21`) and are silently dropped.

> **Gotcha — the #1 FAUST support issue:** a widget with missing or malformed `[stratus:N]` metadata **compiles without any warning and is silently unbound**. The effect installs, audio passes, and the control is frozen at its FAUST default value forever — the physical knob does nothing. All of these are silently unbound: `[stratus:10]`, `[knob:0]`, `[switch:0]`, `[stratus:switch:0]`, and a bare `stratus:0` used as the label text instead of bracket metadata. (`[stratus: 1]` with a stray space happens to bind — FAUST trims surrounding whitespace from label metadata — but write the canonical `[stratus:1]`.) Only `Label[stratus:N]` with a single digit binds. If your knob "does nothing" on the pedal, check this first.

Style metadata like `[style:knob]` only affects on-screen rendering; it is ignored for hardware binding and harmless to include.

### A complete annotated example — 3 knobs, 1 switch

```faust
// dirt-drive.dsp — mono drive pedal: Drive, Tone, Level knobs + Bright switch
declare name "Dirt Drive";   // fine locally; overwritten by the platform at submission
// Do NOT declare stratusId / stratusVersion / filename — see "Forbidden declares".

import("stdfaust.lib");

// ---- Controls -------------------------------------------------------------
// All sliders use the 0-10 range the app and the "9 KNOB" tester send.
// [stratus:0] in the LABEL binds this slider to hardware knob 0.

drive  = hslider("Drive[stratus:0]", 5, 0, 10, 0.1) : si.smoo;   // knob 0
tone   = hslider("Tone[stratus:1]",  5, 0, 10, 0.1) : si.smoo;   // knob 1
level  = hslider("Level[stratus:2]", 5, 0, 10, 0.1) : si.smoo;   // knob 2

// checkbox -> 2-state switch 0. On hardware: UP(0) = off, DOWN(1) = on.
// Note this is switch index 0 — a separate index space from knob 0 above.
bright = checkbox("Bright[stratus:0]");                          // switch 0

// ---- Mapping 0-10 knob values to real units --------------------------------
gain   = drive * 0.1;                          // 0..1 nonlinearity drive
cutoff = 800 + tone * 400 + bright * 2000;     // 800..4800 Hz, +2 kHz when Bright
makeup = ba.db2linear(level * 2.4 - 12);       // -12..+12 dB output

// ---- Process: MUST be exactly 1 in, 1 out (mono chain) ---------------------
process = ef.cubicnl(gain, 0) : fi.lowpass(1, cutoff) : *(makeup);
```

Points worth copying:

- `si.smoo` on every knob: knob values change on the controller thread while audio runs, so smooth them to avoid zipper noise.
- 0–10 slider ranges with in-code mapping to Hz/dB — matching what the app actually sends.
- A three-position variant of the switch would be `nentry("Mode[stratus:1]", 0, 0, 2, 1)` — and remember the value order is UP=0, DOWN=1, MIDDLE=2.

For contrast, the repo's own `examples/pluto/pluto.dsp` and `examples/spectrometer/spectrometer.dsp` have **no** `[stratus:N]` metadata — they predate this pipeline (their `.cpp` files were hand-adapted to read `knobs[]` directly). Do not feed those `.dsp` files through the production pipeline as-is; their sliders would not bind.

### Forbidden declares (Tone Shop submissions)

When you submit FAUST source for Tone Shop, the platform **auto-injects** these declares into your code before compiling, generated from your draft:

```faust
declare stratusId "…";       // server-generated GUID — the effect's identity
declare stratusVersion "…";  // your submitted version
declare filename "…";
declare name "…";            // your draft's name
```

Do **not** write `declare stratusId`, `declare stratusVersion`, or `declare filename` yourself in submitted code — the platform owns them, and hand-written values conflict with the injected ones. `declare name` is likewise overwritten at submission (writing it is fine for local and IDE builds, where it is used as-is).

### Soundfiles and libraries

- All the standard FAUST libraries (`stdfaust.lib` and friends) are available; `import("stdfaust.lib");` works everywhere.
- The `soundfile` primitive is supported by the production pipeline, referenced as `soundfile("label[url:{'name.wav'}]", 1)`. The submission platform enforces limits of up to **10 files, 500 KB total** (see [release-and-submission.md](release-and-submission.md)); the compile pipeline itself just embeds whatever files you provide. The files are embedded into the compiled binary at build time — there is no filesystem access on the device at runtime.
- Code using `ffunction()` (external C functions / lookup tables) cannot compile in a browser: it only builds where the referenced C code is available, i.e. server-side in the production pipeline. Expect the online IDE's in-browser run to fail on it.
- Everything is float32 (`-single`) at a fixed 44 100 Hz. There is no double-precision path on the Cortex-A8's NEON unit — see [runtime-environment.md](runtime-environment.md).

### Testing FAUST output locally with Docker

You can compile and load-check your generated C++ locally without touching hardware, using the Dev-Portal build container ([build-docker.md](build-docker.md) has the full setup — image build, QEMU/binfmt on Linux, Apple Silicon notes). Short version:

```bash
git clone https://github.com/chaosaudio/Dev-Portal.git
cd Dev-Portal
git submodule update --init --recursive
docker buildx build --platform linux/arm -t chaos-audio-builder --load .
```

**Checkpoint:** `docker image ls chaos-audio-builder` shows the image (first build takes ~10 minutes).

Export the generated C++ for your effect from the FAUST IDE (the C++ tab / source export), drop it in the repo, then compile it inside the container with the production flag set:

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
```

```bash
cd /workdir
g++ -fPIC -shared -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard \
    -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays \
    -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only \
    <YOUR-EFFECT>.cpp -o <EFFECT-ID>.so
```

- `<YOUR-EFFECT>.cpp` — the FAUST-generated C++ file (it is self-contained; the architecture wrapper is inlined).
- `<EFFECT-ID>.so` — for local testing any GUID-shaped name works; the real Effect ID is assigned at submission ([release-and-submission.md](release-and-submission.md)).

**Checkpoint:** the command exits silently and produces the `.so`. Verify the architecture:

```bash
file <EFFECT-ID>.so
```

Expected output:

```
<EFFECT-ID>.so: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

You can then run the repo's benchmark harness as a **loads-and-runs check** (it `dlopen`s the plugin the same way the firmware does). The harness is built into `./bins/tests/benchmark/` by the container's default build — if you don't have it yet, run the container once without `--entrypoint bash` first ([build-docker.md](build-docker.md)):

```bash
./bins/tests/benchmark/benchmark-plugin ./<EFFECT-ID>.so 2 44100
```

Expected output (start):

```
Loading library: ./<EFFECT-ID>.so
Loading symbol: create
Creating instance
Setting sample rate: 44100
```

If you get this far, your binary would load on the pedal.

> **Gotcha:** `benchmark-plugin` was written for the hand-written C++ ABI: after loading it invokes `instanceConstants()` and `benchmark()` (`tests/benchmark_plugin.cpp:55` onward). FAUST-architecture binaries do not populate the `benchmark` vtable slot, so the run may crash or print garbage *after* the lines shown above — that is a harness/ABI mismatch, not a defect in your effect. Treat everything through "Setting sample rate" as the meaningful check, and measure real performance on the device instead ([verification.md](verification.md) — QEMU timing numbers are meaningless anyway).

Two other off-pedal options:

- The online FAUST IDE itself — live audio testing in the browser before any export.
- The [chaos-stratus Python wrapper](https://pypi.org/project/chaos-stratus/) (`README.md`, Testing section): build your effect for your local machine with FAUST tooling, then drive its knobs/switches and process audio from a Python script.

To put a locally built `.so` on your device by hand (scp, ownership, service restart, log-watching), follow [deploy-to-hardware.md](deploy-to-hardware.md).

### IDE installer vs Tone Shop submission

The two FAUST outputs are not the same thing:

| | Tier 1: IDE `effect-installer` | Tone Shop submission |
|---|---|---|
| Purpose | Personal testing on **your** device | Public/paid release to all users |
| Compiler | IDE toolchain (FAUST version may differ from production) | Production pipeline: FAUST 2.83.x + bullseye ARM g++, exact flags above |
| Effect ID | Local/dev naming on your device | Platform-assigned GUID; binary is named `<EFFECT-ID>.so` |
| `declare stratusId` etc. | Not injected | Auto-injected — never write them yourself |
| Distribution | Only the pedal you install to | Tone Shop, with artwork/version/review requirements |

Because the compilers can differ, always re-verify the production-compiled result — binding metadata (`[stratus:N]`) and the constraints on this page are what carry over unchanged. The submission flow itself (drafts, assets, review, versioning) is documented in [release-and-submission.md](release-and-submission.md).

---

Next: [build-docker.md](build-docker.md)
