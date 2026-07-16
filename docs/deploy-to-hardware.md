# Test on Hardware

This page shows you how to run a compiled effect (`.so`) on a **Stratus** pedal or **Nimbus** smart amp: upload the binary to the [FX Builder](https://build.chaosaudio.com), give it a quick UI, **publish privately**, and install it from the Chaos Audio app like any other effect. It also covers watching the firmware logs over SSH to confirm the load and catch errors. It assumes you already have a built `.so` — if you don't, do [Build with Docker](build-docker.md) first. The procedure is identical on Stratus and Nimbus: same firmware, same platform, same app.

This is the *developer testing* path. Releasing your effect to users goes through the Tone Shop — see [Release & Submission](release-and-submission.md).

> **Gotcha:** if you wrote your effect in FAUST, you don't need this page to hear it — the FX Builder can **audition FAUST effects in the browser** (built-in audio test with knobs) before you ever touch hardware. In-browser audition applies to FAUST/code drafts only; uploaded prebuilt binaries are auditioned on hardware via private publish, as below.

## Prerequisites

- A compiled effect binary from the [Docker build](build-docker.md) (in `./bins/` after a build).
- A free **Chaos Audio account** at [build.chaosaudio.com](https://build.chaosaudio.com) — it's the same account as the mobile app. Sign up in the FX Builder, or use "Reset Password" if you already have an app account.
- The **Chaos Audio app** on your phone, signed into the same account.
- Your device powered on and paired with the app.
- *Only for the optional log-watching section:* a USB **data** cable and the developer SSH password for your device (ask in the developer Discord / support@chaosaudio.com if you don't have it). This documentation never prints device passwords.

## Step 1 — Upload your binary to the FX Builder

Sign in at [build.chaosaudio.com](https://build.chaosaudio.com), create a new effect, and upload your `.so` (e.g. `./bins/subspace.so` from the Docker build). The upload must be a **32-bit ARM shared object under 5 MB** — anything the provided container produces qualifies.

You never pick an effect ID. The platform assigns your effect's **Effect ID (GUID)** when the effect is created, and names the hosted binary `<EFFECT-ID>.so` itself — see [Release & Submission](release-and-submission.md).

> **Warning:** the upload check is only "32-bit ARM shared object under 5 MB" — the platform does **not** validate your exports, ABI, or CPU cost, and shows a default CPU estimate for uploaded binaries. A binary that uploads fine can still fail to load or blow the audio deadline on hardware. The [Verification](verification.md) checklist remains your safety net; the log-watching section below is how you see the failure.

**Checkpoint:** the FX Builder accepts your upload and shows the binary attached to your effect.

## Step 2 — Give it a quick UI in the UI Builder

Your effect needs a UI before it can be published. In the **UI Builder**, add your knobs, switches, and LED over a background; knobs and switches accept classic rotating/two-state assets or film strips of 2–128 frames, and the store images can be auto-generated. For a test build, minimal is fine — polish it later for release (see [Release & Submission](release-and-submission.md) for asset requirements).

Keep your knob handling designed around the platform's 0–10 range (5 = noon is the platform convention — see [DSP Contract](dsp-contract.md)). Knob values arrive raw and unclamped; validate them in your effect regardless.

## Step 3 — Publish privately

Publish the effect **privately**. Private publish is instant — no review — and the effect appears **only in your own account's library** in the Chaos Audio app. Nobody else can see or install it.

**Checkpoint:** the effect shows up in your library in the Chaos Audio app.

## Step 4 — Install and play

Install the effect on your Stratus or Nimbus from the app like any other effect, add it to a preset, select the preset, and you are hearing *your* code with live knobs.

**Checkpoint:** engaging your effect in the app audibly runs your DSP; turning the app knobs changes it.

## Iterating

The edit–test cycle is: rebuild, upload the new binary to the same effect in the FX Builder (or edit your FAUST there), re-publish privately, and update the effect from the app. No cables, no file management — the platform and the app handle delivery to the device.

## Watching the firmware logs (optional, advanced)

Once an effect is installed via private publish, it lives on-device as `<EFFECT-ID>.so` under `/opt/update/sftp/firmware/effects/`, with its real platform-assigned ID — no manual file placement, ever. SSH access lets you watch the firmware load it (or fail to), which is invaluable when an uploaded binary misbehaves, and it is how you measure real CPU cost per [Verification](verification.md).

### Find your device

Stratus and Nimbus have **no Wi-Fi** and never join your network. Instead, when you connect the device to your computer with a **USB cable**, it presents a small "USB gadget network" over the cable — a private, two-machine network that exists only between your computer and the device. That link is how all `ssh` in these docs works.

Plug the device into your computer over USB (data-capable cable), give it ~30 seconds after power-on, then try the mDNS hostname:

```bash
ping -c 2 stratus.local
```

Expected output (your IP will differ):

```
PING stratus.local (192.168.1.42): 56 data bytes
64 bytes from 192.168.1.42: icmp_seq=0 ttl=64 time=4.1 ms
```

(The IP you see will be one of the fixed USB-network addresses below.) The same hostname and commands apply on Nimbus. If `stratus.local` does not resolve:

- **Use the fixed USB-network address.** The device always sits at a known address on the USB link: try `ssh root@192.168.7.2` first, then `ssh root@192.168.6.2` (which one is live depends on your OS's USB-networking driver). Substitute whichever answers for `stratus.local` in every command on this page.
- **Check the cable and the interface.** A charge-only cable presents no network at all. On macOS a new "network interface" appears in System Settings → Network when the link is up; on Linux, `ip addr` shows a new `usb`/`en` interface with a `192.168.6.x` or `192.168.7.x` address.

> **Gotcha:** `stratus.local` relies on mDNS over the USB link. On Windows, mDNS may need the Bonjour service — and inside WSL2, `.local` names usually do not resolve at all even with Bonjour on the Windows host, so Windows users should expect to use the fixed address (`192.168.7.2`) directly. All commands on this page assume a WSL2 shell on Windows (consistent with [Build with Docker](build-docker.md)).

**Checkpoint:** `ping` gets replies from the device (or you have its IP address written down).

### First SSH login

Connect as `root`:

```bash
ssh root@stratus.local
```

The **first** time you connect to any device, SSH shows a host-key prompt that looks alarming but is normal:

```
The authenticity of host 'stratus.local (192.168.1.42)' can't be established.
ED25519 key fingerprint is SHA256:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.
This key is not known by any other names.
Are you sure you want to continue connecting (yes/no/[fingerprint])?
```

Type `yes` and press Enter. SSH remembers the device from now on. Then enter the developer password for your device when prompted (it is not echoed as you type).

```
root@stratus.local's password:
```

**Checkpoint:** you have a root shell on the device — the prompt changes to something like `root@stratus:~#`.

> **Gotcha:** the device's clock may show a wildly wrong date (`date` says 2019, or January 1st). There is no battery-backed real-time clock — this is harmless. It also means `journalctl` timestamps can look odd; don't try to "fix" anything.

> **Gotcha:** if you later see `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!`, it usually means the device was reflashed/reinstalled and its host key changed. Clear the old key and reconnect:
>
> ```bash
> ssh-keygen -R stratus.local
> ```

Optional, but worth 5 seconds per session: install your SSH public key so you stop typing the password:

```bash
ssh-copy-id root@stratus.local
```

### Read the logs

The firmware logs everything about effect loading to the `bela_startup` journal. Follow it live:

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Now select the preset containing your effect and watch. A **healthy load** looks like this — these strings come from the firmware's effect loader (`effectsLoadingService.cpp`) and chain assembler (`effectsManipulationService.cpp`):

```
Version check found 2.0.0.
Effect found with name:
/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
Effects vector assembled. size = 1
```

The "name" the loader prints is the full path of your `.so`, not your effect's display name. `size` is the number of effects in the assembled chain (yours plus anything else in the preset).

**Failure signatures to know:**

- `dlopen` failure — a linker error message followed by:

  ```
  ./<EFFECT-ID>.so: undefined symbol: __expf_finite
  Effect does not exist.
  ```

  The effect silently vanishes from the chain — no crash, just no effect. The classic cause of the `__expf_finite` flavor is building with `-ffast-math` but without `-fno-finite-math-only`; see [Build Flags Reference](build-flags-reference.md). Other undefined symbols usually mean you built outside the provided container against a too-new toolchain — see [Troubleshooting](troubleshooting.md).

- `The 'setSampleRate' method does not exist in the provided object.` — harmless notice; `setSampleRate` is an optional export.

- `Underrun detected` — your chain blew the 2.9 ms audio deadline (CPU overload or a real-time-safety violation in `compute()`). See [RT Safety](rt-safety.md) and measure properly per [Verification](verification.md).

**Checkpoint:** you can point at the "Effect found" and "Effects vector assembled" lines for your effect in the live journal.

### Hands off the effects directory

Everything under `/opt/update/sftp/firmware/effects/` is placed and owned by the platform now — treat the whole directory as read-only.

> **Warning:** NEVER create, edit, or delete a `.version` file under `/opt/update/sftp/firmware/`. That file is the update system's marker — touching it triggers a **full firmware reinstall**.

> **Warning:** do not rename, overwrite, or delete the `.so` files in `/opt/update/sftp/firmware/effects/`. They are installed effects; the firmware reports an effect as installed purely by file existence, and a preset that references a missing effect will refuse to assemble.

If you hand-deployed GUID-named test files under an earlier version of these docs, delete the ones *you* created — and nothing else in the directory.

## Quick reference

| Item | Value |
| --- | --- |
| Effects directory | `/opt/update/sftp/firmware/effects/` |
| Binary name on device | `<EFFECT-ID>.so` — ID assigned by the platform, file placed by private-publish install |
| Ownership | Managed by the platform — never `chown` or replace files by hand |
| Audio service | `bela_startup` |
| Live logs | `journalctl -u bela_startup -f` |
| Healthy load lines | `Version check found 2.0.0.` / `Effect found with name:` (+ .so path) / `Effects vector assembled. size = N` |
| Never touch | `/opt/update/sftp/firmware/.version` |

Next: [Verification](verification.md)
