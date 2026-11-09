# SeqCalc VST Port — Design Draft

**Status: DRAFT ONLY — for Nick's review. No code, no JUCE scaffolding yet. No build starts until Nick approves this draft.**

- **Source device:** Steve Zydek's SeqCalc, a set-theory Max for Live sequencer/calculator (beta v2, Live 10 era).
- **Source repo:** `Zetsumetsu/SeqCalc` (Nick's fork of `szydek/SeqCalc`), cloned locally at `~/workspace/seqcalc/`.
- **Blessing:** Steve Zydek gave his full blessing on 2026-10-20 to use SeqCalc's ideas and Forte tables. He has not touched the software in 4–5 years and wants to pick it back up. Hardware applications were discussed and are **parked** — the VST port is step one.
- **Design rule for this port:** stay faithful to Steve's original workflow first, then develop from there.
- **How this draft was built:** by reading the actual files — `README.md`, `params.js`, `settheory.js`, `forte.js`, `scribble1.js` (webpack bundle of Scribbletune + a small `src/index.js` entry), and the full patcher JSON extracted from `seqcalc_beta-v2.amxd` (Max 8.1.8, 219 boxes, 247 patchlines, all traced). Anything marked **[inferred]** is a best reading of the patch, not a verified fact.

---

## 1. How SeqCalc works now

### 1.1 The pieces

| File / object | What it does |
|---|---|
| `seqcalc_beta-v2.amxd` | The Max for Live device. Presentation UI plus the patching that wires the four JS files together and talks to Live. |
| `params.js` (`js params.js`, obj-142) | Routes the 12-button PC grid. Takes `[code, action]` and outputs `[action, value]`. Actions: 0 = root note, 1 = base octave (capped at 10), 2 = octave range (capped at 8), 3 = PC-set integer (0–9, a = 10, b = 11), 4 = rhythm-pattern char (`x R - _ [ ]`), 5 = Forte digit (0–9, Z, `-`) — **marked "Not yet implemented" in the code**. |
| `settheory.js` (`js settheory.js`, obj-19) | Translates a PC set into note names. Input list `[root, baseOctave, numOctaves, pcSetString]`. Output: note names joined with `_`, e.g. `C4_D4_F4_...`. |
| `forte.js` (`js forte.js`, obj-171) | A JSON string of 206 rows `{fn, pf, iv}` — Forte number, prime form, interval vector. `anything()` matches the incoming message string against `fn` exactly and outputs the prime form. No input validation. |
| `scribble1.js` (`js scribble1.js`, obj-3) | Webpack bundle: the Scribbletune library plus a small entry (`src/index.js`) that parses one comma-separated string, builds a Scribbletune clip, and writes it into Live via the Live API. |
| `live.thisdevice` (obj-5) | Feeds `js scribble1.js` (device context). |
| `p NoteComputer` (obj-120) | Subpatcher: maps each of the 12 PC-grid buttons to its index 0–11 (messages `'0'`…`'11'`). |
| `p noteNames_to_numbers` (obj-137) | Subpatcher: converts note names to MIDI numbers for the kslider display. |

### 1.2 The heart of it: the 10-field clip string

Everything in the device funnels into **one comma-separated string** that `src/index.js` parses. The Max `combine` object (obj-11, `combine "C 4 major" \, xxxx \, 0 \, 4n \, 1 \, 0 \, 0 \, 0 \, 0 \, "c 4" @triggers -1`) assembles it from ten sources. Field order is fixed and verified against `src/index.js`:

| # | Field | Max source | Meaning in `src/index.js` |
|---|---|---|---|
| 1 | Scale | `combine C 3 " " major` (obj-152) ← `menuRoots` / `menuOctaves` / `menuScales` | e.g. `C3 major`. Root = first 2 chars; a chromatic scale is built from the root too. Default `C3 major`. |
| 2 | Pattern | Textedit obj-9 → `route text` → `regexp \s` (strip spaces) | Scribbletune pattern string, e.g. `xxxx`. Default `x-xRx_RR` in the JS. |
| 3 | Shuffle | Toggle obj-50 (labelled SHUFFLE) | `shuffle = !!parseInt(data[2])`. Shuffles the note list before mapping. |
| 4 | Subdiv | `menuSubdiv` (obj-36) | Overall subdivision, e.g. `4n`. Step duration (see §1.4). |
| 5 | Random-in-scale | Toggle obj-46 (labelled RANDOM IN-SCALE?) | `useScaleNotesForR`: when on, `R` picks from the scale/set notes; when off, `R` picks from the chromatic scale **minus** the scale notes (i.e. out-of-scale notes). |
| 6 | Repeat | `menuRepeatPattern` (obj-15) | `repeatPattern`: the pattern string is repeated this many times. |
| 7 | Root-only | Toggle obj-48 (labelled ROOT ONLY) | `useRootOnly`: reduces the note set to the root note alone. |
| 8 | Rests-for-R | Toggle obj-49 (labelled RAND RESTS) | `useRestsForR`: when on, `R` gets no random-note set (`randomNotes = null`). |
| 9 | Use-PC-set | Toggle obj-1 (labelled USE PC SET) | `useCustomScale`: when on, the PC set (field 10) overrides the scale (field 1) as the note source. |
| 10 | Custom set | `js settheory.js` (obj-19) | Note names from the PC set, e.g. `C4 D4 F4`. Default placeholder in the patch is `c 4`; the live value always comes from `settheory.js`. |

The stored default message (obj-2) shows the format in the wild:

```
scribble1 "C2 major,xxxx,0,4n,1,4,0,0,1,c 4"
```

i.e. scale `C2 major`, pattern `xxxx`, shuffle off, subdiv `4n`, random-in-scale on, repeat 4, root-only off, rests-for-R off, use-PC-set on, custom set `c 4`.

**How the note list is chosen** (`src/index.js`, verbatim logic):

```
scaleNotes = useCustomScale ? customSet : scribble.scale(scale)
notes      = useRootOnly ? root : scaleNotes
crNotes    = scribble.scale(root + " chromatic")
rNotes     = useScaleNotesForR ? scaleNotes : (crNotes minus scaleNotes)
clip = scribble.clip({ notes, pattern: pattern repeated repeatPattern times,
                       randomNotes: useRestsForR ? null : rNotes,
                       shuffle, subdiv })
```

### 1.3 Step-by-step workflow (as built in the patch)

**Setup (once per session):**
1. Load the device on a dedicated track in Live (README: use a dedicated track — some buttons have hardcoded MIDI notes for Launchpad feedback).
2. Select a clip in Session View first. The device UI says so in plain text: *"First select a clip in Ableton Live Session View"* (obj-8). The clip is the destination — the device writes **into the selected clip** (see §1.3, Send). It does not create a new clip; it replaces the selected clip's contents.

**Choosing the notes — two modes:**
3. **Scale mode** (USE PC SET off): pick root, octave and scale from `menuRoots` / `menuOctaves` / `menuScales`. Notes come from `scribble.scale(scale)`.
4. **PC-set mode** (USE PC SET on): the notes come from a pitch-class set, entered one of three ways:
   - **The 12-button grid** (live.button obj-95…133, two staggered rows of six, labelled `0x 2- 4[ 5] 7 9 b-` / `1R 3_ 6 8 aZ`). First pick *which parameter* the grid edits with the six column buttons Root / Roct / Range / PC / Rhy / Forte (live.button obj-163/167/204/209/238/113 → messages `0`…`5` → number obj-61 → `pack 0 0` → `js params.js` action). Then grid buttons send PC integers (0–9, a, b) via `p NoteComputer`.
   - **The typable PC-set field** (textedit obj-214, labelled TYPABLE PITCH CLASS SET): type e.g. `0 2 5`.
   - **A Forte number** (textedit obj-138, labelled FORTE): type e.g. `6-31`, then press **CalcS**. CalcS re-bangs the Forte field → spaces stripped (`regexp \s @substitute %0`) → `js forte.js` looks up the number and returns its prime form (e.g. `0,1,3,5,8,9`) → commas turned to spaces → appended into the typable PC-set field (`s setinp2` → `r setinp2` → obj-214). **[Inferred from the patch trace:]** CalcS also bangs the PC-set field, which flows through `prepend =` (spaces stripped, so `0 2 5` becomes `=025`) into `pack c 4 1 =0001` (defaults: root `c`, base octave `4`, range `1`) → `s set1` → `r set1` → `js settheory.js`, which computes the note names.

**Choosing the rhythm:**
5. Type or build a Scribbletune pattern in the typable pattern field (textedit obj-9, labelled TYPABLE PATTERN LANGUAGE), and/or set Subdiv (`menuSubdiv`) and Repeat (`menuRepeatPattern`). The rhythm-pattern characters can also be entered through the grid: `params.js` action 4 maps grid positions to `x R - _ [ ]`.
6. Set the four behaviour toggles: SHUFFLE, RANDOM IN-SCALE?, ROOT ONLY, RAND RESTS.

**Calculate and send:**
7. **CalcS** (live.button obj-217, labelled `calcS`): computes the note names from the current PC set / Forte number and refreshes the `combine` string. It also fires the Forte lookup (§1.3 step 4).
8. **Send** (live.button obj-104, labelled `SEND`): re-reads the pattern field, rebuilds the `combine` string, then bangs the stored `scribble1 "…"` message (`s scrb1` → `r scrb1` → obj-2) into `js scribble1.js`, which builds the clip and writes it into the selected Live clip via the Live API (`live_set view detail_clip` if a clip is open in Detail View, otherwise `live_set view highlighted_clip_slot clip`). The vendored `scribble.max` sets the clip's `loop_end` to the total length, calls `remove_notes`, then writes each note (pitch, start, duration, velocity 100 — or 70 for fallback `R` notes, see §1.4).
9. **clrS** (live.button obj-232) clears the Forte + PC-set fields and flushes the kslider display. **RESET** (live.button obj-53) restores the default clip string and broadcasts `reset1`.

**Feedback while working:**
- The computed note names are shown as text (message obj-89, e.g. `C4 D4 F4`) and on an 88-key piano strip (kslider obj-190, `range 88`, fed by `prepend chord` from `settheory.js` via `p noteNames_to_numbers`).
- All `live.button` / `live.toggle` / `live.menu` controls are MIDI-mappable in Live (README). Some buttons also emit hardcoded MIDI notes for Launchpad feedback (`s ind1` → `route 0 1 2 3 4 5 6 7` → messages `36 37 38 39 68 69 70 71` → `makenote 40 500 1` → `noteout 1 2`; the UI comment reads *"Edit patch to change default MIDI controller feedback notes"*).

### 1.4 The pattern language (verified from the vendored `clip()`)

Scribbletune's `clip()` validates the pattern against `/[^x\-_\[\]R]/` — only these characters are legal. Semantics, read directly from the vendored code:

| Char | Meaning | Velocity |
|---|---|---|
| `x` | Play the next note from the note list, in order. The note index **wraps to 0** at the end of the list, so notes cycle. | 100 (`amp`) |
| `R` | Random note. If a random-note set exists, pick one from it (always plays). If not, a coin flip: 50% play the next note from the main list, 50% silent. | 100 from the R-set; 70 (`accentLow`) from the main list |
| `-` | Silent step — time advances, no note (the device's own SYNTAX panel labels this "Note off"). | — |
| `_` | Sustain — extends the previous event's length by one step. No new event. | — |
| `[ … ]` | Subdivide — the bracketed group splits one step's length evenly among its characters. | as per chars inside |

- **Shuffle** (`shuffle: true`) shuffles the note list before mapping.
- **Subdiv** sets the step duration, from Scribbletune's map: `1m` = 4 beats, `2m` = 8, `3m` = 12, `4m` = 16, `1n` = 4 beats, `2n` = 2, `4n` = 1, `8n` = 0.5, `16n` = 0.25. (Internally 128 ticks per quarter note.)
- **Repeat** repeats the whole pattern string N times before rendering.

### 1.5 What the device does NOT do

Verified from the code (no such logic exists in `params.js`, `settheory.js`, `forte.js`, or `src/index.js`):

- No transposition, inversion, retrograde, retrograde-inversion, complement, prime-form computation, or set-relationship tests. `forte.js` is a pure lookup table; `settheory.js` only translates a given set to note names.
- No preset system (README lists "Presets?" as a TO-DO).
- No scale-degree or chord-symbol input beyond the scale menus and the PC set.
- `params.js` action 5 (Forte digit entry from the grid) is stubbed as "Not yet implemented" — Forte entry works only through the text field + CalcS.

### 1.6 Known Max-side quirks — do NOT reproduce these

- **Calculation race condition.** README TO-DO: *"Fix calculation race condition and requirement for using spaces when entering Forte and Scribbletune text."* CalcS and Send are separate buttons with separate bang paths, and the patch even contains a 200 ms re-trigger loop on the PC-set textedit (obj-214 outlet 3 → `delay 200` → back into obj-214). The VST must make calculate-then-send a single atomic step.
- **Spaces requirement.** The patch strips all whitespace from Forte and pattern input (`regexp \s @substitute %0`) before lookup. Whatever the exact user-facing behaviour was, the VST must accept input with or without spaces and behave identically.
- **Launchpad feedback.** Hardcoded MIDI notes (`36–39`, `68–71`) out of `noteout 1 2`. This is Live/Max-specific hardware feedback and has no place in the VST.
- **The `=` prefix.** The PC-set string is prefixed with `=` by `prepend =` before reaching `settheory.js`, which silently ignores the `=` (its char filter is `/[0-9a-b]/`). An internal Max-side marker only — the VST needs no equivalent.

---

## 2. VST port design

### 2.1 What changes, what stays

| Aspect | Max for Live | VST |
|---|---|---|
| Host | Ableton Live only (Live API, Session View clips) | Any VST3 host (Ableton, etc.) |
| Output | Writes notes into the **selected Live clip** (overwrites it) | **MIDI out** — see §2.3 |
| Note calculation | `params.js` + `settheory.js` + `forte.js` + `scribble1.js` | Same logic, re-implemented in C++ (see §4) |
| Pattern engine | Vendored Scribbletune `clip()` (JS) | Same semantics, re-implemented in C++ from the documented behaviour (§1.4) — do **not** embed the webpack bundle |
| UI | Max presentation layout | JUCE editor mirroring the same sections (§2.4) |
| MIDI-mappable buttons | Live's MIDI-map | Automatable parameters (§4.3); MIDI learn deferred, see §5 |
| Launchpad feedback | Hardcoded `noteout` | Out of scope |

### 2.2 Inputs (what the plugin takes in)

Same as the device, no additions for v0.1:

- **Pitch-class set**, by any of: the 12-button grid, the typable field, or a Forte number (with CalcS-equivalent lookup).
- **Root** (C…B), **base octave** (0–10, same cap as `params.js`), **octave range** (1–8, same cap).
- **Scale** (root + octave + scale name) for scale mode, when USE PC SET is off.
- **Pattern** (Scribbletune pattern text), **Subdiv**, **Repeat**.
- **Behaviour toggles:** Shuffle, Random-in-scale, Root-only, Rests-for-R, Use-PC-set.

### 2.3 Outputs (what the plugin produces)

A VST cannot write into a host clip, so Send has to become MIDI. Recommended v0.1 design — **both**, because together they cover the original's two uses (a static clip you place, and a pattern you hear):

1. **Live MIDI-out playback (primary).** The plugin is a MIDI generator: it renders the current pattern to a timed note list and plays it out of its MIDI output, synced to the host transport. The user routes the plugin's MIDI to an instrument track (the same routing as Temperament/Pitchulator). This is the "hear it now" path.
2. **Drag-and-drop MIDI export (the Send equivalent).** A drag handle exports the rendered pattern as a standard MIDI file/clip the user drops onto a track — the closest functional match to Send writing a static clip. (Same drag-to-DAW mechanism already used in Temperament.)

If scope has to shrink, keep (1) and defer (2) — but (2) is the faithful Send, so it should not be dropped silently; it is a decision for Nick (§5).

### 2.4 UI — mirror of the original device

Layout follows the `.amxd` presentation view ("SEQUENCE CALCULATOR - beta 2.0"), cleaned of Max-only clutter. Sections, in the device's own order:

1. **PC grid (left).** 12 buttons (0–9, a, b) in two staggered rows of six, exactly as labelled in the device (`0x 2- 4[ 5] 7 9 b-` / `1R 3_ 6 8 aZ`). Above/below them, the six action-select buttons: Root / Roct / Range / PC / Rhy / Forte — these choose which parameter the grid edits (`params.js` actions 0–5). One active at a time **[inferred: radio behaviour from the `sel 1` → `t 1 0` chains]**.
2. **Scale mode (top right).** Root menu, Octave menu, Scale menu — shown when USE PC SET is off. Labelled in the device *"SCALE MODE SETTING - WHEN PC SET IS DEACTIVATED"*.
3. **Typable fields (right).** PC-set field, pattern field, Forte field — plain text entry, no spaces requirement (§1.6).
4. **Behaviour toggles (far left).** RANDOM IN-SCALE?, ROOT ONLY, RAND RESTS, SHUFFLE, plus Subdiv and Repeat menus and the USE PC SET toggle.
5. **CalcS / Send (centre, "CLIP ACTIONS").** CalcS computes and refreshes the note display; Send emits the result (§2.5). Plus clrS (clear set) and RESET (restore defaults: `C2 major,xxxx,0,4n,1,4,0,0,1,c 4`).
6. **Note display (bottom right).** The computed note names as text (e.g. `C4 D4 F4`) and a piano-strip showing the current set's notes — the VST equivalent of the device's kslider (88-key display, `prepend chord`).

### 2.5 CalcS / Send in the VST (atomic, no race)

- **Calc** (CalcS): runs the full pipeline (§2.6 steps 1–6) and updates the note display and the rendered pattern. No output yet.
- **Send**: takes the already-calculated pattern and emits it — starts live MIDI-out playback from step 1 (and/or arms the drag-export, §2.3). Recalculating and emitting are separate user actions, as in the original, but a single Send press always operates on a fully calculated pattern: the VST must never emit a half-calculated state. The Max race condition is designed out, not reproduced.
- Changing any input after a Calc marks the display stale until Calc is pressed again **[decision for Nick: or auto-recalculate on every change — see §5]**.

### 2.6 Behaviour spec — the calculation pipeline

Ported from `src/index.js`, `settheory.js`, `forte.js`, and the vendored `clip()`:

```
1. pcSet = forteNumber ? forteTable[forteNumber].primeForm : typedOrGridPcSet
2. noteNames = setTheory(root, baseOctave, numOctaves, pcSet)
     for o in 0 .. numOctaves-1:
       for each char c in pcSet (chars 0-9, a=10, b=11; anything else ignored):
         pitch = (int(c) + rootOffset) mod 12
         emit scale[pitch] + (baseOctave + o)        // e.g. "C4", "D4", "F4"
3. scaleNotes = usePcSet ? noteNames : scaleNotes(scale)   // scale = "C 3 major" etc.
4. notes = rootOnly ? [root] : scaleNotes
5. crNotes = chromatic scale from root
6. rNotes = useScaleNotesForR ? scaleNotes : (crNotes minus scaleNotes)
7. pattern = typedPattern repeated repeatPattern times
8. events = renderPattern(notes, pattern, rNotes, shuffle, subdiv)
     shuffle: shuffle notes first
     x  -> next note in order (index wraps to 0), velocity 100
     R  -> if rNotes exists: random note from rNotes, velocity 100
           else: 50% next note from notes at velocity 70, 50% silent
     -  -> silent step (time advances, no note)
     _  -> extend previous event by one step
     [..] -> split one step evenly among the bracketed chars
     step duration from subdiv (4n = 1 beat, 8n = 0.5, 16n = 0.25,
     2n = 2, 1n = 4, 1m = 4 = one bar, etc.)
9. output events as MIDI (live playback and/or drag-export file)
```

Notes on fidelity:
- `settheory.js` does **not** sort or dedupe: the PC set is emitted in the order typed, repeated per octave. The VST must do the same (a typed `0 2 5` and a typed `5 2 0` give different note orders).
- The Forte lookup is exact-match on the Forte number string (`6-31`, `6-Z12`, …). No fuzzy matching in v0.1.
- `params.js` caps: base octave ≤ 10, range ≤ 8. Keep both caps.

---

## 3. The Forte table (corrected)

### 3.1 The fixes — six, not four

The October 2026 audit found 4 glitches by checking interval-vector length (must be 6) and digit sum (must equal the pair count for the set size). A **full recomputation** for this draft — recomputing every interval vector from its prime form and cross-checking Z-mates — confirms the four, **corrects one of them**, and finds **two more** the length/sum checks could not catch (digit-permutation errors pass both checks).

| # | Row | As transcribed in `forte.js` | Corrected value for the VST | Evidence |
|---|---|---|---|---|
| 1 | 5-13 | iv `2221311` (7 digits) | iv `221311` | Recomputed from pf `0,1,2,4,8` ✓ |
| 2 | 6-2 | iv `4443211` (7 digits) | iv `443211` | Recomputed from pf `0,1,2,3,4,6` ✓ |
| 3 | 6-31 | Row scrambled: fn holds a stray char + `6`, pf holds `223431`, iv holds `6-31` | **fn `6-31`, pf `0,1,3,5,8,9`, iv `223431`** | See note below — the prime form differs from the October audit |
| 4 | 6-35 | pf `0,2,4,6,8T` (missing comma) | pf `0,2,4,6,8,10` (T = 10); iv `060603` already correct | Recomputed ✓ |
| 5 | **6-Z12** *(corrected 2026-11-08, see note)* | pf `0,1,2,4,5,7` (duplicates 6-Z11's pf), iv `332232` | pf **`0,1,2,4,6,7`**; iv stays `332232` | Published sources (Forte's table; thesis table listing 6-z12 as (012467)) + Z-mate 6-Z41 (pf `0,1,2,3,6,8`, iv `332232`, recomputes correctly) — the two rows must share an iv, so the file's iv was right and the pf was the error |

**⚠ Correction to the draft's own fix #5 (found 2026-11-08 during implementation, by the §3.3 rule 7 check — no duplicate prime forms).** The draft as first written said 6-Z12's iv was wrong (`332232` → `333231`, recomputed from the file's pf). That was backwards: the file's pf `0,1,2,4,5,7` is a copy of 6-Z11's pf, so recomputing from it gave 6-Z11's iv, not 6-Z12's. The file's iv `332232` is correct (6-Z12's Z-mate 6-Z41 has iv `332232`, and published sources list 6-Z12 as (0,1,2,4,6,7) with iv <3,3,2,2,3,2>). The fix is to the prime form only: `0,1,2,4,6,7`, iv unchanged at `332232`. **Do not ship the draft's original `333231`.** The VST table and the smoke test use the corrected value.
| 6 | **6-Z43** *(new)* | iv `233331` | iv `322332` | Recomputed from pf `0,1,2,5,6,8`; confirmed by Z-mate 6-Z17, whose file iv `322332` is correct for its own pf |

**⚠ Correction to the October audit — 6-31's prime form.** The audit's reconstruction was `pf 0,1,3,5,6,9, iv 223431`. That pair is internally inconsistent: `0,1,3,5,6,9` recomputes to iv `224322`, not `223431`. The correct Forte prime form for 6-31 is **`0,1,3,5,8,9`**, which recomputes to `223431` exactly. Verified against two published sources: the *Forte number* article ("the Forte prime form for 6-31 is [0,1,3,5,8,9]") and three independent scale pages listing 6-31's interval vector as `<2,2,3,4,3,1>`. **Do not ship the audit's `0,1,3,5,6,9`.**

### 3.2 The Forte-vs-Rahn decision (five rows, needs Nick/Steve)

Forte's prime-form algorithm and Rahn's (1980) algorithm disagree on six set classes: **5-20, 6-Z29, 6-31, 7-Z18, 7-20, 8-26**. The interval vector is the same either way (it is a property of the set class), but the prime form — i.e. the actual notes you get for a Forte number — differs. The transcribed table uses **Rahn** prime forms for the five rows that survive intact (plus the scrambled 6-31):

| Row | In `forte.js` (Rahn) | Forte's prime form |
|---|---|---|
| 5-20 | `0,1,5,6,8` | `0,1,3,7,8` |
| 6-Z29 | `0,2,3,6,7,9` | `0,1,3,6,8,9` |
| 6-31 | *(scrambled; fixing to Forte's `0,1,3,5,8,9`)* | `0,1,3,5,8,9` ✓ |
| 7-Z18 | `0,1,4,5,6,7,9` | `0,1,2,3,5,8,9` |
| 7-20 | `0,1,2,5,6,7,9` | `0,1,2,4,7,8,9` |
| 8-26 | `0,1,3,4,5,7,8,10` | `0,1,2,4,5,7,9,10` |

Forte's primes above were computed with an implementation of Forte's algorithm that reproduces the published anchors (6-31 → `0,1,3,5,8,9`; 5-20 → `0,1,3,7,8`) and agrees with the file on the other 199 rows — see verification note below. **Correction 2026-11-07 (verification pass):** the draft first listed five divergent rows and missed **7-Z18** — its file value `0,1,4,5,6,7,9` is Rahn's; Forte's is `0,1,2,3,5,8,9`. So the decision covers **six** Forte numbers, not five.

**Verification note (2026-11-07, corrected 2026-11-08):** both algorithms were implemented from scratch and run over all 206 rows of the cloned `forte.js`. Result: 199 rows agree with both algorithms; exactly the six rows above match Rahn only; the only row matching neither is the scrambled 6-31. The same pass re-verified all six §3.1 fixes by recomputation from the prime forms, and the Z-mate cross-check (6-Z43's fix to `322332` pairs it with 6-Z17; 6-Z12's corrected pf `0,1,2,4,6,7` keeps iv `332232`, pairing it with 6-Z41).

**Decision made 2026-11-07 (Nick, with Steve): Forte's prime forms throughout.** The table uses Forte's primes for all six rows above — 5-20 `0,1,3,7,8`, 6-Z29 `0,1,3,6,8,9`, 6-31 `0,1,3,5,8,9`, 7-Z18 `0,1,2,3,5,8,9`, 7-20 `0,1,2,4,7,8,9`, 8-26 `0,1,2,4,5,7,9,10` — alongside the six §3.1 fixes. The table is consistent: Forte's algorithm, no mixing.

### 3.3 Validation rules (load-time + tests)

The VST's table must be validated, not trusted. Rules, applied to all 206 rows at load and in unit tests:

1. **Row count = 206** (cardinalities 3–9; Forte numbers 3-1…9-12, including Z-sets).
2. **Interval vector length = 6**, every digit 0–9.
3. **Digit sum = pair count** for the set size: n(n−1)/2 (3→3, 4→6, 5→10, 6→15, 7→21, 8→28, 9→36).
4. **Recomputed iv from pf must equal the stored iv** — this is the check the October audit lacked, and it is what catches digit-permutation glitches like 6-Z12 / 6-Z43.
5. **Prime form validity:** length = n, values 0–11, ascending, starting at 0, no duplicates.
6. **Z-mates share an iv:** for every Z-pair, both rows' stored ivs must be identical (this independently catches 6-Z12 / 6-Z43-type errors).
7. **Forte numbers unique; no duplicate prime forms** across rows (each set class appears once).

### 3.4 Sourcing and attribution

- The table data is Allen Forte's published table (*The Structure of Atonal Music*, 1973), as transcribed by Steve Zydek into `forte.js`. The VST re-derives it with the §3.1 fixes — it does not copy the glitched rows.
- Credit in the plugin docs/about: "Forte table after Allen Forte (1973), via Steve Zydek's SeqCalc. Set-theory concept and workflow by Steve Zydek, used with his blessing."
- Scribbletune/Tonal are credited in SeqCalc's README for the concept; the VST re-implements the clip behaviour from its documented semantics (§1.4) and does not embed the `scribble1.js` bundle.

---

## 4. JUCE implementation notes

### 4.1 Plugin type and MIDI architecture

- **VST3, JUCE.** MIDI-generating plugin (instrument-style MIDI output, no audio input required; audio output silent / pass-through as appropriate). Same territory as Temperament/Pitchulator — Nick's existing MIDI-generator pattern applies, including the transport-slaving rule (§4.2).
- **No Live API, no Max, no JS runtime.** The four JS files become C++ modules:
  - `ForteTable` — the corrected 206-row table (§3) as a compile-time data table + lookup by Forte-number string.
  - `SetTheory` — the `settheory.js` translation (root offset, per-octave repetition, order-preserving).
  - `PatternEngine` — the `clip()` semantics (§1.4): pattern validation, note cycling, R-random, sustain/extend, bracket subdivide, shuffle, subdiv durations.
  - `ClipRenderer` — renders a pattern + note list to a timed event list (start in beats, duration in beats, note, velocity).
  - `MidiScheduler` — plays the rendered events out of the MIDI output buffer against the host playhead (ppq position, tempo, time signature), and/or writes them to a drag-export MIDI file.
- **State:** all inputs are plugin state (saved/restored with the session). The rendered pattern is derived state — recompute on Calc, never persist a stale render.

### 4.2 Transport behaviour (standing rule)

Nick's transport-slaving rule applies (all VSTs with a sequencer/time-sync feature): DAW transport start starts the plugin at **step 1** of the pattern; DAW transport stop stops it; the plugin keeps its own Stop, latched until its own Play is pressed again; a DAW transport stop clears the latch. Pattern position is derived from the host ppq position so playback stays sample-accurate to the grid.

### 4.3 Parameters (automatable)

| Parameter | Type | Notes |
|---|---|---|
| Root | Choice, 12 (C…B) | = `params.js` action 0 |
| Base octave | Int, 0–10 | Cap from `params.js` action 1 |
| Octave range | Int, 1–8 | Cap from `params.js` action 2 |
| PC set, 12 toggles | 12 × Bool (PC 0–11) | The automatable form of the grid; synced both ways with the typable field |
| PC-set text | Editor-only string (not automatable) | Typing `025` / `0 2 5` sets the 12 toggles; spaces ignored |
| Forte number | Int index 0–205 + editor search list | The editor shows a searchable list of all 206 Forte numbers with prime form + interval vector; the automatable parameter is the row index |
| Scale (root/octave/name) | 3 × Choice | Scale mode only; scale names from the same list the original menus used (§5) |
| Pattern text | Editor-only string (not automatable) | JUCE has no automatable string parameter; saved in plugin state. Plus a small preset choice (common patterns) if Nick wants it |
| Subdiv | Choice (1m/2m/3m/4m/1n/2n/4n/8n/16n) | From Scribbletune's map; exact menu contents to confirm (§5) |
| Repeat | Int (1–8?) | Exact range to confirm (§5) |
| Shuffle / Random-in-scale / Root-only / Rests-for-R / Use-PC-set | 5 × Bool | Direct ports of the five toggles |
| Calc | Trigger (button) | Runs the pipeline, updates the display |
| Send / Play | Trigger (button) | Emits the calculated pattern (§2.5) |

### 4.4 What stays out of scope for v0.1

- **The transforms the original doesn't do:** transposition, inversion, retrograde, retrograde-inversion, complement, prime-form computation, set-relationship tests. These are future development, not part of a faithful port.
- **Launchpad/controller feedback** (the `noteout` path) and **MIDI learn**. Automatable parameters are the VST equivalent of the original's MIDI-mappable buttons; MIDI learn can be added later if wanted.
- **Presets** (README TO-DO, unimplemented in the original).
- **Hardware applications** — explicitly parked; the VST is step one.
- **Scale-mode scale list beyond the original menus** — port the original's list, don't expand it in v0.1.

---

## 5. Open questions (only Steve or Nick can answer)

1. **Forte vs Rahn prime forms (§3.2)? — DECIDED 2026-11-07: Forte's throughout** (Nick, with Steve). The six rows use Forte's primes; see §3.2.
2. **Output mode (§2.3)?** Live MIDI-out playback, drag-and-drop MIDI export, or both for v0.1?
3. **Calc behaviour (§2.5)?** Keep the two-step Calc → Send, or auto-recalculate on every input change (with Send/Play as the only action)?
4. **Menu contents?** The `.amxd` does not store the `live.menu` item lists readably. What exactly is in `menuScales` (which scale names, in which order), `menuSubdiv` (which subdivisions), and `menuRepeatPattern` (which repeat counts)? Steve's memory or the device's own screenshots (`SeqCalc_beta-v2-loaded.png` in the repo) are the sources.
5. **The spaces requirement and the race condition?** Confirm the intended CalcS → Send ordering (is CalcS meant to be pressed before Send, or should Send trigger CalcS?). The VST design assumes Calc-then-Send, atomic.
6. **The six column toggles (Root/Roct/Range/PC/Rhy/Forte)?** Confirmed from the patch that they select which parameter the 12-button grid edits. Is the radio behaviour (one active at a time) correct, and should the grid remember a different PC set per column or share one?
7. **Should the four (now six) Forte fixes be contributed back to the SeqCalc repo?** Nick's fork — he pushes via GitHub Desktop. The fixes would also help Steve, who wants to pick SeqCalc back up.
8. **Name?** Keep "SeqCalc" for the VST, or a new name? (Steve's device, his blessing — his call, with Nick.)
9. **Pattern presets?** A few built-in patterns (e.g. `xxxx`, `x-xRx_RR`) as a starting point, or free text only?

---

## Appendix — where each claim comes from

| Claim | Source |
|---|---|
| Device purpose, Live 10 / Catalina, dedicated track, MIDI-mappable buttons, Launchpad feedback | `README.md` |
| CalcS + Send → new clip for the selected clip | `README.md` ("click the CalcS and Send buttons to generate a new clip"); `src/index.js` (`live_set view detail_clip` / `highlighted_clip_slot clip`) |
| `params.js` actions 0–5, caps, `rhyset`, action 5 "Not yet implemented" | `params.js` |
| `settheory.js` translation, order-preserving, `_`-joined output | `settheory.js` |
| `forte.js` 206 rows, exact-match lookup, no validation | `forte.js` (row count verified by regex: 206) |
| 10-field clip string, field meanings, defaults | `src/index.js` in `scribble1.js` + `combine` obj-11 + `scribble1 "C2 major,xxxx,0,4n,1,4,0,0,1,c 4"` (obj-2) |
| Pattern semantics (x/R/-/_, brackets, cycling, shuffle, subdiv map) | Vendored Scribbletune `clip()` in `scribble1.js` |
| CalcS / Send / clrS / RESET wiring | Patch traces from `obj-217` / `obj-104` / `obj-232` / `obj-53` in `seqcalc_beta-v2.amxd` |
| 12-button grid → `p NoteComputer` → `pack 0 0` → `params.js`; column buttons → obj-61 → action | Patch traces from obj-95…133, obj-120, obj-143, obj-163/167/204/209/238/113 |
| kslider display, `C4 D4 F4` message | obj-190, obj-89, obj-137, obj-240 |
| Race condition + spaces requirement | `README.md` TO-DO; 200 ms `delay` loop on obj-214 |
| Launchpad feedback notes | `s ind1` → messages `36 37 38 39 68 69 70 71` → `makenote 40 500 1` → `noteout 1 2`; UI comment obj-7 |
| 6-31 Forte prime `0,1,3,5,8,9`, iv `223431` | *Forte number* article; ianring.com scale pages 2473/2453/1229; recomputation from pf |
| Forte/Rahn divergence (5-20, 6-29, 6-31, 7-20, 8-26) | *Forte number* article; Rochester set-class table (ecmc.rochester.edu) |
| 6-Z12 / 6-Z43 iv errors | Full recomputation of all 206 rows from pf + Z-mate cross-check (6-Z41, 6-Z17) |

*Draft written 2026-11-07 from the cloned repo at `~/workspace/seqcalc/` (origin `Zetsumetsu/SeqCalc`). No code written; no JUCE project created.*
