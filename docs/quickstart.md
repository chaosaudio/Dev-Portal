# Quickstart — Zero to Sound

This page takes you from an empty folder to hearing a compiled effect running on your **Stratus** pedal — or your **Nimbus** smart amp; effects are the same `.so` format on both, and every command on this page applies to Nimbus exactly as written. You will clone the Dev-Portal repo, build the Docker build image, compile the example effects, upload one to the **FX Builder**, publish it privately, and play through it from the Chaos Audio app. No DSP knowledge required yet.

> **Shortcut:** Just want to *make* an effect — not port an existing binary? You can skip this entire toolchain: write FAUST directly in the browser at [build.chaosaudio.com](https://build.chaosaudio.com), audition it there, and publish without installing anything locally. See [guide-faust.md](guide-faust.md).

**Total wall time: roughly 30–45 minutes**, most of it waiting on two Docker builds.

| Step | What happens | Typical time |
|---|---|---|
| [0. Prerequisites](#0-prerequisites) | Gather hardware, accounts, tools | 5–10 min |
| [1. Clone the repo](#1-clone-the-repo-with-submodules) | `git clone` + submodules | 2–5 min |
| [2. Build the Docker image](#2-build-the-docker-build-image) | One-time builder image | ~10 min |
| [3. Compile the examples](#3-compile-the-examples) | All examples → `bins/*.so` | ~15 min first time |
| [4. Smoke-test the plugin](#4-smoke-test-the-plugin-in-the-container) | Prove the `.so` loads and runs | ~2 min |
| [5. Publish privately in the FX Builder](#5-publish-privately-in-the-fx-builder) | Upload `equalizer.so`, quick UI, publish | ~5 min |
| [6. Install it in the app and play](#6-install-it-in-the-app-and-play) | Chaos Audio app → your device | ~5 min |

---

## 0. Prerequisites

### Hardware

- [ ] A **Stratus** pedal or **Nimbus** smart amp, powered on.
- [ ] A guitar (or any instrument) and cables. For Stratus, something downstream to hear yourself — an amp or headphones.
- [ ] A phone with the **Chaos Audio app**, signed in to your Chaos Audio account (see Accounts below).
- [ ] *(Optional)* a **data-capable USB cable** if you later want to watch the device's firmware logs over SSH — the devices have no Wi-Fi; the cable presents a small "USB network" to your computer, which is how `ssh` reaches them as `stratus.local`. See [deploy-to-hardware.md](deploy-to-hardware.md). Nothing in this guide requires it.

### Accounts & access

- [ ] **A free Chaos Audio account.** You'll upload your compiled effect to the [FX Builder](https://build.chaosaudio.com) in Step 5. It's the same account the mobile app uses — sign up in the FX Builder, or sign in with your existing app account (use "Reset Password" if you've never set one for the web).
- [ ] No GitHub account needed — the repo is public.

SSH access to your device is **not** required for this guide — it's only used for optional log-watching and on-device CPU measurement; see [deploy-to-hardware.md](deploy-to-hardware.md) and [verification.md](verification.md).

### Tools, per OS

| OS | What to install | Notes |
|---|---|---|
| **macOS** (Intel or Apple Silicon) | [Docker Desktop](https://www.docker.com/products/docker-desktop/), `git` | `ssh` is built in. Docker Desktop includes the QEMU/binfmt emulation the ARM build needs — nothing extra. Verified working on Apple Silicon. |
| **Windows** | Docker Desktop with the WSL 2 backend, plus a WSL 2 distro (e.g. Ubuntu) | Run **all** commands on this page inside your WSL 2 shell — they use `$(pwd)` and `$UID`, which need a POSIX shell. `git` and `ssh` come with the distro. |
| **Linux** | Docker Engine, `git`, `openssh-client` | You must install the QEMU binfmt handlers once (command in Step 2). Your user should be in the `docker` group. |

Verify Docker works before continuing:

```bash
docker version
```

**Checkpoint:** `docker version` prints both a Client and a Server section without errors.

---

## 1. Clone the repo (with submodules)

```bash
git clone https://github.com/chaosaudio/Dev-Portal.git
cd Dev-Portal
git submodule update --init --recursive
```

Expected output (trimmed):

```text
Cloning into 'Dev-Portal'...
...
Submodule 'modules/FFTConvolver' (https://github.com/falkTX/FFTConvolver.git) registered for path 'modules/FFTConvolver'
Submodule 'modules/RTNeural' (https://github.com/MaxPayne86/RTNeural.git) registered for path 'modules/RTNeural'
Submodule 'modules/dr_libs' (https://github.com/mackron/dr_libs.git) registered for path 'modules/dr_libs'
Submodule 'modules/r8brain' (https://github.com/avaneev/r8brain-free-src.git) registered for path 'modules/r8brain'
...
Submodule path 'modules/FFTConvolver': checked out '...'
```

> **Gotcha:** The submodule step is **not optional**. The top-level `CMakeLists.txt` builds `featured/aida-x-convolver` unconditionally, and it compiles sources straight out of `modules/FFTConvolver` and `modules/r8brain`. With empty submodule directories, the build in Step 3 fails at configure time with `Cannot find source file` errors.

**Checkpoint:** `ls modules/FFTConvolver` shows source files (e.g. `AudioFFT.cpp`), not an empty directory.

---

## 2. Build the Docker build image

The devices run 32-bit ARM Linux (armhf) on a TI AM335x Cortex-A8. The repo ships a `Dockerfile` (Debian bullseye-slim + `build-essential` + `cmake`) that becomes your build environment. This is **QEMU-emulated native armhf compilation, not cross-compilation** — Docker runs an actual ARM userland on your machine, emulated. That's why it's slow, and also why it's reliable: the compiler, libc, and flags match the device.

> **Gotcha:** Building inside this container also pins your binary to a glibc the production units actually have. Compiling on a random newer distro can produce `GLIBC_2.3x` symbol requirements that fail to load on the device. The container handles this for you.

**Linux only** — install the QEMU binfmt handlers once (Docker Desktop on macOS/Windows has this built in, skip ahead):

```bash
docker run --privileged --rm tonistiigi/binfmt --install all
```

Now build the image. **This takes about 10 minutes the first time** (it downloads the base image and installs the toolchain under emulation). It's cached afterwards.

From the `Dev-Portal` directory (the trailing `.` is the build context — the command fails anywhere else):

```bash
docker buildx build --platform linux/arm -t chaos-audio-builder --load .
```

Expected output (trimmed):

```text
[+] Building ...
 => [1/4] FROM docker.io/library/debian:bullseye-slim
 => [2/4] RUN apt-get update && apt-get install -y build-essential cmake ...
 ...
 => => naming to docker.io/library/chaos-audio-builder
```

**Checkpoint:** `docker images chaos-audio-builder` lists the image.

Full details on this flow (including troubleshooting `exec format error`): [build-docker.md](build-docker.md).

---

## 3. Compile the examples

Run the builder with the repo mounted at `/workdir`. Its entrypoint configures CMake, builds every example, and installs the resulting `.so` files into `./bins/` in your checkout. From the `Dev-Portal` directory (`$(pwd)` must be the repo root):

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder
```

**The first build takes around 15 minutes** — everything is compiled under QEMU emulation, so a laptop that would normally chew through this in a minute crawls. Incremental rebuilds (after you edit one file) are much faster because the `build/` directory is preserved in your checkout.

Expected output (trimmed, near the end):

```text
[100%] Built target benchmark-plugin
...
-- Install configuration: "Release"
-- Installing: /workdir/bins/equalizer.so
-- Installing: /workdir/bins/pluto.so
-- Installing: /workdir/bins/spectrometer.so
-- Installing: /workdir/bins/aida-x-convolver/aida-x-convolver.so
-- Installing: /workdir/bins/tests/benchmark/benchmark-player
-- Installing: /workdir/bins/tests/benchmark/benchmark-convolver
-- Installing: /workdir/bins/tests/benchmark/benchmark-plugin
```

**Checkpoint:** `ls bins/` shows one `.so` per example plus a `tests/` directory:

```bash
ls bins/
```

```text
aida-x-convolver  equalizer.so  pluto.so  spectrometer.so  tests
```

The exact list of `.so` files depends on the example set in your checkout — any one of them will do for the rest of this guide.


---

## 4. Smoke-test the plugin in the container

Before touching hardware, prove your `.so` exports the right symbols and processes audio. The build installed a `benchmark-plugin` harness (source: `tests/benchmark_plugin.cpp`) that `dlopen`s an effect, reads its `dsp_version`, calls `create()`, and pushes 2 seconds of signal through it — the same loading sequence the firmware uses.

This guide uses `equalizer.so` — a 5-band EQ — for the remaining steps. Any other `.so` from `bins/` works identically; just substitute its name in every command below.

Open a shell in the build container and run the harness:

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
```

Then, inside the container:

```bash
cd /workdir
./bins/tests/benchmark/benchmark-plugin ./bins/equalizer.so 2 44100
```

Expected output:

```text
Loading library: ./bins/equalizer.so
Loading symbol: create
Creating instance
Setting sample rate: 44100
Invoking instanceConstants method
Invoking benchmark method
Starting benchmark
Generating input signal
Computing 2 seconds of data @44100Hz
Processed 2 seconds of signal in 0.038303 seconds
120.161316 x real-time
Deleting instance
```

Type `exit` to leave the container.

> **Warning:** The "x real-time" number is **meaningless under QEMU** — it measures your emulated CPU, not the Cortex-A8. Use this step only as a "loads and runs without crashing" check (and for *relative* comparisons between two builds of the same effect). Real CPU numbers come from the device itself — see [verification.md](verification.md).

**Checkpoint:** The harness reached `Deleting instance` without a `dlopen`/`dlsym` error (and without a `The 'dsp_version' symbol does not exist` warning).

---

## 5. Publish privately in the FX Builder

The [FX Builder](https://build.chaosaudio.com) is Chaos Audio's browser-based development and publishing platform, and it's how a compiled `.so` gets onto your device — no cables, no manual file copying.

1. Sign in at [build.chaosaudio.com](https://build.chaosaudio.com) with your Chaos Audio account.
2. **Create a new effect** and **upload `bins/equalizer.so`** as its binary. The upload must be a 32-bit ARM shared object under 5 MB — anything built by the container in Step 3 qualifies.
3. Give it a quick UI in the **UI Builder** — knobs, switches, LED, background. Store images can be auto-generated, so don't sink time into artwork yet.
4. **Publish privately.** Private publish is instant — no review — and the effect appears only in your own account's library in the Chaos Audio app.

The platform assigns the effect its **Effect ID (GUID)** — you never invent one. Once installed, the binary lives on-device as `<EFFECT-ID>.so` under `/opt/update/sftp/firmware/effects/`, which is why log-watching and CPU measurement work the same as ever. When you're ready to release to everyone in the Tone Shop, see [release-and-submission.md](release-and-submission.md).

> **Gotcha:** For an uploaded prebuilt binary the platform only checks "32-bit ARM shared object under 5 MB" — it does **not** validate exports, ABI, or CPU cost, and shows a default CPU estimate. The container smoke test in Step 4 (and later the full [verification.md](verification.md) checklist) remains your safety net. Also, uploaded binaries can't be auditioned in the browser — that's for FAUST written in the editor. You'll hear this one on hardware in the next step.

**Checkpoint:** The effect shows as privately published in the FX Builder and appears in your library in the Chaos Audio app.

---

## 6. Install it in the app and play

1. Open the **Chaos Audio app** — signed in to the *same* account — and connect to your device.
2. Find your private effect in your library and **install** it on your Stratus/Nimbus, like any other effect.
3. Add it to your signal chain and make sure it's engaged (stomp **down** = on; up = bypassed).
4. Play.

If you hear only your dry signal, check that the effect is engaged. To iterate, upload a new binary in the FX Builder and publish privately again, then update the effect from the app. If you want to watch the firmware confirm the load (or debug a `dlopen` error), follow the logs over SSH — see [deploy-to-hardware.md](deploy-to-hardware.md).

**Checkpoint:** You hear the effect processing your instrument, and turning the knobs in the app changes the sound in real time. That's zero-to-sound — done.

---

## What to read next

You just published someone else's DSP. To ship your own, pick a path:

**Path A — FAUST in the FX Builder (recommended for most developers, zero local toolchain).** Write DSP in the [FAUST](https://faust.grame.fr/) language directly in the [FX Builder](https://build.chaosaudio.com)'s browser editor — with optional AI assistance, in-browser audition (built-in audio test with knobs), and cloud compilation for Stratus/Nimbus — then publish privately, all without touching this repo. Start with [guide-faust.md](guide-faust.md).

**Path B — Native C++ (full control, bring your own algorithms).** Subclass `struct dsp` from `resources/dsp.hpp`, implement `instanceConstants()` and `compute()`, export `create()`/`dsp_version`, and build with the exact flag set the repo's CMake applies. Start with [guide-native-cpp.md](guide-native-cpp.md).

Whichever path you take, read these before writing DSP of your own:

- [dsp-contract.md](dsp-contract.md) — the ABI your `.so` must satisfy (vtable order, exports, lifecycle).
- [runtime-environment.md](runtime-environment.md) — 44.1 kHz mono in-place audio, one Cortex-A8 core, the 2.9 ms budget.
- [rt-safety.md](rt-safety.md) — what must never happen inside `compute()`.

Next: [dsp-contract.md](dsp-contract.md)
