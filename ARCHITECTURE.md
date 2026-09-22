# Dychka — time & volume envelope effect (JUCE 8)

Fixed spec agreed with the user. Implementers: follow this file exactly; do not add features.

Dychka is a digital sibling of Image-Line's *Gross Beat*, built on the Diskach / Pitarda / Minigun
technology (same toolchain, palette, LookAndFeel, knob control, project layout). The input is
written into a two-bar audio buffer; a **TIME envelope** says how far behind the write head the
read head sits (half-time, reverse, stutters, tape stops, scratches), a **VOLUME envelope** is a
gain curve (gates, pumps, fades). Both loop with the song position. 36 slots of each kind are
selected by parameters, automation or MIDI notes, and every slot is editable in a point / curve
editor with snap, hold steps and tension.

## Toolchain
- JUCE 8.0.4 at `external/JUCE` (plain copy from `../diskach/external/JUCE`, not a submodule).
- CMake ≥ 3.22, MSVC 2022 Build Tools (x64), Windows SDK 10.0.26100.
- Configure: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
- Build: `cmake --build build --config Release --target Dychka_VST3 Dychka_Standalone`
- Test: `cmake --build build --config Release --target DychkaEngineTest` then run
  `build\DychkaEngineTest_artefacts\Release\DychkaEngineTest.exe` (exit 0 = pass).
- Targets: VST3 + Standalone. `COPY_PLUGIN_AFTER_BUILD FALSE` — copy `Dychka.vst3` to
  `C:\Program Files\Common Files\VST3` from an elevated shell.
- C++20. Warnings as errors OFF. `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`, `JUCE_VST3_CAN_REPLACE_VST2=0`.
- Plugin: company "Dallas Audio", manufacturer code `Dlas`, plugin code `Dych`, `IS_SYNTH FALSE`,
  `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT FALSE`, VST3 categories `Fx Delay`.
  Buses: one stereo input "Input" (mono or disabled also accepted), one stereo (or mono) output.
- `tools/make_icon.ps1` renders `resources/icon_512.png` / `icon_64.png` (System.Drawing).
  `tools/screenshot.ps1` grabs the standalone window, `tools/crop.ps1` cuts the manual images
  (run the standalone from an interactive shell — inside a sandboxed shell the audio device never
  opens and the window is not created). `docs/build-pdf.ps1` prints the manuals with headless Edge.

## Source layout
```
CMakeLists.txt                  (plug-in + DychkaEngineTest console app)
tests/EngineTest.cpp            (envelope maths, factory data, JSON / file round trips, engine behaviour, MIDI)
docs/                           (manual_en.html, manual_uk.html, manual.css, img/, build-pdf.ps1 -> PDFs)
tools/                          (make_icon.ps1, screenshot.ps1, crop.ps1, gb2dychka.py - Gross Beat .fst -> bank JSON)
banks/GrossBeat/                (the eight factory Gross Beat presets as Dychka banks, 440 slots)
src/
  PluginProcessor.h/.cpp        (owner of the Bank + engine, parameters, undo, bank files, import/export, MIDI slot requests)
  PluginEditor.h/.cpp           (top-level layout 1100x720 fixed, file drop, footer, 30 Hz timer)
  Model/EnvelopeModel.h         (SHARED CONTRACT — Point / Envelope / Bank + JSON + editing helpers)
  Model/FactoryEnvelopes.h/.cpp (hand-written factory bank: 25 time shapes + 24 volume shapes)
  Model/BankStore.h/.cpp        (bank / envelope files, file kind detection)
  Engine/DychkaEngine.h/.cpp    (clock, envelope evaluation, ring buffer read head, declick, gain, mix, MIDI slots)
  UI/DychkaLookAndFeel.h/.cpp   (Diskach's LookAndFeel, renamed)                   [from Diskach]
  UI/KnobControl.h/.cpp         (knob + caption + typable field)                    [from Diskach]
  UI/ClickFocusTextEditor.h                                                          [from Diskach]
  UI/HeaderBar.h/.cpp           (logo, slot LCD T/V, Save/Load bank, Undo/Redo, BEAT + MIDI LEDs, meter, output knob)
  UI/SlotPanel.h/.cpp           (two 4x9 slot grids with thumbnails, slot LCD, Import/Export/Copy/Paste/Reset, slot menu)
  UI/EnvelopeEditor.h/.cpp      (EnvelopeCanvas + toolbar: TIME/VOLUME tabs, name, LENGTH, SNAP, tools, RETRIG, HOLD, LIBRARY)
  UI/ControlPanel.h/.cpp        (EFFECT ON, tempo LCD, CUE, TAP, SYNC, knobs, MIDI slot mapping, help)
```

## Data model (src/Model/EnvelopeModel.h — the contract)
- `kNumSlots = 36` per kind, `kMaxPoints = 512`, `kMaxDelayBeats = 8` (TIME y = 1 → two bars back),
  `kLengthChoices = {1, 2, 4, 8, 16}` beats, default length 8 (two bars, as in Gross Beat).
- `EnvKind { time, volume }`; `flatValue (kind)` = 0 for TIME (no delay), 1 for VOLUME (full level).
- `Point`: `x` 0..1 of the envelope length, `y` 0..1, `tension` −1..1 for the segment that starts at
  the point, `hold` = the segment keeps the point's value until the next point (a step).
  `curveShape (t, τ)`: τ = 0 linear; τ > 0 → `t^e`, τ < 0 → `1 − (1 − t)^e`, `e = 10^|τ|` (1..10).
- `Envelope`: `name`, `info`, `lengthBeats`, `retrigger` (restart at slot activation; else follow
  the song grid), `hold` (MIDI latch), `points` sorted by x with `points[0].x == 0`.
  `valueAt (phase)`: segment = last point with x ≤ phase; hold → its y; after the last point → its
  y (tail hold); two points with the same x form a jump (the later one wins). `clampAll()` sorts,
  clamps, anchors the first point at 0, keeps 1..512 points. Helpers `shift (dx)` (rotate with wrap,
  inserts the cut value at 0 and at 1), `flipX()`, `flipY()`. `Envelope::flat (kind)` = one point.
- `Bank`: `name` + `slots[kind][36]`; `of (kind)`.
- JSON envelope: `{ "format": "dychka-envelope", "version": 1, "kind": "time"|"volume", "name",
  "info", "length", "retrigger", "hold", "points": [ [x, y, tension, hold01], ... ] }` (object-style
  points `{x, y, t, h}` are accepted on read). Bank: `{ "format": "dychka-bank", "version": 1,
  "name", "time": [ { "index", ...envelope } ], "volume": [ ... ] }` — slots not listed are left
  untouched on load. File names: `*.dychka-bank.json`, `*.dychka-envelope.json`.
- **Factory bank** (`FactoryEnvelopes.cpp`): slot 1 of each kind = `Off` (flat). TIME 2–26:
  Half-time 2 bars / 1 bar / 1/2 bar / 1 beat, Quarter-time 1 bar, Reverse 1 bar / 1/2 bar / 1 beat /
  1/8, Reverse half-time 1 bar, Double-time 1 bar, Stutter 1/16 / 1/8 / 1/32 / 1/4 / 1/2 bar, Roll 1 bar,
  Build 1/16–1/32, Tape stop 1 bar / 2 bars, Scratch 1/8, Freeze offbeat, Delay 1/8 / 1 beat / 1 bar.
  VOLUME 2–25: Gate 1/8 / 1/16 / 1/8 offbeat / 1/4 / triplet / 1/32, Pump 1/4 / 1/2 / 1 bar / 1/8, Duck
  beat 1, Fade in / out 2 bars, Swell 1 bar, Ramp 1 beat, Pluck 1 beat, Tremolo 1/8 / 1/16, LFO 1 bar,
  Chop 3-3-2, Trance gate 1 bar, Half-bar duck, Stutter fade, Random gate 2 bars. The rest = `Init` (flat).
  Time geometry: slope of the curve in beats-of-delay per beat = 1 − playback speed (0.5 → half
  speed, 1 → frozen, 2 → reverse, negative → faster than live); a staircase of hold steps repeats a slice.
- **Gross Beat import** (`tools/gb2dychka.py`): a `.fst` preset is an FLP chunk file; event 213 holds
  the plug-in state (48-byte header, 49 for format versions 3 / 8) followed by 72 slot records
  (36 TIME, 36 VOLUME): name, 15 option bytes (byte 13 = restart on select → `retrigger`), three
  int32 (3, 2 or 3, point count), points `{ double dx beats, double y, float tension, int32 mode }`,
  int32 + 16 × 0xFF. One Gross Beat loop = 4 beats (`lengthBeats = 4`); TIME y = 1 is live and 0 is
  two bars back (`y_dychka = 1 − y`), VOLUME y is the gain. A point's mode / tension describe the
  segment ending at it: hold → `hold`, single curves → `tension = −t`, double curves / smooth → two
  halves, half sine → tension ∓0.45, stairs → hold steps (exact count from the end value when the
  staircase is a pure repeat, else `round (1 / t⁴)`), pulse → `2·C` hold steps (`C = 4N` for the named
  `1/N Bt Gate` slots, else `round (1 / t⁴)`), wave → a triangle of `2·C + 1` linear steps. Older
  format versions pack flags into the upper bytes of the mode (masked with 0xFF).

## Parameters (apvts, `dychka::ParamIDs`) — all automatable

| id | name | range | default | note |
|---|---|---|---|---|
| `on` | Effect On | bool | on | switching on restarts the envelopes |
| `timeSlot` | Time Slot | int 1..36 | 1 | |
| `volSlot` | Volume Slot | int 1..36 | 1 | |
| `sync` | Sync | Host / Internal | Host | |
| `tempo` | Tempo | 40..300 BPM (skew 0.6) | 120 | internal sync (also the fallback without a host) |
| `smoothing` | Smoothing | 0..100 | 0 | read-head glide 0..60 ms; 0 = clean cross-faded jumps |
| `attack` | Volume Attack | 0..200 ms (skew 0.4) | 1 | rise time of the gain |
| `release` | Volume Release | 0..200 ms (skew 0.4) | 1 | fall time of the gain |
| `mix` | Mix | 0..100 | 100 | dry .. wet |
| `output` | Output | −60..+6 dB | 0 | after everything |
| `midiTime` | MIDI Time Slots | bool | on | notes select TIME slots |
| `midiVol` | MIDI Volume Slots | bool | on | notes select VOLUME slots |
| `timeNote` | Time Base Note | int 0..92 | 60 (C4) | note of TIME slot 1; slots 2..36 follow |
| `volNote` | Volume Base Note | int 0..92 | 24 (C1) | note of VOLUME slot 1 |

## Processor ↔ UI contract (PluginProcessor.h)
- `getBank()`, `getEditKind()/setEditKind()` (which kind the editor shows; saved with the project),
  `getCurrentSlot (kind)` (= slot parameter − 1), `setCurrentSlot (kind, slot)` (host gesture),
  `getCurrentEnvelope (kind)` (editable), `getEnvelope (kind, slot)`.
- `envelopeEdited (kind)`: clamp, undo step (merged inside a gesture or within 700 ms),
  `engine.publishEnvelope`, `sendChangeMessage()`. `replaceSlot (kind, slot, envelope)` (import /
  paste / reset / library, one undo step), `resetSlotToFactory()`, `isSlotModified()` (≠ factory).
- Undo history = (kind, slot, JSON before) entries over envelope edits only; `undo()` also selects
  the kind and slot. `begin/endUndoGesture()` around drags. Loading a bank or restoring a project
  resets the history.
- Bank files: `saveBankToFile()` (all 72 slots), `loadBankFromFile()` (replaces listed slots),
  `getBankFile()`. `importFile (file, message)`: an envelope file goes into the current slot of the
  kind stored in the file (and selects that kind); a bank file replaces the bank. `exportEnvelopeJson (kind, slot, file)`.
- Transport: `tap()` (taps > 2 s apart start a new sequence; the first tap restarts the envelopes;
  ≥ 2 taps set `tempo`), `restart()` (CUE).
- Engine readouts through `getEngine()`: phase / value / active slot / note-held per kind, bar phase,
  beat, beats per bar, bar counter, effective BPM, following-host flag, smoothed delay in beats,
  smoothed gain; `consumeMidiActivity()`; `hasHostTransport()`, `isHostPlaying()`.
- Timer 30 Hz in the processor: applies the engine's slot requests (MIDI note → slot parameter),
  re-bases the undo commit point when a slot parameter moved (automation) and broadcasts a change.
- State: XML `<Dychka>` with the apvts `PARAMETERS` child and `<bank file name edit>` holding the
  JSON of the slots that differ from the factory bank (so a project stays small and portable).

## Engine behaviour (DychkaEngine, audio thread, no allocation / blocking)
- **Envelope hand-off**: `shared[2][36]` EngineEnvelopes (fixed arrays, no strings) written by the
  message thread under a SpinLock with a version counter; the audio thread copies its two active
  slots with a ScopedTryLock when a slot or the version changed (retries next block if busy).
  `EngineEnvelope::valueAt (phase, cache)` keeps the current segment index, so a running phase is O(1).
- **Clock**: `beatPos` in quarter notes. HOST sync while the host plays: `beatPos = ppq` every block
  (the envelope loops therefore line up with the song's bars; 4/4 from position 0 is assumed, like
  Gross Beat). Otherwise (transport stopped, no play head, INTERNAL): free-running at the host tempo
  (if known) or `tempo`; `requestRestart()` (CUE, first tap, effect switched on) zeroes the beat
  counter in free run and re-anchors both envelopes. Phase of kind k = frac((beatPos − anchor[k]) /
  lengthBeats); anchor[k] = 0 unless the slot has `retrigger`, then it is the beatPos at which the
  slot became active (parameter change, MIDI note or CUE).
- **Slot selection**: slot parameters are edge-triggered. A MIDI note-on inside a kind's range
  activates that slot sample-accurately (always re-anchoring a `retrigger` slot), posts
  `pendingSlot[k]` for the processor (which moves the parameter) and remembers the slot it replaced;
  the parameter echo is recognised (`expectedEcho`) and ignored. Note-off on the held note returns to
  the remembered slot (momentary) and posts that too — unless the slot has `hold`, then it stays
  until another note or a parameter change. Any other parameter change ends the MIDI state.
- **Read head**: the input is written into a ring buffer (power of two ≥ 2 bars at 40 BPM + 0.5 s;
  mono input duplicated). targetDelay = yTIME × 8 beats × samplesPerBeat (clamped to the buffer;
  0 < d < 2 samples is raised to 2). SMOOTH 0: the delay follows the target instantly and any jump
  larger than 1 ms (a hold step, an equal-x jump, the loop wrap) starts a 2 ms equal-gain
  cross-fade from the old read position, so stutters are click-free and pitch-true. SMOOTH > 0: a
  one-pole glide with time constant SMOOTH × 60 ms — jumps become tape-like pitch swoops, ramps lag
  slightly. Reading uses 4-point Hermite interpolation (exact at integer offsets, so slot Off is a
  bit-exact pass-through).
- **Volume**: gain target = yVOLUME, asymmetric one-pole with ATTACK (rising) / RELEASE (falling) ms
  (0 = instant). wet = delayed × gain.
- **Mix**: out = dry + (wet − dry) × mix × onMix; `on` off = bypass through a 6 ms cross-fade.
  Output gain and peak metering in the processor.
- **MIDI**: parsed from the raw bytes (no allocation): note-on → MIDI LED + slot selection (see
  above), note-off → momentary return. Program change and CCs are ignored.

## UI (1100×720, fixed; Minigun/Pitarda/Diskach palette and LookAndFeel)
- Header 56 px: logo (three saw teeth with teal drop lines, the icon) + "DYCHKA", slot LCD
  (`T 03 Half-time 1 bar | V 01 Off`, the edited kind in amber, `*` = edited), Save Bank / Load Bank,
  Undo / Redo, **BEAT** LED (red on beat 1, green on the others, lit for the first 35 % of each beat),
  **MIDI** LED (teal, note-on), 8-segment OUT meter, output knob.
- Body padding 24 / top 18 / bottom 10; footer 28 px: bank file or the last status message (amber, 6 s)
  left; `v0.1.0 · HOST 120.0 BPM · bar N · T03 V01 · −1.50 b · 100 %` right (active slots, smoothed
  delay in beats, smoothed gain).
- Columns: **SlotPanel 300** | 15 | **EnvelopeEditor** (rest = 497) | 15 | **ControlPanel 225**.
- SlotPanel: title `SLOTS · <bank> · N edited`; captions TIME (amber) / VOLUME (teal) with the MIDI note
  range; two 4 × 9 grids (cell = number, thumbnail of the curve, amber dot = edited; amber fill = the
  slot the engine plays (stronger while a MIDI note holds it), teal frame = the slot being edited);
  click = select for playing and editing; right-click = Copy / Paste / Reset / Load factory shape ▸ /
  Export / Import; tooltip = name + info. Slot LCD (hovered or edited slot: `T03 · name · length ·
  RETRIG · HOLD`); buttons Import / Export, Copy / Paste / Reset acting on the edited slot.
- EnvelopeEditor: row 1 — TIME | VOLUME tabs, name field, LENGTH combo (1 beat … 4 bars), SNAP combo
  (1/4 1/8 1/16 1/32 1/12 1/24 off); row 2 — ◀ ▶ (rotate by one snap step), FLIP (mirror in time),
  INV (invert values), CLEAR (flat), INIT (factory reset), RETRIG and HOLD toggles, LIBRARY ▾ (factory
  shapes of the edited kind). Canvas: beat / bar grid with beat labels `bar.beat`; TIME y axis
  `live … −1 bar … −2 bars` (live at the top, like Gross Beat) with faint dashed slope-1 "freeze" guides;
  VOLUME y axis 100 … 0 %; the curve (amber / teal) with a translucent fill, points (white = hold),
  the selected point ringed, the hovered segment tinted; play head (amber line + white dot) while
  the edited slot is the one playing. Mouse: click on empty = add a snapped point and drag it; drag
  a point (x clamped between its neighbours, the first point stays at 0; Shift = no snap; TIME y
  snaps to the same division in beats of delay, VOLUME to 10 %); drag a segment vertically = tension
  (the curve follows the mouse); wheel on a segment = tension ± 0.05; double-click a segment = toggle
  hold; double-click / Delete = remove the point (never the first); right-click = point / segment
  menu (delete, reset value, hold / curve, straight, add point). A drag is one undo gesture. Info line:
  hover readout `beat 1.2.50 · −1.25 beats · speed 0.50x / frozen / reverse 1.00x` or the shape's info.
- ControlPanel: EFFECT ON (teal when on), tempo LCD (`120.0 BPM · ▶ HOST`) + CUE + TAP (accent),
  SYNC combo, knobs SMOOTH MIX / ATTACK RELEASE / TEMPO (`SliderParameterAttachment` on
  `KnobControl::getSlider()`), MIDI SLOT SELECT: TIME NOTES toggle + base-note combo (`C4 .. B6 (60)`),
  VOLUME NOTES toggle + combo; help text.
- Editor: `FileDragAndDropTarget` for `.json` envelope / bank files (teal border while hovering; an
  envelope lands in the current slot of its kind, several files in consecutive slots); Ctrl+Z / Ctrl+Y /
  Ctrl+Shift+Z; text fields take focus only from a click; TooltipWindow 600 ms.
