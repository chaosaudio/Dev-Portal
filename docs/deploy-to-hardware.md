# Deploy to Hardware

This page shows you how to copy a compiled effect (`.so`) onto a **Stratus** pedal or **Nimbus** smart amp over SSH, load it with the Beta app's "9 KNOB" tester, watch the firmware logs to confirm it loaded, and iterate quickly. It assumes you already have a built `.so` — if you don't, do [Build with Docker](build-docker.md) first. The procedure is identical on Stratus and Nimbus: same firmware, same paths, same commands.

This is the *developer testing* path. Releasing your effect to users goes through the Tone Shop — see [Release & Submission](release-and-submission.md).

## Prerequisites

- A compiled effect binary from the [Docker build](build-docker.md) (in `./bins/` after a build).
- The Chaos Audio **Beta app** on your phone — join the [Beta Program](https://chaosaudio.com/pages/beta-program) to get it.
- Your device powered on and on the same network as your computer.
- The developer password for your device (issued with the developer program — apply via the [Developer Application](https://chaosaudio.com/pages/developer-portal), or ask in the developer Discord / support@chaosaudio.com if you don't have it). This documentation never prints device passwords.

## Step 1 — Find your device

Your device runs a small Linux system that announces itself on the local network. Try the mDNS hostname first:

```bash
ping -c 2 stratus.local
```

Expected output (your IP will differ):

```
PING stratus.local (192.168.1.42): 56 data bytes
64 bytes from 192.168.1.42: icmp_seq=0 ttl=64 time=4.1 ms
```

The same hostname and commands apply on Nimbus. If `stratus.local` does not resolve:

- **Use the IP address from the app.** The Chaos Audio app displays the device's IP address on its device/connection screen; substitute that IP for `stratus.local` in every command on this page.
- **USB network.** When connected over USB, the device exposes a gadget network interface. It is typically reachable at `192.168.7.2` (some setups use `192.168.6.2`) — try `ssh root@192.168.7.2` first.

> **Gotcha:** `stratus.local` relies on mDNS. On Windows, mDNS resolution may need the Bonjour service — and inside WSL2, `.local` names usually do not resolve at all even with Bonjour installed on the Windows host, so Windows users should expect to use the IP address from the app. All commands on this page assume a WSL2 shell on Windows (consistent with [Build with Docker](build-docker.md)); if the name won't resolve anywhere, fall back to the raw IP.

**Checkpoint:** `ping` gets replies from the device (or you have its IP address written down).

## Step 2 — First SSH login

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

Optional, but worth 5 seconds per iteration: install your SSH public key so you stop typing the password:

```bash
ssh-copy-id root@stratus.local
```

## Step 3 — Copy your effect to the device

Effects live in one directory on the device, and the firmware loads them strictly by filename: a file named `<EFFECT-ID>.so` is loaded when a preset references that effect ID. Before copying, verify the device has comfortable free space — this directory sits on the same partition the firmware updater and preset writer use, so filling it breaks more than your next `scp`:

```bash
ssh root@stratus.local 'df -h /opt/update'
```

Periodically delete every stray GUID-shaped test file *you* created in earlier sessions, not only today's — and never anything else in the directory.

Now copy your binary there with `scp`, renaming it in the same step:

```bash
scp <YOUR-EFFECT>.so root@stratus.local:/opt/update/sftp/firmware/effects/<EFFECT-ID>.so
```

- `<YOUR-EFFECT>.so` — the binary you built, e.g. `./bins/subspace.so` from the Docker build.
- `<EFFECT-ID>` — the effect's ID (a GUID). For local testing you can use any GUID-shaped name, but for the app-based testing flow below use the **9 KNOB tester ID**: `55631e3a-94f7-42f8-8204-f5c6c11c4a21`. The real production ID is assigned by the Tone Shop at submission — see [Release & Submission](release-and-submission.md).

So the typical test deploy is:

```bash
scp <YOUR-EFFECT>.so root@stratus.local:/opt/update/sftp/firmware/effects/55631e3a-94f7-42f8-8204-f5c6c11c4a21.so
```

Expected output:

```
<YOUR-EFFECT>.so                              100%   48KB   1.2MB/s   00:00
```

> **Warning:** never `scp` directly over a `.so` that is in the currently selected preset while the device is playing. `scp` truncates the destination file in place, and the firmware keeps every loaded `.so` memory-mapped for the life of the process (see Step 4) — truncating a mapped file can crash the audio process mid-note, *before* you even restart. If the effect is in the active chain, either select a preset that doesn't use it first, or copy to a temp name and atomically swap:
>
> ```bash
> scp <YOUR-EFFECT>.so root@stratus.local:/tmp/fx.so && \
> ssh root@stratus.local 'mv /tmp/fx.so /opt/update/sftp/firmware/effects/<EFFECT-ID>.so && chown update:sftponly /opt/update/sftp/firmware/effects/<EFFECT-ID>.so && systemctl restart bela_startup'
> ```
>
> `mv` on the same filesystem replaces the directory entry without touching the old file's contents, so the running audio process keeps its old mapping intact until the restart.

> **Warning:** NEVER create, edit, or delete a `.version` file under `/opt/update/sftp/firmware/`. That file is the update system's marker — touching it triggers a **full firmware reinstall**.

> **Warning:** do not rename, overwrite, or delete the *other* `.so` files in `/opt/update/sftp/firmware/effects/`. They are the user's installed effects; the firmware reports an effect as installed purely by file existence, and a preset that references a missing effect will refuse to assemble. Only ever touch the file you put there.

**Checkpoint:** `ssh root@stratus.local 'ls -l /opt/update/sftp/firmware/effects/55631e3a-*'` shows your file with today's transfer size.

## Step 4 — Fix ownership and restart the audio service

The effects directory is managed by the device's update/SFTP system, so your file must be owned by `update:sftponly`. The firmware also caches the `dlopen` handle of every loaded `.so` for the life of the process, so a replaced binary is only picked up after restarting the audio service (`bela_startup`):

```bash
ssh root@stratus.local 'chown update:sftponly /opt/update/sftp/firmware/effects/<EFFECT-ID>.so && systemctl restart bela_startup'
```

- `<EFFECT-ID>` — same ID you used in Step 3.

Audio drops for a few seconds while the service restarts, then the device reconnects to the app.

**Checkpoint:** the device's audio comes back and the app reconnects within ~10–15 seconds.

## Step 5 — Audition it with the 9 KNOB tester

In the Beta app, under the **"Development"** category, there is an effect titled **"9 KNOB"**. It exposes 9 parameters, each with a range of **0 to 10 and a step of 0.1**, and it maps to effect ID `55631e3a-94f7-42f8-8204-f5c6c11c4a21` — which is why you named your file exactly that in Step 3. Add "9 KNOB" to a preset, select the preset, and you are hearing *your* code with 9 live knobs.

Because the tester only sends 0–10, design your test build's knob handling around that range (0–10, 5 = noon is the platform convention — see [DSP Contract](dsp-contract.md)). Knob values arrive raw and unclamped; validate them in your effect regardless.

> **Warning:** do NOT tap "install" on the 9 KNOB effect in the app — it downloads the real tester binary and **overwrites your file**.

**Checkpoint:** engaging the 9 KNOB effect in the app audibly runs your DSP; turning the app knobs changes it.

## Step 6 — Read the logs

The firmware logs everything about effect loading to the `bela_startup` journal. In a second terminal, follow it live:

```bash
ssh root@stratus.local 'journalctl -u bela_startup -f'
```

Now select the preset containing your effect (or restart the service) and watch. A **healthy load** looks like this — these strings come from the firmware's effect loader (`effectsLoadingService.cpp`) and chain assembler (`effectsManipulationService.cpp`):

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
  ./55631e3a-94f7-42f8-8204-f5c6c11c4a21.so: undefined symbol: __expf_finite
  Effect does not exist.
  ```

  The effect silently vanishes from the chain — no crash, just no effect. The classic cause of the `__expf_finite` flavor is building with `-ffast-math` but without `-fno-finite-math-only`; see [Build Flags Reference](build-flags-reference.md). Other undefined symbols usually mean you built outside the provided container against a too-new toolchain — see [Troubleshooting](troubleshooting.md).

- `The 'setSampleRate' method does not exist in the provided object.` — harmless notice; `setSampleRate` is an optional export.

- `Underrun detected` — your chain blew the 2.9 ms audio deadline (CPU overload or a real-time-safety violation in `compute()`). See [RT Safety](rt-safety.md) and measure properly per [Verification](verification.md).

**Checkpoint:** you can point at the "Effect found" and "Effects vector assembled" lines for your effect in the live journal.

## Step 7 — Iterate fast

After the first setup, the whole edit-test cycle is one rebuild plus this one-liner (about 20 seconds end to end, most of it the service restart):

```bash
scp <YOUR-EFFECT>.so root@stratus.local:/opt/update/sftp/firmware/effects/<EFFECT-ID>.so && \
ssh root@stratus.local 'chown update:sftponly /opt/update/sftp/firmware/effects/<EFFECT-ID>.so && systemctl restart bela_startup'
```

> **Warning:** if the effect you are replacing is in the *currently playing* preset, use the copy-to-temp-and-`mv` variant from the Step 3 warning instead of this direct `scp` — overwriting a mapped `.so` in place can crash live audio before the restart.

Keep the `journalctl -f` terminal from Step 6 open the whole session — restarting `bela_startup` does not interrupt it, and every reload's "Effect found" line (or dlopen error) appears there immediately.

> **Gotcha:** the restart is not optional. The firmware caches each loaded `.so` handle for the life of the process, so an scp'd replacement is ignored until `bela_startup` restarts.

## Step 8 — Clean up

When you're done testing, remove your test binary so the 9 KNOB slot goes back to normal (the user can reinstall the real tester from the app), and restart once more:

```bash
ssh root@stratus.local 'rm /opt/update/sftp/firmware/effects/55631e3a-94f7-42f8-8204-f5c6c11c4a21.so && systemctl restart bela_startup'
```

Remove **only** files you deployed yourself — that includes any stray GUID-shaped test files left over from *earlier* sessions, not just today's. Everything else in that directory belongs to the device.

**Checkpoint:** the file is gone from `/opt/update/sftp/firmware/effects/` and the device boots its normal effect set.

## Quick reference

| Item | Value |
| --- | --- |
| Effects directory | `/opt/update/sftp/firmware/effects/` |
| Required filename | `<EFFECT-ID>.so` (GUID, exact) |
| 9 KNOB tester ID | `55631e3a-94f7-42f8-8204-f5c6c11c4a21` |
| Required ownership | `update:sftponly` |
| Audio service | `bela_startup` (`systemctl restart bela_startup`) |
| Live logs | `journalctl -u bela_startup -f` |
| Healthy load lines | `Version check found 2.0.0.` / `Effect found with name:` (+ .so path) / `Effects vector assembled. size = N` |
| Never touch | `/opt/update/sftp/firmware/.version` |

Next: [Verification](verification.md)
