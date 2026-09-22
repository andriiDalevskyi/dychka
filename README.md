# Dychka

Digital sibling of Image-Line's **Gross Beat** as a VST3 / Standalone plug-in, built with JUCE 8 on the [Diskach](../diskach) / [Pitarda](../pitarda) / [Minigun](../minigun) technology. Insert it on any track: a **TIME envelope** moves the read head through the last two bars of audio (half-time, reverse, stutters, tape stops, scratches) and a **VOLUME envelope** shapes the gain (gates, side-chain pumps, fades) — both locked to the DAW bars. 36 slots of each kind, selected by click, automation or MIDI notes, every one editable in a point / curve editor.

**English** · [Українська](#dychka-українською)

## Features

- **Two envelopes**: TIME (how far back in the two-bar buffer the read head sits; a line going down = slowing, slope 1 = frozen, slope 2 = reverse, steps = stutter) and VOLUME (a gain curve). 36 slots each; slot 1 is *Off*.
- **49 factory shapes**: half-time (2 bars … 1 beat), quarter-time, reverse (1 bar … 1/8), double-time, stutters (1/4 … 1/32), rolls and builds, tape stops, scratch, freeze, delays; gates (1/4 … 1/32, offbeat, triplet, trance gate, 3-3-2 chop, random), side-chain pumps, ducks, fades, swells, tremolos.
- **Envelope editor**: click to add points, drag to move (snap to 1/4 … 1/32 and triplets, Shift = free), drag a segment to bend it (tension), double-click for a **hold** step, rotate / flip / invert / clear, a **LIBRARY** of factory shapes; the info line tells you the playback speed at the mouse (*speed 0.50×*, *frozen*, *reverse 1.00×*).
- **Per slot**: envelope length (1 beat … 4 bars), **RETRIG** (the envelope restarts when the slot is selected — otherwise it follows the song grid) and **HOLD** (a MIDI note latches the slot instead of being momentary).
- **Controls**: EFFECT ON, SYNC (HOST locks the loops to the DAW position; INTERNAL runs on TEMPO / TAP), CUE, SMOOTH (0 = click-free cross-faded cuts for stutters, higher = tape-like pitch glides), volume ATTACK / RELEASE, MIX, output level.
- **MIDI**: notes select slots — one range of 36 notes for TIME slots (default C4 … B6), another for VOLUME slots (default C1 … B3); momentary or latched per slot; the base notes are parameters.
- **Files**: bank files with all 72 slots, single-envelope files, copy / paste as JSON, drag & drop onto the window; DAW projects store only the slots you changed.
- **Gross Beat banks**: the eight factory Gross Beat presets (Patterns, Momentary, Stutter, Repeater, Turntablist, Juggling Science, Pitch shifter, Flanging) converted from FL Studio's `.fst` files with `tools/gb2dychka.py` — 440 slots in `banks/GrossBeat/`, ready for LOAD BANK.
- Undo / Redo over envelope edits, typable knob fields, Ctrl+click reset, tooltips everywhere.

## Installation

- **VST3**: copy the `Dychka.vst3` folder to `C:\Program Files\Common Files\VST3\` and rescan plug-ins in your DAW. The plug-in appears as *VST3: Dychka (Dallas Audio)* under FX (Delay).
- **Standalone**: run `Dychka.exe`, pick an audio device under *Options* and untick *Mute audio input*.

## Quick start

1. Insert Dychka on a drum loop or a synth (an audio insert). EFFECT ON is lit; SYNC is HOST.
2. Press play and click TIME slot **2** (*Half-time 2 bars*): the loop plays at half speed and restarts every two bars. Try slot **7** (*Reverse 1 bar*) and slot **13** (*Stutter 1/16*).
3. Click a VOLUME slot: **2** gates in eighths, **8** is a side-chain pump.
4. Route a MIDI track to Dychka and play C4 – B6 to switch TIME slots live (release the key to return). Turn on HOLD for a slot that should stay.
5. Draw your own shape: pick an empty slot, click points onto the grid, double-click a segment to make it a step. Save the bank when you are done.

## Building (Windows)

Requirements: Visual Studio 2022 Build Tools (C++ workload), CMake ≥ 3.22, JUCE 8.0.4 in `external/JUCE` (not included in the repository):

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE external/JUCE
```

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release --target Dychka_VST3 Dychka_Standalone
```

Outputs: `build\Dychka_artefacts\Release\VST3\Dychka.vst3` and `build\Dychka_artefacts\Release\Standalone\Dychka.exe`. The offline test is the `DychkaEngineTest` target (exit code 0 = pass).

## Files

- Bank: `Documents\Dychka Banks\<name>.dychka-bank.json` — all 36 + 36 slots.
- Envelope: `<name>.dychka-envelope.json` — one envelope; the same JSON goes through the clipboard with Copy / Paste.
- Gross Beat: `banks/GrossBeat/GB <preset>.dychka-bank.json` — converted factory presets. `python tools/gb2dychka.py "<FL Studio>\Data\Patches\Plugin presets\Effects\Gross Beat" banks/GrossBeat` regenerates them (and converts your own `.fst` presets). Hold steps, curves and jumps are exact; pulse / wave / stairs segments are expanded into points with an estimated cycle count and say so in the slot info.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the data formats, parameters and engine details, and `docs/` for the user manuals (EN / UA).

## Credits

- Concept: Image-Line's Gross Beat (FL Studio). Dychka is not affiliated with Image-Line; "Gross Beat" and "FL Studio" are their trademarks.
- License: GNU AGPL v3.0 or later (see LICENSE).

---

# Dychka українською

Цифровий брат **Gross Beat** від Image-Line у форматі VST3 / Standalone на JUCE 8 — на технологіях [Diskach](../diskach), [Pitarda](../pitarda) і [Minigun](../minigun). Ставите інсертом на будь-який трек: обвідна **TIME** рухає головку відтворення по останніх двох тактах звуку (half-time, реверс, статтери, tape stop, скретч), обвідна **VOLUME** формує гучність (гейти, сайдчейн-помпа, фейди) — обидві прив'язані до тактів DAW. По 36 слотів кожного типу, вибір кліком, автоматизацією чи MIDI-нотами, кожен слот редагується в редакторі точок і кривих.

## Можливості

- **Дві обвідні**: TIME (наскільки головка читання відстає у дво­тактовому буфері: лінія вниз = сповільнення, нахил 1 = стоп-кадр, нахил 2 = реверс, сходинки = статтер) і VOLUME (крива гучності). По 36 слотів; слот 1 — *Off*.
- **49 фабричних форм**: half-time (2 такти … 1 доля), quarter-time, реверс (такт … 1/8), double-time, статтери (1/4 … 1/32), роли й білд-апи, tape stop, скретч, фриз, затримки; гейти (1/4 … 1/32, офбіт, тріолі, транс-гейт, чоп 3-3-2, рандом), сайдчейн-помпа, дакінг, фейди, свели, тремоло.
- **Редактор**: клік додає точку, перетягування рухає (прив'язка 1/4 … 1/32 і тріолі, Shift = вільно), тягнете сегмент — гнете криву (tension), подвійний клік — **hold**-сходинка, поворот / дзеркало / інверсія / очистка, **LIBRARY** з фабричними формами; інфо-рядок показує швидкість відтворення під курсором (*speed 0.50×*, *frozen*, *reverse 1.00×*).
- **На слот**: довжина обвідної (1 доля … 4 такти), **RETRIG** (обвідна стартує з початку, коли слот обрано; інакше йде за сіткою пісні) і **HOLD** (MIDI-нота фіксує слот замість моментального режиму).
- **Керування**: EFFECT ON, SYNC (HOST прив'язує цикли до позиції DAW; INTERNAL — TEMPO / TAP), CUE, SMOOTH (0 = чисті кросфейдові зрізи для статтерів, більше — «плівкові» гліди висоти), ATTACK / RELEASE гучності, MIX, вихідний рівень.
- **MIDI**: ноти обирають слоти — діапазон із 36 нот для TIME (типово C4 … B6) і окремий для VOLUME (типово C1 … B3); моментально або з фіксацією на слот; базові ноти — параметри.
- **Файли**: банки на всі 72 слоти, файли окремих обвідних, копіювання/вставка як JSON, drag & drop на вікно; проєкт DAW зберігає лише змінені слоти.
- **Банки Gross Beat**: вісім фабричних пресетів Gross Beat (Patterns, Momentary, Stutter, Repeater, Turntablist, Juggling Science, Pitch shifter, Flanging), сконвертовані з `.fst`-файлів FL Studio скриптом `tools/gb2dychka.py` — 440 слотів у `banks/GrossBeat/`, відкриваються через LOAD BANK.
- Undo / Redo правок обвідних, поля для введення значень ручок, Ctrl+клік скидає, підказки всюди.

## Встановлення

- **VST3**: скопіюйте папку `Dychka.vst3` в `C:\Program Files\Common Files\VST3\` і пересканируйте плагіни. Плагін — ефект: *VST3: Dychka (Dallas Audio)* серед FX (Delay).
- **Standalone**: запустіть `Dychka.exe`, оберіть аудіопристрій у *Options* і зніміть *Mute audio input*.

## Швидкий старт

1. Поставте Dychka на драм-луп чи синтезатор. EFFECT ON світиться, SYNC = HOST.
2. Натисніть play і клікніть TIME-слот **2** (*Half-time 2 bars*): луп грає вдвічі повільніше і перезапускається кожні два такти. Спробуйте слот **7** (*Reverse 1 bar*) і **13** (*Stutter 1/16*).
3. Клікніть VOLUME-слот: **2** гейтить вісімками, **8** — сайдчейн-помпа.
4. Заведіть MIDI-трек у Dychka і грайте C4 – B6, щоб перемикати TIME-слоти наживо (відпустили клавішу — повернувся попередній). Увімкніть HOLD для слота, який має залишатися.
5. Намалюйте свою форму: оберіть порожній слот, клацайте точки на сітці, подвійним кліком робіть сходинки. Збережіть банк.

Докладніше — в [ARCHITECTURE.md](ARCHITECTURE.md) і в інструкціях у `docs/`.
