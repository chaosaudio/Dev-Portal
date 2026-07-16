# Quickstart — Zero to Sound

This page takes you from an empty folder to hearing a compiled effect running on your **Stratus** pedal — or your **Nimbus** smart amp; effects are the same `.so` format on both, and every command and hostname on this page applies to Nimbus exactly as written. You will clone the Dev-Portal repo, build the Docker build image, compile the example effects, push one to your device over SSH, and play through it from the Beta app. No DSP knowledge required yet.

**Total wall time: roughly 30–45 minutes**, most of it waiting on two Docker builds.

| Step | What happens | Typical time |
|---|---|---|
| [0. Prerequisites](#0-prerequisites) | Gather hardware, accounts, tools | 5–10 min |
| [1. Clone the repo](#1-clone-the-repo-with-submodules) | `git clone` + submodules | 2–5 min |
| [2. Build the Docker image](#2-build-the-docker-build-image) | One-time builder image | ~10 min |
| [3. Compile the examples](#3-compile-the-examples) | All examples → `bins/*.so` | ~15 min first time |
| [4. Smoke-test the plugin](#4-smoke-test-the-plugin-in-the-container) | Prove the `.so` loads and runs | ~2 min |
| [5. Deploy to your device](#5-deploy-to-your-device) | `scp` + restart audio service | ~3 min |
| [6. Load it in the app and play](#6-load-it-in-the-app-and-play) | 9 KNOB tester effect | ~5 min |

---

## 0. Prerequisites

### Hardware

- [ ] A **Stratus** pedal or **Nimbus** smart amp, powered on and connected to the **same network as your computer** (set up Wi-Fi through the Chaos Audio app if you haven't).
- [ ] A guitar (or any instrument) and cables. For Stratus, something downstream to hear yourself — an amp or headphones.
- [ ] A phone with the **Beta** version of the Chaos Audio app (see Accounts below).

### Accounts & access

- [ ] **SSH access to your device.** You need the developer password for your device (issued with the developer program). Not in the program yet? Apply via the [Developer Application](https://chaosaudio.com/pages/developer-portal) or email support@chaosaudio.com — the password and the developer Discord invite arrive with your acceptance. Already accepted but missing the password? Ask in the developer Discord/support. Steps 0–4 work fine while you wait; only Step 5 onward needs the device password.
- [ ] **Beta app.** Join the [Beta Program](https://chaosaudio.com/pages/beta-program) to get the Beta version of the mobile app — it contains the "9 KNOB" tester effect this guide uses.
- [ ] No GitHub account needed — the repo is public.

### Tools, per OS

| OS | What to install | Notes |
|---|---|---|
| **macOS** (Intel or Apple Silicon) | [Docker Desktop](https://www.docker.com/products/docker-desktop/), `git` | `ssh`/`scp` are built in. Docker Desktop includes the QEMU/binfmt emulation the ARM build needs — nothing extra. Verified working on Apple Silicon. |
| **Windows** | Docker Desktop with the WSL 2 backend, plus a WSL 2 distro (e.g. Ubuntu) | Run **all** commands on this page inside your WSL 2 shell — they use `$(pwd)` and `$UID`, which need a POSIX shell. `git`, `ssh`, `scp` come with the distro. |
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

## 5. Deploy to your device

The firmware loads effects from `/opt/update/sftp/firmware/effects/`, where each effect is a shared object named `<EFFECT-ID>.so`. The Beta app's **"9 KNOB"** tester effect is wired to one specific ID — name your binary after it and the app can drive your effect with 9 live knobs (each ranging 0–10 in 0.1 steps):

```text
55631e3a-94f7-42f8-8204-f5c6c11c4a21
```

Copy your compiled effect onto the device under that name:

```bash
scp bins/equalizer.so root@stratus.local:/opt/update/sftp/firmware/effects/55631e3a-94f7-42f8-8204-f5c6c11c4a21.so
```

- `equalizer.so` — the example you built; substitute your own `<YOUR-EFFECT>.so` here later.
- `55631e3a-94f7-42f8-8204-f5c6c11c4a21` — the `<EFFECT-ID>` of the 9 KNOB tester. For a real release your effect gets its own ID; see [release-and-submission.md](release-and-submission.md).
- `stratus.local` — your device's hostname (works for Nimbus the same way). If it doesn't resolve, use the device's IP address; see [troubleshooting.md](troubleshooting.md).

The very first time you connect to the device, `ssh`/`scp` stops with a host-key prompt (trimmed):

```text
The authenticity of host 'stratus.local' can't be established.
...
Are you sure you want to continue connecting (yes/no/[fingerprint])?
```

This is normal on first contact with any new host, not an error — type `yes`. See [deploy-to-hardware.md](deploy-to-hardware.md) Step 2 for details. Then, when prompted, enter the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it).

On a first deploy the destination file doesn't exist yet, so a plain `scp` is safe. When you later redeploy over an effect that is currently loaded and playing, don't overwrite it in place — use the copy-to-temp-then-`mv` swap pattern in [deploy-to-hardware.md](deploy-to-hardware.md), or the copy itself can crash live audio.

Now fix ownership and restart the audio service so it picks the file up:

```bash
ssh root@stratus.local 'chown update:sftponly /opt/update/sftp/firmware/effects/55631e3a-94f7-42f8-8204-f5c6c11c4a21.so && systemctl restart bela_startup'
```

Audio on the device drops for a few seconds while the service restarts.

> **Warning:** Never create or modify a `.version` file inside `/opt/update/sftp/firmware/` — it triggers a **full firmware reinstall**. Only touch the `effects/` directory.

**Checkpoint:** Watch the device logs and confirm your effect loads cleanly:

```bash
ssh root@stratus.local 'journalctl -f -u bela_startup.service'
```

After you select the effect in the app (next step), look for an `Effect found` line for your ID. If you instead see a `dlopen` error or `Effect does not exist.`, the binary failed to load — the most common cause is a missing-symbol failure; see [troubleshooting.md](troubleshooting.md) and [build-flags-reference.md](build-flags-reference.md). Press `Ctrl+C` to stop following the log.

---

## 6. Load it in the app and play

1. Open the **Beta** version of the Chaos Audio app and connect to your device.
2. In the effect list, open the **"Development"** category and find the effect titled **"9 KNOB"**.
3. Add **9 KNOB** to your signal chain and make sure it's engaged (stomp **down** = on; up = bypassed).
4. Play.

The 9 knobs in the app map to your effect's knob indices 0–8, each sending raw values 0–10. Knobs your effect doesn't read simply do nothing. If you hear only your dry signal, check that the effect is engaged and re-check the log checkpoint above.

> **Warning:** Do **not** tap "install" on the 9 KNOB effect in the app — it will overwrite your binary on the device with an unrelated one.

**Checkpoint:** You hear the effect processing your instrument, and turning the mapped knobs in the app changes the sound in real time. That's zero-to-sound — done.

---

## What to read next

You just deployed someone else's DSP. To ship your own, pick a path:

**Path A — FAUST (recommended for most developers, zero local toolchain to start).** Write DSP in the [FAUST](https://faust.grame.fr/) language; the [online FAUST IDE](https://faustide.grame.fr/) has integrated Stratus support and can build and install effects for you. Start with [guide-faust.md](guide-faust.md).

**Path B — Native C++ (full control, bring your own algorithms).** Subclass `struct dsp` from `resources/dsp.hpp`, implement `instanceConstants()` and `compute()`, export `create()`/`dsp_version`, and build with the exact flag set the repo's CMake applies. Start with [guide-native-cpp.md](guide-native-cpp.md).

Whichever path you take, read these before writing DSP of your own:

- [dsp-contract.md](dsp-contract.md) — the ABI your `.so` must satisfy (vtable order, exports, lifecycle).
- [runtime-environment.md](runtime-environment.md) — 44.1 kHz mono in-place audio, one Cortex-A8 core, the 2.9 ms budget.
- [rt-safety.md](rt-safety.md) — what must never happen inside `compute()`.

Next: [dsp-contract.md](dsp-contract.md)
