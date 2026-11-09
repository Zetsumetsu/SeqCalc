# SeqCalc VST v0.1.0 — build notes

JUCE VST3 port of Steve Zydek's SeqCalc Max for Live sequencer/calculator.
Design spec: `VST-PORT-DRAFT.md` (in this folder). Source device files:
`~/workspace/seqcalc/` (params.js, settheory.js, forte.js, scribble1.js,
seqcalc_beta-v2.amxd).

## Build

```
cd ~/workspace/seqcalc-vst/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSEQCALC_BUILD_SMOKE_TEST=ON
cmake --build . --parallel
```

- JUCE 8.0.6 via FetchContent (same pattern as Pitchulator).
- Linux VST3: `build/SeqCalcVST.vst3/`.
- Headless smoke test: `build/tools/smoke_test/SeqCalcSmokeTest` (runs
  automatically as a post-build step).
- If the build fails with `X11/extensions/Xrandr.h: No such file or
  directory`, the VM was replaced and /usr/include lost the X11 dev
  headers. Re-run `~/workspace/fm-grime/vst-plugin/tools/install_x11_headers.sh`
  and configure again.

## What was built

- `core/ForteTable.{h,cpp}` + `core/ForteTableData.h`: the full 206-row
  Forte table, generated from `~/workspace/seqcalc/forte.js`, with the
  six §3.1 fixes and Forte prime forms on all six formerly-Rahn rows.
  §3.3 validation runs as load-time checks and in the smoke test.
- `core/SetTheory.{h,cpp}`: PC-set parsing (order-preserving), PC set
  to MIDI, MIDI note names.
- `core/PatternEngine.{h,cpp}`: the Scribbletune pattern language
  (x, -, _, R, brackets, subdiv, repeat, shuffle, velocities 100/70).
- `core/ClipRenderer.{h,cpp}`: the §2.6 pipeline (steps 1-8).
- `src/PluginProcessor.{h,cpp}`: APVTS parameters, Calc/Send, transport
  slaving, MIDI-out scheduler, drag-export MIDI file, state save/load.
- `src/PluginEditor.{h,cpp}`: the device layout (action buttons, 12-button
  grid, text fields, scale menus, toggles, CalcS/SEND/STOP, note display,
  piano strip, drag handle).

## Open items resolved (draft §5 Q4)

- Subdiv list: 1m, 2m, 3m, 4m, 1n, 2n, 4n, 8n, 16n (Scribbletune's map).
  Default 4n.
- Repeat range: 1-8, default 1.
- Scale list: Major, Natural Minor, Dorian, Mixolydian, Pentatonic Major,
  Pentatonic Minor, Chromatic. Only Major was confirmed from the device
  screenshot; the rest are sensible Tonal-style names.
- Pattern presets: free text only for v0.1.0, no built-in presets.
- Default PC set: 0 3 7 (minor triad). The device's pack default is a
  placeholder, so a musical triad was chosen so the plugin plays
  something useful on first load.

## Deliberate deviations from the literal Max patch

- Forte lookup is one-shot: a successful CalcS fills the PC-set field
  with the prime form and clears the Forte field. In the patch the
  Forte field persists, so a later hand edit to the set would be
  overwritten by the next CalcS. The one-shot keeps the set the user's
  after it is filled. Flagged for the v0.1.0 test.
- Grid action 5 (Forte) is implemented: the patcher sets the action
  but params.js has no case 5, so grid presses did nothing in Forte
  mode. The VST appends the Forte chars (0-9, Z, -) to the Forte field.
- The drag export writes a MIDI file to the temp directory
  (`SeqCalc-pattern.mid`, deleted first, written with truncate) and
  hands it to the OS drag. The patch wrote a clip into the selected
  Live slot; the VST has no slot, so drag-to-track is the Send
  equivalent and live MIDI-out playback is the primary output.
- The plugin does not respond to incoming MIDI notes and does not pass
  them through to the output. The device has no MIDI-input behaviour
  either, but the patcher does pass Live notes through the device
  object; the VST consumes them so host notes never leak into the
  generated output.
- Forte lookup normalises its input (whitespace stripped, lowercase z
  to Z), so " 6-z12 " finds 6-Z12. The table itself is exact-match.
- Scheduler detail: a note-off only fires for a note that is actually
  sounding. An end time of exactly the pattern length maps to t=0 in
  the first block of a run, before its note has ever sounded; without
  the guard that spurious off fires. (Edge case: a note whose onset
  and end fall in the same 512-sample block, i.e. shorter than ~0.05
  beats, has its note-off delayed by one loop iteration.)

## Test checklist

- `docs/seqcalc-v0.1.0-test-checklist.md` (canonical working copy) and
  `docs/seqcalc-v0.1.0-test-checklist.pdf` (fillable AcroForm snapshot),
  generated from `docs/seqcalc-v0.1.0-checklist-spec.py` with
  `~/workspace/build-tools/make_checklist.py`.

## Source zip

- `SeqCalc-v0.1.0-source.zip` (source only, no build/ artifacts).
