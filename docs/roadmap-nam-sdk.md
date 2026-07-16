# Roadmap: Neural Amp Modeler SDK

> **Status: planned — not yet available.** Nothing on this page can be used
> today; it exists so you know what's coming and can plan around it. Dates
> intentionally unpromised.

## The idea

[Neural Amp Modeler](https://www.neuralampmodeler.com/) (NAM) is the
open-source standard for neural captures of amps and pedals, and it already
powers first-party effects on Stratus and Nimbus. We plan to make NAM
available **inside your own plugins**: a small wrapper you include from
`common/`, a vendored copy of the open-source NAM inference engine under
`modules/`, and an example effect showing the pattern —

```cpp
// The planned shape (subject to change):
ChaosNAM nam;
nam.loadFromMemory(my_capture_json, my_capture_len);  // in instanceConstants()
...
nam.process(inputs, outputs, count);                  // in compute()
```

Load a `.nam` capture, wrap your own drive/tone/level controls around it,
ship one `.so` — same build flow, same contract, same deploy as every other
effect in these docs.

## What to expect (honest constraints)

- **CPU is the boundary.** Neural inference is by far the most expensive
  thing an effect can do on this hardware: our heavily-optimized first-party
  NAM runs a Nano-class model at roughly 40% of the entire core. The SDK
  will target **small (Feather/Nano-class) models only**, and NAM-based
  effects will be held to the [verification](verification.md) CPU budget
  like everything else — expect a NAM effect to dominate whatever preset
  it's in.
- **Models will be compiled in** for v1 (a provided script turns a `.nam`
  file into a C array). User-swappable model files may come later with
  store-side plumbing.
- **44.1 kHz native.** Captures trained at 44.1 kHz will be most faithful;
  48 kHz-trained captures will run but shift slightly darker.

## What you can do today

- Ship a capture as its own effect: `.nam` captures can already be uploaded
  and published through the [FX Builder](https://build.chaosaudio.com)
  today — what this page adds is using captures *inside your own plugin's*
  DSP.
- Build neural effects with **[RTNeural](https://github.com/jatinchowdhury18/RTNeural)**,
  already vendored in `modules/` and used by the AIDA-X effects in
  `featured/` — see the [repo tour](runtime-examples-map.md).
- Keep captures you'd want to ship in `.nam` format at small sizes —
  they'll port straight into the SDK when it lands.
- Tell us in the developer Discord if this matters to you — demand directly
  affects priority and whether a richer on-device NAM API follows.

Next: [back to the README](../README.md)
