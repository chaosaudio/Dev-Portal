# Release & Submission

This page covers everything around pressing "submit": final binary naming, version-string rules, control-count limits, the store assets you'll need, a pre-flight checklist, the FX Builder submission flow itself, how you get paid, and the legal documents you accept. It's for developers whose effect already builds, runs on hardware ([deploy-to-hardware.md](deploy-to-hardware.md)), and passes verification ([verification.md](verification.md)).

One submission ships to **both** devices. Stratus (the pedal) and Nimbus (the 100 W smart amp) run the same Cortex-A8 CPU, the same firmware architecture, and the same effect format. You submit one `.so`; users of both products get it from Tone Shop™.

## What the platform assigns vs. what you choose

| The platform assigns | You choose |
|---|---|
| **The production Effect ID** — a GUID generated when you create the effect in the FX Builder. You cannot pick it, and you must not invent one. | Effect **name**, **subtitle** (2-3 words), **description**, **author** name (your developer name, set once in Profile Settings — unique across the platform), and **price** (self-serve in the FX Builder: free, or US$0.99–$49.99; change it anytime) |
| The final hosted binary name, `<EFFECT-ID>.so` | Your effect's **version number** (see below) |
| For FAUST submissions: the `stratusId` / `stratusVersion` metadata injected into your source | Your knob/switch layout and all store artwork |

During local development you can name your `.so` anything you like. When you upload it, the FX Builder stores it as `<EFFECT-ID>.so` under the Effect ID it assigned at creation — you never name the shipped file yourself ([deploy-to-hardware.md](deploy-to-hardware.md) covers getting it onto your device for testing).

> **Gotcha:** FAUST authors — do **not** write `declare stratusId`, `declare stratusVersion`, or `declare filename` in your source. The platform injects these from your submission automatically; hand-written values conflict with the injected ones.

## The final `.so` name: `<EFFECT-ID>.so`

The firmware loads effects with `dlopen` from `/opt/update/sftp/firmware/effects/<EFFECT-ID>.so` — the filename **is** the lookup key. The build-output hint in `resources/compilation_flags.txt` says the same thing:

```text
... "Effect Name".cpp -o "Effect ID".so
```

The file must be named exactly `<EFFECT-ID>.so`, where `<EFFECT-ID>` is the GUID the platform assigned when you created the effect in the FX Builder. No prefix (not `lib...so`), no suffix, no version number in the filename.

## Version strings: `dsp_version` is not your effect version

There are **two** versions in play. Confusing them is a common submission mistake.

**1. `dsp_version` — the ABI version. Leave it at `"2.0.0"`.**

This is the exported data symbol the firmware reads to decide how to talk to your plugin (see [dsp-contract.md](dsp-contract.md)). Every example in this repo exports it the same way — from `examples/equalizer/equalizer.cpp`:

```cpp
extern "C" const char* dsp_version = DSP_VERSION;
```

where `DSP_VERSION` is `"2.0.0"` (`resources/dsp.hpp`, line 14). It describes the plugin *interface*, not your effect. Do not bump it when you release v1.1 of your fuzz pedal.

> **Warning:** The firmware compares this string **exactly**. `"2.0.0"` gets the modern path (full-buffer `compute()`, one `setSampleRate(44100)` call at load). A missing symbol means legacy 16-sample chunking. Any other value is unsupported — export `"2.0.0"` verbatim; there is no valid reason to ship anything else.

**2. Your effect version — submission metadata. Must always increase.**

Your effect's own version lives in the submission metadata (and, for FAUST builds, in the platform-injected `stratusVersion`). Per the repo `README.md` ("Public Release to Tone Shop™"): it should be in the format `"0.0.0"` and **MUST always be higher than any previous version** you have submitted. Pick a scheme (e.g. semver) and increment on every resubmission — the store rejects or misbehaves on non-increasing versions.

## Knob / switch limits and label conventions

Hard limits come from the ABI header (`resources/dsp.hpp`, lines 20-21):

```cpp
#define MAXKNOBS 10
#define MAXSWITCHES 5
```

- **Up to 10 knobs** (indices 0-9) and **up to 5 switches** (indices 0-4). Anything beyond these indices can never be bound to the app UI.
- **Knob range convention: 0-10, with 5 = noon.** This is what the app sends and displays. If your DSP wants dB, Hz, or cents, map internally (see [dsp-contract.md](dsp-contract.md)) — and remember values arrive unclamped, so validate them regardless.
- **Keep label text short.** The app renders each knob's label from the artwork/metadata you submit; label text longer than about 9 characters gets truncated in the builder UI. "Drive", "Mix", "Tone" — not "Harmonic Saturation Amount".
- Switches are 2- or 3-position (`UP` / `DOWN` / `MIDDLE`); the app renders them from your switch artwork.

## Assets the store needs (pointer only)

The full, authoritative list lives in the repo `README.md` under **"Public Release to Tone Shop™"**; example artwork ships in `resources/ui_assets/` (knob base/spin layers, footswitch up/down, LED on/off, audio jacks). In summary you will need:

- Pedal **mockup** and square **preview card**
- Pedal **background image** (recommended aspect ratio 230 × 423; no knobs/labels/jacks baked in)
- Per-knob: **base image** (non-rotating), **knob image** (rotating), **label image**
- Stomp switch **pressed** and **released** images
- **LED ON** and **LED OFF** images
- **Audio jack** image(s), usually two

You upload all of these in the FX Builder's **UI Builder**. Two conveniences worth knowing: the UI Builder can **auto-generate** the store background/mockup/preview set for you, and knobs/switches accept either classic assets (rotating knob layer, two-state switch images) or **film strips of 2–128 frames** for fully custom animation.

## Pre-submission checklist

Everything here should already be green from [verification.md](verification.md) — re-run it against the **final** binary you intend to submit, not an earlier build.

1. **Built in the provided container** ([build-docker.md](build-docker.md)) with the full flag set ([build-flags-reference.md](build-flags-reference.md)), including `-fno-finite-math-only`. Building on a random newer distro can pull in glibc symbols production units don't have — the effect then silently never loads.
2. **Exports are correct.** Verify from the repo root (or inside the build container):

   ```bash
   nm -D bins/<YOUR-EFFECT>.so | grep -E ' (create|dsp_version)$'
   ```

   `<YOUR-EFFECT>.so` — your compiled effect binary in `bins/`.

   Expected output (addresses will differ):

   ```text
   000108f4 T create
   00021048 D dsp_version
   ```

   `create` must be `T` (a function) and `dsp_version` must be `D` (a **data** symbol — if you accidentally wrote it as a function, it shows `T` and the firmware reads garbage). Hand-written C++ must **not** show a `getExtensions` symbol here; that's the FAUST-pipeline marker and exporting it makes the firmware skip your `instanceConstants()`.
3. **Loads and runs** in the container harness (`benchmark-plugin`, per [verification.md](verification.md)).
4. **CPU budget met on real hardware.** Measured via `/proc/xenomai/sched/stat` on the device, your effect should stay under roughly **15% of the core**. The whole chain shares one 2.9 ms callback on both Stratus and Nimbus.
5. **Robustness drills all green** ([verification.md](verification.md)): arbitrary block sizes (1-1024, not just 128), out-of-range/NaN knob values, bypass behavior, repeated `instanceConstants()` resets on a dirty instance, zero underruns over a soak run.
6. **No RT violations** in `compute()` — no allocation, locks, I/O, or syscalls ([rt-safety.md](rt-safety.md)).

**Checkpoint:** you have a single `.so` that passes all six items above, plus the metadata and artwork from the previous sections. That is a complete submission package.

> **Warning:** If you upload a prebuilt binary, the platform only checks that it is a 32-bit ARM shared object under 5 MB — it does **not** validate your exports, ABI, or CPU cost, and prebuilt uploads receive a default CPU estimate rather than a measurement. A broken binary sails through upload and fails on users' pedals. The checklist above is your only safety net.

## Submitting

The submission happens in the **FX Builder** ([build.chaosaudio.com](https://build.chaosaudio.com); free Chaos Audio account required — the same account as the mobile app):

1. **Final smoke test via private publish.** Upload your final `.so` (a 32-bit ARM shared object under 5 MB) to your effect, make sure its UI in the UI Builder is complete, and **publish privately**. Private publish is instant (no review) and the effect appears only in your own account's library in the Chaos Audio app — install it on your Stratus/Nimbus and play it one last time. Submit the exact build you just played, not an earlier one.
2. **Submit for review.** In the FX Builder, submit the effect for public review.
3. **Accept the Developer Distribution Agreement.** On your first public submission (and again whenever the agreement version changes) a click-through popup presents the agreement — scroll it, download a copy if you like, and accept. Submission is blocked until you do.
4. **Review by Chaos Audio** typically takes **5–7 business days** (not guaranteed). Approval is not an endorsement. Review staff may install and test your effect free of charge.
5. **Approved** effects appear in Tone Shop™ — usually immediately, within 24 hours — for **both** Stratus and Nimbus users. If denied, you get the reason by email; revise and resubmit.

Ship an update by submitting a new version through the FX Builder with an **increased** effect version — the Effect ID stays the same for the life of the effect.

For developer and marketplace questions, email development@chaosaudio.com. General support and device questions remain support@chaosaudio.com / the developer Discord.

## Money

- By default **you keep 75% of Net Revenue** on every sale. Net Revenue = the gross sale price, minus payment-processing fees (currently 2.9% + $0.30 per sale), minus any sales tax/VAT collected.
- Set up payouts in the FX Builder under **Profile Settings → Developer Payouts** — a Stripe onboarding flow that takes about 3 minutes. Your share transfers to your Stripe account per sale.
- Payouts land on a **14-day delay**, matching the customer 14-day refund guarantee.
- Sales made before you finish Stripe onboarding aren't lost: they accrue automatically and transfer the moment onboarding completes.
- Refunds and chargebacks revoke the buyer's copy of the effect and reverse the corresponding developer share.
- Chaos Audio is the merchant of record and handles sales tax/VAT.

## The legal documents

- **[Developer Distribution Agreement](https://build.chaosaudio.com/developer-agreement)** — the binding contract you accept before distributing publicly. Presented as a click-through popup on your first public submission, and again whenever the agreement version changes.
- **Developer Guidelines** — the house rules: technical, asset, and content requirements, plus worked payout examples. In the FX Builder at `/guidelines` (sign-in required).
- **[DMCA Policy](https://build.chaosaudio.com/dmca)** — how copyright complaints against store content are handled.

Content rules worth restating here: no third-party trademarks or brand names in your effect's name, description, or artwork; only publish captures/IRs you have the rights to; respect open-source licenses (no GPL/copyleft code in a paid effect unless the license permits); and your effect must fit the CPU budget shown in the FX Builder.

Next: [back to the README](../README.md)
