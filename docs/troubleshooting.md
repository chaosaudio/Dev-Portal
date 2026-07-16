# Troubleshooting

This page is a symptom-indexed reference for everything that commonly goes wrong between "my effect compiles" and "my effect works on the pedal". It applies identically to **Stratus** and **Nimbus** — same CPU, same firmware, same effect format; the same hostname and commands shown for `stratus.local` work for Nimbus the same way. Find your symptom in the index, then follow the Symptom / Cause / Fix / How to confirm structure of each entry.

## Symptom index

| Symptom | Jump to |
|---|---|
| Effect never appears in the chain / preset stays silent | [1](#1-effect-never-appears-in-the-chain) |
| Knobs do nothing | [2](#2-knobs-do-nothing) |
| 3-way switch acts backwards | [3](#3-the-3-way-switch-acts-backwards) |
| Audio dropouts, crackles, CPU spikes | [4](#4-audio-dropouts) |
| Crash when changing presets | [5](#5-crash-at-preset-change) |
| Crash the moment the effect loads | [6](#6-crash-at-load) |
| Effect sounds wrong only in 16-sample chunks | [7](#7-effect-sounds-wrong-only-at-16-sample-chunks) |
| Docker container build fails | [8](#8-container-build-fails) |
| ssh / scp to the device refused | [9](#9-sshscp-refused) |

## First move: read the log

Almost every load-time failure explains itself in the firmware journal. Before anything else, open a live log on the device and reproduce the problem:

```
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Use the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it).

A **successful** load prints:

```
Version check found 2.0.0.
Effect found with name:
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
```

`<EFFECT-ID>` is the GUID your `.so` is named after (see [deploy-to-hardware.md](deploy-to-hardware.md)).

These lines appear only the **first** time the `.so` is loaded after a service restart; on later preset loads the cached handler is reused silently — restart `bela_startup` if you need to see the load lines again.

**Checkpoint:** you have a live journal tail in one terminal and can trigger the failing action (preset load, knob turn, etc.) from the app or pedal. Every entry below tells you what to look for in that tail.

---

## 1. Effect never appears in the chain

**Symptom:** you copied the `.so` to the device, restarted `bela_startup`, selected the preset — and your effect is simply absent. No crash, no error in the app; the chain behaves as if the effect were never installed.

The firmware loads effects with `dlopen(..., RTLD_NOW)` from `/opt/update/sftp/firmware/effects/<EFFECT-ID>.so`. With `RTLD_NOW`, **every** undefined symbol must resolve at load time — one unresolvable symbol and the effect is silently dropped from the chain. The only evidence is in the journal. There are four common causes; each produces a distinct journal line.

### 1a. `__expf_finite` undefined — missing `-fno-finite-math-only`

**Symptom:** journal shows, at preset load:

```
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so: undefined symbol: __expf_finite
Effect does not exist.
```

(The exact symbol varies: `__expf_finite`, `__logf_finite`, `__powf_finite`, …)

**Cause:** you compiled with `-ffast-math` but without `-fno-finite-math-only`. Plain `-ffast-math` makes GCC emit calls to `__*_finite` libm entry points that the device's libm does not export, so `dlopen(RTLD_NOW)` fails. This is the single most common "my effect vanished" cause, and it is why `-fno-finite-math-only` is load-bearing in the official flag set (see [build-flags-reference.md](build-flags-reference.md)). The repo's `examples/CMakeLists.txt` release flags already include it:

```
-fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon
-ftree-vectorize -ffast-math ... -fno-finite-math-only
```

**Fix:** add `-fno-finite-math-only` after `-ffast-math` and rebuild. If you use the repo's CMake/Docker flow ([build-docker.md](build-docker.md)) you already have it — this bites people who hand-roll a `g++` line.

**How to confirm:** before deploying, check the binary for finite-math imports (run inside the build container or on any Linux box):

```
nm -D --undefined-only <YOUR-EFFECT>.so | grep _finite
```

`<YOUR-EFFECT>.so` is your compiled plugin. Expected output after the fix: nothing. Any `__*_finite` line means the `.so` will not load on the device.

### 1b. glibc too new

**Symptom:** journal shows a version-mismatch dlerror, then the drop line:

```
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so: /lib/arm-linux-gnueabihf/libc.so.6: version `GLIBC_2.34' not found (required by /opt/update/sftp/firmware/effects/<EFFECT-ID>.so)
Effect does not exist.
```

(The exact `GLIBC_2.3x` version depends on the distro you built on.)

**Cause:** you built on a distro newer than the device's runtime. Devices run glibc 2.31 (Debian Bullseye era); a `.so` built against a newer glibc references versioned symbols the device doesn't have.

**Fix:** build inside the provided container ([build-docker.md](build-docker.md)) — it is `debian:bullseye-slim` (see `Dockerfile` at the repo root), which pins the glibc floor for you. Don't build device binaries directly on a recent Ubuntu/Fedora/Arch host toolchain.

**How to confirm:**

```
objdump -T <YOUR-EFFECT>.so | grep -o 'GLIBC_2\.[0-9]*' | sort -Vu | tail -1
```

Expected output: `GLIBC_2.31` or lower.

### 1c. Missing `create` export

**Symptom:** journal shows the effect **was** found, then:

```
Effect found with name:
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
Failed to find 'create' symbol in library
```

**Cause:** your plugin does not export the mandatory factory function. The firmware `dlsym`s `create` and refuses the effect without it. C++ name mangling is the usual culprit — `dsp* create()` without `extern "C"` exports a mangled name the firmware can't find.

**Fix:** every plugin must contain, at file scope (see [dsp-contract.md](dsp-contract.md)):

```cpp
extern "C" dsp* create() {
    return new MyEffect();
}
```

This is exactly what the shipped example does — `examples/equalizer/equalizer.cpp` ends with `extern "C" dsp* create() { return new Equalizer(); }` plus `extern "C" const char* dsp_version = DSP_VERSION;`.

**How to confirm:**

```
nm -D <YOUR-EFFECT>.so | grep -w create
```

Expected output (a defined `T` symbol, unmangled):

```
000012a4 T create
```

If you see nothing, or a mangled name like `_Z6createv`, the `extern "C"` is missing.

### 1d. Wrong filename

**Symptom:** no dlopen error at all — often no log lines for your effect whatsoever, and the app may report the effect as not installed. If the name is close-but-wrong you may instead see:

```
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so: cannot open shared object file: No such file or directory
Effect does not exist.
```

**Cause:** the file must be named exactly `<EFFECT-ID>.so` — the GUID the preset references, nothing else. `resources/compilation_flags.txt` spells this out (`-o "Effect ID".so`). Two extra wrinkles:

- The firmware reports an effect as "installed" purely on file existence, and **a preset containing any missing effect is not assembled at all** — so one misnamed file can make the whole preset load empty.
- A same-GUID `.namb`/`.nam` (NAM model) or `.wav` (IR) file in the effects directory **shadows** your `.so`: the loader prefers those file types for a given GUID.

**Fix:** name the file exactly `<EFFECT-ID>.so` (case-sensitive, no suffixes like `-v2`), and make sure no `.nam`/`.namb`/`.wav` with the same GUID sits in `/opt/update/sftp/firmware/effects/`. For bench testing with the Beta app's "9 KNOB" tester, the name must be exactly `55631e3a-94f7-42f8-8204-f5c6c11c4a21.so` — see [deploy-to-hardware.md](deploy-to-hardware.md).

**How to confirm:**

```
ssh root@stratus.local 'ls -l /opt/update/sftp/firmware/effects/ | grep -i <EFFECT-ID>'
```

You should see exactly one file, `<EFFECT-ID>.so`, owned by `update:sftponly`.

---

## 2. Knobs do nothing

**Symptom:** the effect loads and passes (processed) audio, but turning a knob in the app changes nothing.

Three distinct causes, in descending order of frequency:

### 2a. FAUST widget not bound — missing/malformed `[stratus:N]`

**Cause:** in the FAUST path, a slider binds to a hardware knob **only** via the label metadata `[stratus:N]` where `N` is a single digit `0`–`9`. Anything else — `[knob:0]`, a bare `stratus:0` label, `[stratus:switch:0]`, `[stratus:10]` — compiles fine, produces a working effect, and the widget is **silently unbound**: it freezes at its FAUST init value forever. This is the number-one FAUST support issue.

**Fix:** put `[stratus:N]` inside every slider label:

```
gain = hslider("Gain[stratus:0][style:knob]", 0.5, 0, 1, 0.01);
```

Rules: sliders → knobs (max 10, indices 0–9); buttons/checkboxes → switches (max 5, indices 0–4); `nentry` binds as a switch only with `min=0`, `max=1` or `2`, `step=1`. See [guide-faust.md](guide-faust.md) for the full binding table.

**How to confirm:** every widget you expect to be live has a `[stratus:N]` with a unique single digit in its label string. If two widgets share an index, the first registered wins and the second is dropped.

### 2b. Wrong knob index (hand-written C++)

**Cause:** the app sends knob 0, your code reads `knobs[1]` — or your UI metadata maps a control to an index your `compute()` never reads. `knobs[]` is a plain `float knobs[MAXKNOBS]` array (`MAXKNOBS` is 10, `resources/dsp.hpp`); nothing warns you about reading the wrong slot, it just sits at its default of `0.5`.

**Fix:** cross-check the index your app-side control metadata declares against the index you read in code. With the "9 KNOB" tester effect, knob positions map to indices 0–8 (verify the exact ordering with a quick per-index test), sending values 0–10.

**How to confirm:** temporarily log knob values **outside** the audio path — e.g. print `knobs[0..9]` from `instanceConstants()` (which runs at chain assembly, off the audio thread), redeploy, and watch the journal while re-loading the preset. Never `printf` from `compute()` — see [rt-safety.md](rt-safety.md).

### 2c. Knob read only in the constructor

**Cause:** you compute your gain/coefficients from `knobs[]` once, in the constructor or only in `instanceConstants()`. But the host sets knobs **after** `instanceConstants()` at every chain assembly, and live knob turns call `setKnob()` from the controller thread at any time while `compute()` runs. Values captured at construction are stale forever — and instances are pooled and never destroyed, so the constructor runs exactly once per instance, ever.

**Fix:** read `knobs[]` (or a value cached by an overridden `setKnob()`) inside `compute()`, re-deriving expensive coefficients only when the value actually changed. Validate every read: knob values arrive raw and unclamped from presets/BLE (real presets have contained `128` where 0–10 was expected) — clamp or fall back to a default on out-of-range/NaN. See [dsp-contract.md](dsp-contract.md).

**How to confirm:** turn a knob while audio runs. If the sound changes only after switching presets away and back, you are capturing knob state too early.

---

## 3. The 3-way switch acts backwards

**Symptom:** your 3-position switch (or stomp logic) behaves inverted or scrambled — "down" selects what you coded as middle, etc.

**Cause:** two traps in `resources/dsp.hpp`, and they compound:

1. The enum order is not the intuitive one:

   ```cpp
   enum SWITCH_STATE{
       UP = 0,
       DOWN = 1,
       MIDDLE = 2
   };
   ```

   `DOWN` is 1 and `MIDDLE` is 2 — not the physical top-to-bottom order. For the stomp switch this also means **UP = 0 = bypassed**, DOWN = 1 = engaged: the base-class `stompSwitchPressed()` only calls `compute()` when `stompSwitch` is nonzero.

2. The debug helper `getTextForEnum()` in the same header **contradicts the enum** — it prints `1` as `"MIDDLE"` and `2` as `"DOWN"`:

   ```cpp
   else if(enumVal == 1)//SWITCH_STATE::MIDDLE)
       *out = "MIDDLE";
   else if(enumVal == 2)//SWITCH_STATE::DOWN)
       *out = "DOWN";
   ```

   If you calibrated your switch logic by that printer's output, you wired it backwards.

**Fix:** trust the enum, never the debug printer. Write `switch` statements against `dsp::UP` / `dsp::DOWN` / `dsp::MIDDLE` by name, not against remembered numbers. In FAUST, a 3-position `nentry(..., 0, 0, 2, 1)` bound with `[stratus:N]` delivers 0/1/2 with the same UP/DOWN/MIDDLE meaning.

**How to confirm:** hardcode a distinct, obvious behavior per named enum value (e.g. `UP` = silence, `DOWN` = full wet, `MIDDLE` = half gain), deploy, and flip the physical/app switch through all three positions.

---

## 4. Audio dropouts

**Symptom:** clicks, crackles, or momentary silence — either constantly, or in bursts tied to some action, or "randomly" when playing quietly.

There are three different diseases with the same cough. Tell them apart with the Xenomai scheduler stats on the device:

```
ssh root@stratus.local 'cat /proc/xenomai/sched/stat'
```

Representative (trimmed) output — the columns you care about are `MSW` (mode switches) and `%CPU`:

```
CPU  PID    MSW        CSW        XSC        PF    STAT       %CPU  NAME
  0  1234   1          52001      104213     0     W          61.2  bela-audio
  0  1250   340        1201       2400       0     W           2.1  bela-aux
```

(Representative, trimmed — thread names and counts will differ on your unit.)

Run it twice, ~10 seconds apart, while reproducing the dropout, and compare.

### 4a. Mode switches — allocation/syscalls in `compute()`

**Cause:** heap allocation (`new`, `malloc`, `std::vector` growth, `std::string`), locks, file/console I/O (`printf`, spdlog), or any syscall on the Xenomai audio thread demotes it to secondary mode — an audible dropout every time.

**How to tell:** the `MSW` column for `bela-audio` **increments between your two readings** while audio is running. A healthy audio thread's MSW stays frozen (a small constant count from startup is normal). Dropouts correlate with specific events — a knob turn that triggers a lazy allocation, a preset change, the first time a code path runs.

**Fix:** pre-allocate everything in the constructor / `instanceConstants()`; nothing on the RT list inside `compute()`. Full rules and the audit checklist: [rt-safety.md](rt-safety.md).

### 4b. CPU over budget

**Cause:** your effect is simply too expensive. The whole chain shares one 2.9 ms callback (128 frames at 44.1 kHz) on a single Cortex-A8 core that also runs Bluetooth, LEDs, and the OS.

**How to tell:** `MSW` is stable but `%CPU` of `bela-audio` is high — compare it with your effect in the chain vs. removed. As guidance, a single effect should stay under ~15% of the core (chains stack; the heaviest first-party effect uses ~40% and dominates its preset). Sustained overload also prints `Underrun detected` lines in `journalctl -u bela_startup`.

**Fix:** optimize (NEON, cheaper algorithms, lower-order filters), or reduce per-sample work by hoisting per-block computation. Measure properly per [verification.md](verification.md) — and note that any userspace benchmark run on-device is preempted by the audio thread, so stop `bela_startup` first or your numbers read 2–3x high.

### 4c. Denormal tails

**Cause:** decaying filters/reverb/delay tails drift into subnormal floats, which cost 10–100x per operation on Cortex-A8 — the classic "CPU spikes when the guitar goes quiet".

**How to tell:** `%CPU` is fine while playing, then **spikes during silence or long decays**, and dropouts happen at the tail end of notes. MSW stays stable.

**Fix:** the firmware sets FTZ/DN on the audio thread and the official builds use `-ffast-math`, so on-device you're mostly protected — but don't rely on it: add a tiny DC offset to feedback paths or snap state to zero below ~1e-6 on long decays, especially if your code may also run on other hosts. See [rt-safety.md](rt-safety.md).

---

## 5. Crash at preset change

**Symptom:** audio runs fine, but switching presets (especially back and forth to a preset containing your effect) crashes the firmware or produces a blast of garbage audio.

**Cause:** instance pooling. Effect instances are pooled per effect ID and **never destroyed** — your destructor never runs, and the next preset that uses your effect gets the **same object back, with all its old state**. `instanceConstants()` is called at every chain assembly precisely so you can reset that dirty instance. Three failure flavors:

- **`instanceConstants()` doesn't reset everything.** Stale write indices, envelope states, or counters from the previous life read/write out of range or explode filters. `instanceConstants()` must deterministically re-zero *all* DSP state: delay lines, filter states, envelopes, ring-buffer indices, sample counters. (Don't treat it as "set defaults once" — treat it as "reset DSP state". Leave `knobs[]` alone conceptually; the host overwrites knobs right after anyway.)
- **State that only the constructor initializes.** The constructor runs once per pooled instance, ever; anything it half-initializes that `compute()` then mutates is dirty on reuse.
- **Mutable file-scope/static state + trails.** When trails are enabled, retired chain instances keep being `compute()`d on a low-priority worker thread while the new chain runs on the audio thread. Two instances of your `.so` touching shared `static`/global mutable state concurrently has corrupted the heap in the field. Keep all mutable state in instance members.

**Fix:** audit `instanceConstants()` against every member your `compute()` mutates; move any file-scope mutable state into the class. Contract details: [dsp-contract.md](dsp-contract.md).

**How to confirm:** cycle presets A→B→A twenty times with audio running (`journalctl -u bela_startup -f` open). If the crash only reproduces with trails enabled on the effect, suspect static/global state specifically.

---

## 6. Crash at load

**Symptom:** the journal shows the effect being found —

```
Effect found with name:
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
```

— and then the service dies (journal shows `bela_startup` restarting, or a SIGSEGV) at or immediately after preset assembly.

Three causes:

### 6a. Vtable mismatch — you rolled your own class layout

**Cause:** the firmware dispatches virtual calls **by vtable slot, not by name**. The slot order defined by `resources/dsp.hpp` is the ABI: `setKnob, getKnob, setSwitch, getSwitch, setStompSwitch, getStompSwitch, stompSwitchPressed, instanceConstants, compute, benchmark`. If you wrote your own base class, added a virtual method before `compute()`, or reordered anything, the firmware's `instanceConstants()` call lands on some other method — often `compute()` with garbage arguments — and crashes.

**Fix:** subclass `struct dsp` from `resources/dsp.hpp` verbatim and only override the declared virtuals; add nothing virtual before `benchmark`. Also: a hand-written C++ effect must **not** export a symbol named `getExtensions` — the firmware treats that as the FAUST-pipeline marker and *skips* `instanceConstants()` entirely, so your setup never runs. See [dsp-contract.md](dsp-contract.md).

### 6b. Wrong `dsp.hpp`

**Cause:** you built against a stale copy of the header. Older checkouts shipped a v1-era `examples/dsp.hpp` (no `DSP_VERSION`, no `filePaths`/trails members) with a **different object layout** — the firmware, compiled against the real header, then reads `knobs[]`/`switches[]` at wrong byte offsets: garbage knob values at best, crash at worst.

**Fix:** the ONLY authoritative header is `resources/dsp.hpp` (byte-identical to the firmware's copy). Point your include path there and nowhere else.

**How to confirm:**

```
grep DSP_VERSION $(dirname <path-you-include>)/dsp.hpp
```

Expected output:

```
#define DSP_VERSION "2.0.0"
```

No match = stale v1 header; fix your include path.

### 6c. Exceptions crossing the ABI

**Cause:** throwing from your constructor, `create()`, or `instanceConstants()` propagates a C++ exception across the `dlopen` boundary into firmware code that does not catch it → `std::terminate` → the whole audio service dies. Mixed `-fno-exceptions` builds make this worse (no unwind tables at all).

**Fix:** never let an exception escape your `.so`. Wrap fallible setup (file loads, parsing) in try/catch inside the plugin and degrade to a safe passthrough state instead of throwing. Also avoid file-scope objects with non-trivial constructors — their initializers run inside `dlopen` itself, before `create()`, where nothing of yours can catch a failure; keep file-scope data const/POD and do all setup in the constructor or `instanceConstants()`.

**How to confirm:** if the crash happens only when a resource file is missing/corrupt, it's almost certainly an escaping exception from your setup path.

---

## 7. Effect sounds wrong only at 16-sample chunks

**Symptom:** the effect sounds correct in your bench harness (which feeds 128-frame blocks) but wrong on the device — or vice versa; artifacts, doubled/backwards modulation, or block-rate zipper noise.

**Cause:** the firmware chose the **legacy chunking path** for your plugin, feeding `compute()` 16-sample chunks (final chunk may be smaller) instead of one full-buffer call — and your code assumes a fixed block size. The chunking decision comes from the `dsp_version` symbol: if it's missing, the journal says so explicitly at load:

```
The 'dsp_version' symbol does not exist in the shared library. Assuming version 1.0.0.
```

Version 1.0.0 (or missing/unparseable) → 16-sample chunks. `"2.0.0"` → single full-buffer `compute()` per callback.

Two sub-bugs to check:

1. **You forgot the export, or exported it as a function.** It must be a **data** symbol:

   ```cpp
   extern "C" const char* dsp_version = DSP_VERSION;   // correct: pointer VARIABLE
   ```

   `extern "C" const char* dsp_version() {...}` (a function) also resolves in `dlsym`, but the firmware dereferences it as data and reads code bytes as a string — undefined behavior.

2. **Your `compute()` assumes `count` is constant.** It must work for **any** `count` from 1 to 1024: per-block state (LFO phase increments, envelope steps) must be derived from `count`, not hardcoded to 128; fixed stack buffers must be bounds-checked or chunked internally; never read/write past `count` samples.

**Fix:** export the data symbol above, and audit `compute()` for block-size assumptions. Then re-run your bench at multiple block sizes.

**How to confirm:**

```
nm -D <YOUR-EFFECT>.so | grep dsp_version
```

Expected output — note the `D` (data), not `T` (function):

```
00004008 D dsp_version
```

And in the journal at load you should now see `Version check found 2.0.0.` instead of the "does not exist" line. For block-size robustness, run the benchmark harness at several sizes per [verification.md](verification.md).

---

## 8. Container build fails

**Symptom:** `docker buildx build` or the compile run inside `chaos-audio-builder` errors out before producing `bins/<YOUR-EFFECT>.so`.

Three usual suspects:

### 8a. Submodules not initialized

**Cause:** the top-level `CMakeLists.txt` adds `featured/aida-x-convolver` and `featured/aida-x-tests` unconditionally, and those (plus example dependencies) live in git submodules. A fresh clone without submodules fails at configure with missing-directory/missing-source errors.

**Fix:**

```
git submodule update --init --recursive
```

This is required again in every new git worktree or fresh CI checkout — submodule contents are per-worktree.

**How to confirm:** `git submodule status` shows no lines prefixed with `-` (uninitialized).

### 8b. Stale `build/` directory

**Cause:** the container's entrypoint (`entrypoint.sh`) runs CMake in `/workdir/build`. If that directory holds a `CMakeCache.txt` from a previous **host** (e.g. you once ran `cmake` natively on your Mac/PC in the same tree), the cached compiler paths poison the container build with errors like `CMake Error: The CMAKE_CXX_COMPILER: ... is not able to compile a simple test program` or is-a-different-compiler complaints.

**Fix:**

```
rm -rf build/
```

then rerun the container.

**How to confirm:** a clean rerun proceeds past configure into actual compilation of `examples/`.

### 8c. binfmt/QEMU not installed (Linux hosts)

**Cause:** the image is `--platform linux/arm` and compilation runs as QEMU-emulated native armhf — not cross-compilation. Docker Desktop (macOS/Windows, including Apple Silicon) ships the QEMU binfmt handlers built in; on a Linux host you must install them once, or the container dies instantly with `exec /entrypoint.sh: exec format error` (or `standard_init_linux.go: exec user process caused: exec format error`).

**Fix (Linux only, once):**

```
docker run --privileged --rm tonistiigi/binfmt --install all
```

**How to confirm:**

```
docker run --rm --platform linux/arm debian:bullseye-slim uname -m
```

Expected output:

```
armv7l
```

Full walk-through of the container flow: [build-docker.md](build-docker.md). Expect ~10 min for the first image build and ~15 min for the first full compile under QEMU — that's emulation, not a hang.

---

## 9. ssh/scp refused

**Symptom:** `scp`/`ssh` to `root@stratus.local` fails — connection refused, timeout, password rejected, or a scary host-key warning.

### 9a. Host key changed

**Symptom:**

```
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
```

**Cause:** the device regenerated its host key (firmware reinstall/factory reset), or `stratus.local` now resolves to a different unit on your bench.

**Fix:**

```
ssh-keygen -R stratus.local
```

then connect again and accept the new key.

**How to confirm:** the next `ssh root@stratus.local` prompts to accept a new fingerprint instead of aborting.

### 9b. Wrong password

**Cause:** the device does not use a public default password, and it is not your Chaos Audio account password. Access uses the developer password for your device (issued with the developer program — ask in the developer Discord/support if you don't have it). Repeated wrong guesses look like `Permission denied, please try again.`

**Fix:** use the issued developer credentials. Never commit them to scripts you share.

### 9c. Device unreachable — asleep, powered down, or off-network

**Symptom:** `ssh: connect to host stratus.local port 22: Operation timed out` or `Could not resolve hostname stratus.local`.

**Cause:** the device is powered down or still booting, the USB cable is charge-only (no data lines — presents no network at all), your OS hasn't brought up the USB network interface, or mDNS (`.local` resolution) isn't available on your system (typical inside WSL2). Remember: Stratus and Nimbus have no Wi-Fi — the connection is always the USB cable.

**Fix:** swap in a known-good data cable and reconnect; give the device ~30 s after power-on; confirm a new USB network interface appeared (macOS: System Settings → Network; Linux: `ip addr`); if `.local` doesn't resolve, use the fixed USB-network address directly — `ssh root@192.168.7.2`, then `192.168.6.2` — in place of `stratus.local`.

**How to confirm:**

```
ping -c 3 stratus.local
```

Expected output: three replies. Resolution failure with a pingable IP = mDNS problem; no replies at all = power/network problem.

> **Gotcha:** while poking around in `/opt/update/sftp/firmware/`, never create or modify a `.version` file there — it triggers a full firmware reinstall on next boot.

---

## Still stuck?

- Re-check the binary contract end to end: [dsp-contract.md](dsp-contract.md)
- Re-check what the runtime actually does to your plugin: [runtime-environment.md](runtime-environment.md)
- Measure before guessing: [verification.md](verification.md)
- Ask in the developer Discord with the relevant `journalctl -u bela_startup` excerpt — the exact log line is usually the whole diagnosis.

Next: [runtime-examples-map.md](runtime-examples-map.md)
