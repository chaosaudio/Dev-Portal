# Building Effects with Docker

This page takes you from a machine with nothing installed to a compiled effect `.so` in `bins/`, using the Docker build container that ships in this repo. It is the recommended way to build effects on your own computer — the same binary runs unchanged on both **Stratus** (the pedal) and **Nimbus** (the smart amp), because both share the same Cortex-A8 CPU and firmware. If you have never used Docker before, start at the top; if Docker is already installed, jump to [The universal build flow](#the-universal-build-flow).

For what the compiler flags mean, see [build-flags-reference.md](build-flags-reference.md). For what to do with the finished `.so`, see [verification.md](verification.md) and [deploy-to-hardware.md](deploy-to-hardware.md).

## How this build actually works (read this once)

The container defined in the repo's `Dockerfile` is a **32-bit ARM (armhf) Debian bullseye** image with a stock ARM `gcc` and `cmake` inside:

```dockerfile
FROM debian:bullseye-slim
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*
```

Your x86_64 or Apple Silicon machine cannot execute ARM binaries directly, so Docker runs the whole container under **QEMU user-mode emulation**: every instruction of the ARM compiler is emulated. This is *emulated native compilation*, not cross-compilation (the comment in the `Dockerfile` calls it cross-compilation — it isn't).

Why do it this way?

- **Zero toolchain setup.** No cross-compiler, no sysroot, no SDK to install. Docker + this repo is the entire toolchain.
- **Binary compatibility is pinned for you.** Debian bullseye ships glibc 2.31 and GCC 10.x — matching the runtime on the devices. Build on a random modern distro instead and your `.so` can pick up `GLIBC_2.3x` symbol versions the device doesn't have, and it will silently fail to load on hardware.
- **The trade-off is speed.** Emulation is roughly an order of magnitude slower than a native build. Expect ~10 minutes to build the image and ~15 minutes for the first full compile (measured on an Apple Silicon Mac; other hosts are in the same ballpark). Incremental rebuilds are much faster because the CMake cache persists — see [Incremental rebuilds](#incremental-rebuilds).

## Step 1 — Install Docker (per OS)

### macOS (Apple Silicon and Intel)

1. Download **Docker Desktop for Mac** from <https://www.docker.com/products/docker-desktop/> — pick the *Apple Silicon* or *Intel chip* download to match your Mac (Apple menu → About This Mac if unsure).
2. Open the `.dmg`, drag Docker to Applications, launch it, and accept the service agreement. Wait until the whale icon in the menu bar stops animating.
3. Verify from a terminal:

```bash
docker version && docker buildx version
```

Expected output (trimmed):

```
Client:
 Version:           27.x.x
...
Server: Docker Desktop
...
github.com/docker/buildx v0.x.x
```

Docker Desktop ships the QEMU/binfmt emulation handlers built in — **you do not need the `tonistiigi/binfmt` step** that Linux users need.

> **Gotcha:** Docker Desktop's *"Use Rosetta for x86_64/amd64 emulation on Apple Silicon"* setting is irrelevant here. Rosetta only accelerates 64-bit x86 containers; this build targets 32-bit ARM (`linux/arm`), which always goes through QEMU no matter how that toggle is set. Don't waste time flipping it to chase build speed.

**Checkpoint:** `docker run --rm hello-world` prints "Hello from Docker!".

### Windows (Docker Desktop + WSL2)

1. Install **WSL2** if you don't have it: open PowerShell *as Administrator* and run `wsl --install` (installs Ubuntu by default), then reboot.
2. Download and install **Docker Desktop for Windows** from <https://www.docker.com/products/docker-desktop/>. During setup, keep **"Use WSL 2 based engine"** enabled. In Docker Desktop → Settings → Resources → WSL Integration, enable integration for your Ubuntu distro.
3. Do **all** of the following steps of this guide from a WSL2 shell (open "Ubuntu" from the Start menu), not from PowerShell or CMD.

Like macOS, Docker Desktop on Windows includes the QEMU/binfmt handlers — no extra emulation setup.

> **Warning:** Clone the repo **inside the WSL2 filesystem** (e.g. `~/Dev-Portal`), *not* under `/mnt/c/...`. Two real problems with a Windows-drive checkout: (1) bind-mounted I/O across the Windows/Linux boundary is drastically slower, which stacks on top of already-slow QEMU compilation; (2) Windows Git defaults to `core.autocrlf=true`, which checks files out with CRLF line endings — shell scripts and configuration read inside the Linux container then break in confusing ways. Cloning with Git inside WSL2 avoids both.

> **Gotcha:** `$UID` and `$(pwd)` in the build commands below are Unix shell syntax. They work in the WSL2 shell. They do **not** work in PowerShell, and Git Bash additionally rewrites paths like `/workdir` into `C:/...` (MSYS path mangling), silently breaking the `-v` mount. Use the WSL2 shell, full stop.

**Checkpoint:** inside your WSL2 shell, `docker run --rm hello-world` prints "Hello from Docker!".

### Linux

1. Install Docker Engine. The quickest supported route:

```bash
curl -fsSL https://get.docker.com | sh
```

(Or follow your distro's instructions for `docker-ce` — make sure the `docker-buildx-plugin` package comes with it.)

2. Add yourself to the `docker` group so you don't need `sudo` for every command, then log out and back in (or run `newgrp docker`):

```bash
sudo usermod -aG docker $USER
```

3. **One-time emulation setup.** Unlike Docker Desktop, a bare Linux engine has no ARM emulation registered. Install the QEMU binfmt handlers:

```bash
docker run -it --rm --privileged tonistiigi/binfmt --install all
```

Expected output (trimmed):

```
installing: arm qemu-arm OK
...
{
  "supported": [
    ...
    "linux/arm/v7",
    ...
  ],
```

> **Gotcha:** binfmt registrations don't always survive a reboot, depending on your distro. If ARM containers worked yesterday and today fail with `exec format error`, just re-run the `tonistiigi/binfmt` command above.

**Checkpoint:** `docker run --rm hello-world` works without `sudo`, and the binfmt output lists `linux/arm/v7` under `supported`.

## The universal build flow

Everything from here on is identical on macOS, Windows (inside WSL2), and Linux.

### Step 2 — Clone the repo and its submodules

```bash
git clone https://github.com/chaosaudio/Dev-Portal.git
cd Dev-Portal
git submodule update --init --recursive
```

The submodule step is **required**, not optional: the top-level `CMakeLists.txt` builds `featured/aida-x-convolver` and `featured/aida-x-tests` unconditionally, and those pull sources straight out of `modules/` (the submodules listed in `.gitmodules`: `RTNeural`, `FFTConvolver`, `r8brain`, `dr_libs`). Skip it and the very first build fails with:

```
CMake Error at featured/aida-x-convolver/CMakeLists.txt:22 (add_library):
  Cannot find source file:

    ../../modules/FFTConvolver/AudioFFT.cpp
```

**Checkpoint:** `ls modules/FFTConvolver` shows source files (e.g. `AudioFFT.cpp`), not an empty directory.

### Step 3 — Build the builder image (once)

From the repo root:

```bash
docker buildx build --platform linux/arm -t chaos-audio-builder --load .
```

Expected output (trimmed — the first run takes **~10 minutes** because the `apt-get install` of the toolchain runs under emulation; subsequent runs are seconds thanks to layer caching):

```
[+] Building 600.0s (10/10) FINISHED
 => [internal] load build definition from Dockerfile
 => [1/4] FROM docker.io/library/debian:bullseye-slim
 => [2/4] RUN apt-get update && apt-get install -y     build-essential     cmake ...
 => [3/4] COPY entrypoint.sh /
 => [4/4] RUN chmod +x /entrypoint.sh
 => exporting to image
 => => naming to docker.io/library/chaos-audio-builder
```

What the flags mean: `--platform linux/arm` builds a 32-bit ARM image (matching the devices), `-t chaos-audio-builder` names it, and `--load` puts the result into your local image store so `docker run` can find it.

**Checkpoint:** `docker images` lists `chaos-audio-builder`.

### Step 4 — Compile

Still from the repo root:

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder
```

Flag by flag:

| Flag | What it does | Why you need it |
|---|---|---|
| `--rm` | Deletes the container when the build finishes. | Each run is throwaway; all state that matters lives in your repo directory via the mount. |
| `-it` | Interactive terminal. | Lets you see live build output and Ctrl-C a stuck build. |
| `-u $UID` | Runs the build as *your* user ID instead of root. | The build writes `build/` and `bins/` into your repo through the mount. Without this they end up **owned by root** and later `git`/`rm`/editor operations on your host fail with permission errors. If your shell doesn't define `$UID` (plain `sh` doesn't), use `-u "$(id -u)"`. |
| `--platform linux/arm` | Forces the 32-bit ARM variant of the image. | Prevents multi-arch hosts from warning about or picking a mismatched platform. |
| `-v "$(pwd):/workdir"` | Bind-mounts your current directory to `/workdir` in the container. | The container's entrypoint builds whatever is at `/workdir` — so you **must run this from the repo root**. Artifacts written to `/workdir` appear directly in your checkout. |
| `chaos-audio-builder` | The image from Step 3. No command is given after it, so the image's entrypoint runs. | |

The entrypoint (`entrypoint.sh` in the repo, baked into the image) is the entire build script:

```bash
cd /workdir
mkdir -p build && cd build \
&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DBUILD_BENCH=ON ../ \
&& cmake --build . \
&& make install DESTDIR="/workdir"
```

Expected output (heavily trimmed — the first full build takes **~15 minutes** under emulation):

```
-- The C compiler identification is GNU 10.2.1
-- The CXX compiler identification is GNU 10.2.1
CMAKE_CXX_FLAGS_RELEASE in /workdir/examples =  -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only
...
[ 40%] Building CXX object examples/CMakeFiles/equalizer.dir/equalizer/equalizer.cpp.o
...
-- Installing: /workdir/bins/equalizer.so
-- Installing: /workdir/bins/tests/benchmark/benchmark-plugin
```

That `CMAKE_CXX_FLAGS_RELEASE` line is worth checking on every fresh configure: it is the device-correct flag set from `examples/CMakeLists.txt`, including the load-bearing `-fno-finite-math-only` (without it, `-ffast-math` generates libm symbols the device lacks and the effect silently fails to load — details in [build-flags-reference.md](build-flags-reference.md)).

> **Gotcha:** If configure fails instead of compiling, don't stare at the wall of red — the two common causes (missing submodules, and a stale `build/` directory configured on your host) are in the [troubleshooting table](#troubleshooting) below, with exact error text.

**Checkpoint:** the run ends with `-- Installing: ...` lines and no `Error`, and a `bins/` directory now exists in your repo.

### Step 5 — Where the artifacts land

Everything installs into `bins/` at the repo root (the CMake install rules use `DESTINATION bins` with the prefix stripped, and shared libraries are built without the `lib` prefix):

```bash
ls bins
```

Expected output:

```
aida-x-convolver  equalizer.so  pluto.so  spectrometer.so  tests
```

- `*.so` — the compiled example effects (plus your own, once you add one — see [guide-native-cpp.md](guide-native-cpp.md)).
- `bins/tests/benchmark/benchmark-plugin` — a host-side load-and-run harness for your `.so`. Run it **inside the container**, since it's an ARM binary; [verification.md](verification.md) covers this, including why QEMU timing numbers are meaningless and how to get real CPU numbers on the device.
- `bins/aida-x-convolver/` — the featured convolver effect.

These are 32-bit ARM binaries: they will not run on your host OS, only in the container or on a Stratus/Nimbus. To put one on hardware you rename it to `<EFFECT-ID>.so` and copy it over — that whole flow is [deploy-to-hardware.md](deploy-to-hardware.md). (`<EFFECT-ID>` is the GUID the firmware loads the effect by; deploy doc explains where it comes from.)

**Checkpoint:** `file bins/equalizer.so` (on Linux/WSL2/macOS with `file` installed) reports `ELF 32-bit LSB shared object, ARM`.

### Incremental rebuilds

The entrypoint runs `mkdir -p build` inside `/workdir` — which is your repo directory. So the CMake cache and all object files **persist between runs** in `./build/`. Re-running the Step 4 command after editing one source file only recompiles what changed; an incremental rebuild is typically well under a minute instead of ~15.

Delete the cache and start clean —

```bash
rm -rf build bins
```

— whenever any of these happen:

- **You configured the repo on your host** (ran `cmake` outside Docker, or your IDE did it for you). The cached compiler paths in `build/CMakeCache.txt` point at your host toolchain; the next container run then fails or, worse, half-reuses host artifacts.
- You switched branches and the CMake target layout changed.
- You edited any `CMakeLists.txt` and the errors stop making sense.
- You upgraded/rebuilt the `chaos-audio-builder` image.

> **Warning:** If you ever configure by hand (in a container shell, or via an IDE) instead of letting the entrypoint do it, you **must** pass `-DCMAKE_BUILD_TYPE=Release`, exactly as `entrypoint.sh` does. The entire device-correct flag set — including the load-bearing `-fno-finite-math-only` — lives in `CMAKE_CXX_FLAGS_RELEASE` in `examples/CMakeLists.txt`, so a plain `cmake ../` silently builds with **none** of those flags and the resulting `.so` can fail to load on hardware. Self-check: the `CMAKE_CXX_FLAGS_RELEASE in /workdir/examples = ...` line from Step 4 must appear in your configure output.

### Poking around inside the container

To get a shell in the build environment instead of running the build (useful for running `benchmark-plugin`, re-running `make` by hand, or debugging):

```bash
docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" --entrypoint bash chaos-audio-builder
```

Your prompt may say `I have no name!` — cosmetic; it's because `-u $UID` maps to a user that has no entry in the container's `/etc/passwd`.

## Troubleshooting

| Symptom (exact text where possible) | Cause | Fix |
|---|---|---|
| `exec format error` (or `exec /entrypoint.sh: exec format error`) when running any `linux/arm` container | No ARM binfmt/QEMU handlers registered — Linux engine without the one-time setup, or the registration didn't survive a reboot | Run `docker run -it --rm --privileged tonistiigi/binfmt --install all` (Linux only; Docker Desktop has this built in — there, update Docker Desktop instead) |
| `docker: 'buildx' is not a docker command` | Ancient Docker, or the buildx plugin isn't installed | Update Docker Desktop, or on Linux install the `docker-buildx-plugin` package (the `get.docker.com` script includes it) |
| `docker buildx build` succeeds but `docker run` says `Unable to find image 'chaos-audio-builder'` | You built with a `docker-container` builder and forgot `--load`, so the image never reached the local store | Re-run the build with `--load` (as shown in Step 3), or `docker buildx use default` first |
| CMake errors about the cache: `CMakeCache.txt ... is different than the directory` / compiler is `/usr/bin/cc` with a host path / `The CMAKE_CXX_COMPILER: ... is not able to compile a simple test program` | Stale `build/` from a configure run on your **host** (IDE integrations do this silently) | `rm -rf build bins` and re-run the Step 4 command |
| `Cannot find source file: ../../modules/FFTConvolver/AudioFFT.cpp` | Submodules not initialized | `git submodule update --init --recursive`, then rebuild — required again in every new git worktree or fresh CI checkout; submodule contents are per-worktree |
| `Cannot find source file: dsp/dsp.cpp` / `No SOURCES given to target: dsp` | Old checkout — current `examples/CMakeLists.txt` ships a skip guard so the example auto-discovery loop ignores directories without a matching `<name>.cpp` (like `examples/dsp/`, a header-only library) | Update your checkout (`git pull`), then rebuild |
| `g++: internal compiler error: Killed (program cc1plus)`, or the container dies with exit code 137 | Docker Desktop VM out of memory — QEMU-emulated compiles are memory-hungry, especially large translation units like JUCE (`examples/juce_effect`) | Docker Desktop → Settings → Resources → raise **Memory** to 6–8 GB, then re-run the build |
| `permission denied ... /var/run/docker.sock` (Linux) | Your user isn't in the `docker` group yet | `sudo usermod -aG docker $USER`, then log out and back in |
| `build/` and `bins/` are owned by root; `git status`/`rm` fail | You ran the container without `-u $UID` | `sudo chown -R $USER: build bins`, and include `-u $UID` next time |
| Build is very slow on Apple Silicon and toggling the Rosetta setting doesn't help | Expected: Rosetta only accelerates `linux/amd64`; this 32-bit ARM build always runs under QEMU | Nothing to fix — rely on incremental rebuilds; first build ~15 min is normal |
| Weird script/CMake failures on Windows, e.g. `$'\r': command not found` | Repo cloned on the Windows side (`/mnt/c/...`) with CRLF line endings | Re-clone inside the WSL2 filesystem with WSL's git |

Still stuck? [troubleshooting.md](troubleshooting.md) covers failures that happen *after* the build — effects that compile but don't load or misbehave on the device.

Next: [build-flags-reference.md](build-flags-reference.md)
