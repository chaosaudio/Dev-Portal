# Chaos Audio Dev-Portal

Everything you need to build audio effects for **Stratus®** (multi-effects
pedal) and **Nimbus™** (100 W smart amp), and publish them on **Tone Shop™**.

Both devices share the same brain — a Cortex-A8 processor running the same
firmware architecture — and load effects in the same format: a single
compiled `.so` plugin. **Build your effect once and it runs on every Stratus
and every Nimbus.** Everything in these docs applies to both devices; where
commands mention `stratus.local`, the same steps work on a Nimbus.

**New here? Watch the 2½-minute intro** — the platform, both development
paths, and how payouts work:

[![Chaos Audio Stratus & Nimbus Developer Update — 2½-minute video intro](https://cdn.loom.com/sessions/thumbnails/46cf6fb16e2440de8bb0af152aaa308b-fa85aacd91e769df.gif)](https://www.loom.com/share/46cf6fb16e2440de8bb0af152aaa308b)

## The fastest way: build in your browser

You don't need this repo — or any local tooling — to make an effect. The
**[FX Builder](https://build.chaosaudio.com)** is Chaos Audio's browser-based
development and publishing platform: create an effect and write **FAUST code
directly in the browser editor**, with optional AI assistance (describe the
sound you want; the AI drafts and iterates on the FAUST for you). The FX
Builder validates and compiles for Stratus/Nimbus in the cloud, and you can
**audition your effect right in the browser** — a built-in audio test with
knobs — before ever touching hardware. When it sounds right, give it a quick
UI in the UI Builder, **publish privately** (instant, no review), and install
it on your own Stratus or Nimbus from the Chaos Audio app.

All you need is a free Chaos Audio account — the same account as the mobile
app. Sign up in the FX Builder, or use "Reset Password" if you already have
an app account. This is the easiest way to develop for Stratus, and the
[FAUST guide](docs/guide-faust.md) covers it first.

**This repo is the local toolchain.** It's for developers who want to write
C++ by hand, port a JUCE effect, or run FAUST locally: you compile the ARM
`.so` with the Docker toolchain described here, then upload the binary to the
FX Builder (create effect → upload binary), build a UI, and publish privately
to test on hardware — and submit for review when you're ready for Tone Shop™.

## Start here

**[→ The Quickstart](docs/quickstart.md)** takes you from a fresh machine to
hearing a compiled example effect running on your own hardware in about
30-45 minutes. Do it once even if you're experienced — it validates your whole
toolchain.

## Choose your path

| You want to… | Path | Start at |
|---|---|---|
| Build effects **without installing anything** | FX Builder — FAUST in the browser, with optional AI and in-browser audition | [build.chaosaudio.com](https://build.chaosaudio.com) + [FAUST guide](docs/guide-faust.md) |
| Write **C++ by hand** with full control | Native C++ + Docker build | [Native C++ guide](docs/guide-native-cpp.md) |
| Use **FAUST locally** with the Docker toolchain | FAUST → C++ → Docker | [FAUST guide](docs/guide-faust.md), then [Docker builds](docs/build-docker.md) |
| Port an existing **JUCE** effect | JUCE example project | [examples/juce_effect](examples/juce_effect/) + [Repo tour](docs/runtime-examples-map.md) |

No path is second-class: they all produce the same kind of `.so`, and they
all must honor the same [plugin contract](docs/dsp-contract.md) and
[real-time safety rules](docs/rt-safety.md).

## The documentation

Read in this order the first time; use as reference afterward.

1. **[Quickstart](docs/quickstart.md)** — clone → build → upload to the
   FX Builder → hear it.
2. **[The plugin contract](docs/dsp-contract.md)** — the `dsp` class, the
   four exported symbols, and the binary rules every effect must satisfy.
3. **[The runtime environment](docs/runtime-environment.md)** — what the
   device does around your code: buffers, block sizes, knobs, bypass.
4. **[Real-time safety](docs/rt-safety.md)** — how to never cause a click,
   dropout, or crash. The most important page here.
5. **[Native C++ guide](docs/guide-native-cpp.md)** — a complete effect,
   written line by line.
6. **[FAUST guide](docs/guide-faust.md)** — the in-browser FX Builder path,
   the local FAUST → C++ path, and the `[stratus:N]` binding rules.
7. **[Docker builds](docs/build-docker.md)** — set up the build toolchain
   from absolute zero on macOS, Windows, or Linux.
8. **[Compiler flags reference](docs/build-flags-reference.md)** — the
   canonical flag lists and why each one matters.
9. **[Test on hardware](docs/deploy-to-hardware.md)** — publish privately
   in the FX Builder, install from the Chaos Audio app, and (optionally)
   watch the firmware logs over SSH.
10. **[Verification](docs/verification.md)** — prove your effect is
    CPU-safe, crash-safe, and dropout-free before you ship it.
11. **[Troubleshooting](docs/troubleshooting.md)** — symptom-indexed fixes
    for everything we've ever seen go wrong.
12. **[Repo tour](docs/runtime-examples-map.md)** — what every example,
    helper, and vendored module is for.
13. **[Release & submission](docs/release-and-submission.md)** — naming,
    versioning, the pre-ship checklist, the FX Builder review flow for
    Tone Shop™, pricing and payouts, and the legal documents.

## What's in this repo

```
resources/dsp.hpp   The plugin ABI header — the ONE header your effect includes
examples/           Complete effects, from beginner (equalizer) to advanced
common/             Reusable DSP helpers (Biquad, EQ, ValueSmoother)
modules/            Vendored libraries (RTNeural, FFTConvolver, r8brain, dr_libs)
featured/           Production-grade community effects worth reading
tests/              benchmark_plugin.cpp — builds the benchmark-plugin tool
                    (installs to bins/tests/benchmark/)
Dockerfile          The build container (compiles for the device's CPU)
docs/               The documentation listed above
```

## On the roadmap

- **Neural Amp Modeler SDK** — use NAM captures inside your own plugins.
  Planned, not yet available: see [the roadmap note](docs/roadmap-nam-sdk.md)
  for the idea, the constraints, and what you can do today.

## Required hardware

- A [Stratus®](https://chaosaudio.com/products/stratus) pedal **or**
  Nimbus™ amp for on-hardware testing (everything up to hardware testing
  works without one).
- Any macOS, Windows, or Linux computer that can run Docker.

## Publishing & legal

When you're ready to release publicly, these are the documents that govern
distribution on Tone Shop™:

- **[Developer Distribution Agreement](https://build.chaosaudio.com/developer-agreement)**
  — the binding contract you accept (in a click-through popup) before your
  first public submission, and again whenever the agreement version changes.
- **[Developer Guidelines](https://build.chaosaudio.com/guidelines)** — the
  house rules: technical, asset, and content requirements, plus worked payout
  examples (sign-in required).
- **[DMCA Policy](https://build.chaosaudio.com/dmca)** — how copyright
  complaints are handled.

Pricing is self-serve in the FX Builder (free, or US$0.99–$49.99), and your
revenue share pays out via Stripe — details in
[Release & submission](docs/release-and-submission.md).

## Getting help

- The [Troubleshooting page](docs/troubleshooting.md) first — it's indexed
  by symptom.
- The Chaos Audio developer community (Discord) — link in your developer
  welcome email.
- Developer and marketplace matters — development@chaosaudio.com.
- [FAUST documentation](https://faust.grame.fr/), the
  [online FAUST IDE](https://faustide.grame.fr/), and
  [our FAUST + Stratus tutorial](https://github.com/chaosaudio/Dev-Portal/wiki/Faust-and-the-Stratus-%E2%80%90-a-basic-tutorial)
  for FAUST-specific questions.

## Cloning this repo

```bash
git clone https://github.com/chaosaudio/Dev-Portal.git
cd Dev-Portal
git submodule update --init --recursive
```

> **Warning:** the `git submodule` step is not optional — the build fails
> without it. It downloads the four vendored DSP libraries (RTNeural,
> FFTConvolver, r8brain, dr_libs). JUCE is vendored separately inside
> `examples/juce_effect/` and doesn't need this step.

Then head to the **[Quickstart](docs/quickstart.md)**.

Next: [Quickstart](docs/quickstart.md)
