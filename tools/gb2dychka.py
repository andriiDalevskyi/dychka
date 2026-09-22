# Converts Image-Line Gross Beat presets (.fst) into Dychka bank files.
#
# Usage: python tools/gb2dychka.py "<Gross Beat preset folder>" banks/GrossBeat
#   e.g. python tools/gb2dychka.py "C:\Program Files\Image-Line\FL Studio 2026\Data\Patches\Plugin presets\Effects\Gross Beat" banks\GrossBeat
#
# The .fst container is FL Studio's FLP chunk format (FLhd / FLdt, event stream). Event 213 holds
# the Gross Beat state: a 48-byte header (49 bytes for the older format versions 3 and 8) and 72
# slot records (36 TIME, then 36 VOLUME):
#   u8 nameLen, name, 15 bytes of slot options, int32 (3), int32 (2 or 3), int32 numPoints,
#   numPoints x { double dx (beats since the previous point), double y, float tension, int32 mode },
#   int32, 16 x 0xFF
# One Gross Beat loop is 4 beats (one bar). y = 1 is "live" for TIME (0 = two bars back) and full
# level for VOLUME. A point's tension and mode describe the segment that ENDS at the point (FL
# convention); Dychka stores them on the point that STARTS the segment.
# Modes: 0 single curve, 1 double curve, 2 hold, 3 stairs, 4 smooth stairs, 5 pulse, 6 wave,
# 7 single curve 2, 8 double curve 2, 9 half sine, 10 smooth, 11 single curve 3, 12 double curve 3.
# Hold and single curves convert exactly (tension sign flipped: FL positive = fast start); the
# repetitive modes (stairs / pulse / wave) are expanded into points: a TIME staircase whose end value
# matches a pure repeat gets its exact step count from the geometry (Inf rep / Repeat slots), the
# named "1/N Bt Gate" pulses get 4N cycles, everything else uses the cycle count round(1 / tension^4),
# which reproduces the named factory gates within one cycle. Waves become triangles. ("Inf rep 1/N" =
# 1/N of a bar, handled by the geometry rule.)
# Slot option byte 13 (set on every slot of the "Momentary" preset and on the one-shot Turntablist
# slots) is taken as "restart the envelope when the slot is selected" -> RETRIG.
import json, math, os, re, struct, sys

def parse_flp(data):
    assert data[:4] == b'FLhd'
    hlen = struct.unpack('<I', data[4:8])[0]
    p = 8 + hlen
    assert data[p:p+4] == b'FLdt'
    dlen = struct.unpack('<I', data[p+4:p+8])[0]
    p += 8
    end = p + dlen
    events = []
    while p < end:
        eid = data[p]; p += 1
        if eid < 64: val = data[p:p+1]; p += 1
        elif eid < 128: val = data[p:p+2]; p += 2
        elif eid < 192: val = data[p:p+4]; p += 4
        else:
            n = 0; shift = 0
            while True:
                b = data[p]; p += 1
                n |= (b & 0x7f) << shift; shift += 7
                if not (b & 0x80): break
            val = data[p:p+n]; p += n
        events.append((eid, val))
    return events

def parse_gross_beat(state):
    ver = struct.unpack('<H', state[:2])[0]
    p = 0x31 if ver in (3, 8) else 0x30
    slots = []
    while p < len(state) - 20 and len(slots) < 72:
        n = state[p]; name = state[p+1:p+1+n].decode('latin-1'); p += 1 + n
        opts = state[p:p+15]; p += 15
        _a, _c, npts = struct.unpack('<iii', state[p:p+12]); p += 12
        pts = []
        for i in range(npts):
            dx, y, t, mode = struct.unpack('<ddfi', state[p + i*24: p + i*24 + 24])
            pts.append((dx, y, t, mode & 0xFF))   # older versions pack flags into the upper bytes
        p += npts * 24 + 20
        slots.append({'name': name, 'opts': opts, 'points': pts})
    if len(slots) != 72:
        raise ValueError('expected 72 slots, got %d' % len(slots))
    return slots

MODE_NAMES = {0: 'curve', 1: 'double curve', 2: 'hold', 3: 'stairs', 4: 'smooth stairs', 5: 'pulse', 6: 'wave',
              7: 'curve 2', 8: 'double curve 2', 9: 'half sine', 10: 'smooth', 11: 'curve 3', 12: 'double curve 3'}
LOOP_BEATS = 4.0
MAX_POINTS = 500

def clamp(v, lo, hi): return max(lo, min(hi, v))

def cycles(t):
    """Number of repetitions of a pulse / wave / stairs segment: FL's tension knob maps roughly to 1 / t^4
    (t = 0.51 -> 15..16 gates, 0.39 -> 32 gates, 1.0 -> a single cycle)."""
    a = abs(t)
    if a < 0.25: return 256
    return int(clamp(round(1.0 / (a ** 4)), 1, 256))

def convert_segment(out, x0, y0, x1, y1, mode, t, approx, kind='time', pulse_cycles=None):
    """Appends the Dychka points that start at x0 for the FL segment x0..x1 (x1's point is added by the caller)."""
    dx = x1 - x0
    if dx <= 1e-9:
        out.append([x0, y0, 0.0, 0]); return
    if mode == 2:
        out.append([x0, y0, 0.0, 1]); return
    if mode in (0, 7, 11):
        out.append([x0, y0, clamp(-t, -1.0, 1.0), 0]); return
    if mode in (1, 8, 12, 10):
        tt = 0.5 if mode == 10 else clamp(abs(t), 0.0, 1.0) * (1 if t >= 0 else -1)
        out.append([x0, y0, clamp(-tt, -1, 1), 0])
        out.append([x0 + dx * 0.5, (y0 + y1) * 0.5, clamp(tt, -1, 1), 0])
        approx.add(MODE_NAMES[mode]); return
    if mode == 9:
        out.append([x0, y0, clamp(-0.45 * (t if abs(t) > 0.1 else 1.0), -1, 1), 0])
        approx.add('half sine'); return
    if mode in (3, 4):
        k = None
        if kind == 'time':
            # A TIME staircase that steps back as far as it advances is a repeat: the head jumps back by one
            # step width at every step, so the end value tells the exact step count (dx / (dx - back)).
            back = (y1 - y0) * 8.0          # beats further back at the end (Dychka y grows with delay)
            span = dx * LOOP_BEATS          # segment length in beats
            if 0 < back < span:
                kx = span / (span - back)
                if abs(kx - round(kx)) < 0.08 and 2 <= round(kx) <= 256:
                    k = int(round(kx))
        if k is None:
            k = cycles(t)
            approx.add(MODE_NAMES[mode])
        k = int(clamp(k, 2, MAX_POINTS - len(out) - 2))
        for i in range(k):
            out.append([x0 + dx * i / k, y0 + (y1 - y0) * i / (k - 1 if k > 1 else 1), 0.0 if mode == 3 else 0.5, 1 if mode == 3 else 0])
        return
    if mode == 5:
        c = pulse_cycles or cycles(t)
        halves = int(clamp(2 * c, 2, MAX_POINTS - len(out) - 2))
        for i in range(halves):
            out.append([x0 + dx * i / halves, y0 if i % 2 == 0 else y1, 0.0, 1])
        if pulse_cycles is None: approx.add('pulse')
        return
    if mode == 6:
        c = cycles(t)
        halves = int(clamp(2 * c + 1, 1, MAX_POINTS - len(out) - 2))
        for i in range(halves):
            out.append([x0 + dx * i / halves, y0 if i % 2 == 0 else y1, 0.0, 0])
        approx.add('wave'); return
    out.append([x0, y0, 0.0, 0])
    approx.add('mode %d' % mode)

def convert_slot(slot, kind, preset, index):
    name = slot['name'].strip()
    pts = slot['points']
    flat = 0.0 if kind == 'time' else 1.0
    if not pts or (not name and len(pts) <= 2):
        return None
    retrig = len(slot['opts']) >= 14 and slot['opts'][13] == 1
    gate = re.match(r'1/(\d+) Bt Gate', name, re.I)
    pulse_cycles = 4 * int(gate.group(1)) if gate and len(pts) == 2 else None
    # absolute x in beats
    xs = []; acc = 0.0
    for dx, y, t, mode in pts:
        acc += dx; xs.append(acc)
    total = xs[-1] if xs else LOOP_BEATS
    scale = 1.0 / LOOP_BEATS
    approx = set()
    out = []
    for i in range(len(pts)):
        x0 = xs[i] * scale
        y0 = (1.0 - pts[i][1]) if kind == 'time' else pts[i][1]
        if i + 1 < len(pts):
            x1 = xs[i + 1] * scale
            y1 = (1.0 - pts[i + 1][1]) if kind == 'time' else pts[i + 1][1]
            _, _, t, mode = pts[i + 1]
            convert_segment(out, x0, y0, x1, y1, mode, t, approx, kind, pulse_cycles)
        else:
            out.append([x0, y0, 0.0, 0])
    if not out: out = [[0.0, flat, 0.0, 0]]
    out[0][0] = 0.0
    for p in out:
        p[0] = clamp(p[0], 0.0, 1.0); p[1] = clamp(p[1], 0.0, 1.0); p[2] = clamp(p[2], -1.0, 1.0)
        p[0] = round(p[0], 6); p[1] = round(p[1], 6); p[2] = round(p[2], 4)
    if len(out) > MAX_POINTS: out = out[:MAX_POINTS]
    modes = sorted(set(MODE_NAMES.get(p[3], str(p[3])) for p in pts[1:]))
    info = 'Gross Beat "%s" slot %d (%d points, %s)' % (preset, index, len(pts), ', '.join(modes))
    if approx: info += ' - approximated: ' + ', '.join(sorted(approx))
    if abs(total - LOOP_BEATS) > 1e-3: info += ' - loop %.2f beats' % total
    return {'format': 'dychka-envelope', 'version': 1, 'kind': kind, 'name': name or 'GB %d' % index,
            'info': info, 'length': 4, 'retrigger': retrig, 'hold': False, 'points': out}

def convert_preset(path):
    data = open(path, 'rb').read()
    events = parse_flp(data)
    plugin = [v.replace(bytes([0]), b'').decode('latin-1') for i, v in events if i == 201]
    if not plugin or 'gross beat' not in plugin[0].lower():
        raise ValueError('%s is not a Gross Beat preset (plugin %r)' % (path, plugin))
    state = [v for i, v in events if i == 213][0]
    slots = parse_gross_beat(state)
    preset = os.path.splitext(os.path.basename(path))[0]
    bank = {'format': 'dychka-bank', 'version': 1, 'name': 'GB ' + preset, 'time': [], 'volume': []}
    stats = {'time': 0, 'volume': 0, 'approx': 0}
    for i, slot in enumerate(slots):
        kind = 'time' if i < 36 else 'volume'
        env = convert_slot(slot, kind, preset, (i % 36) + 1)
        if env is None: continue
        env['index'] = i % 36
        bank[kind].append(env)
        stats[kind] += 1
        if 'approximated' in env['info']: stats['approx'] += 1
    return preset, bank, stats

def main():
    if len(sys.argv) < 3:
        print(__doc__ or 'usage: gb2dychka.py <preset folder> <output folder>'); sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    for fn in sorted(os.listdir(src)):
        if not fn.lower().endswith('.fst'): continue
        preset, bank, stats = convert_preset(os.path.join(src, fn))
        if stats['time'] + stats['volume'] == 0:
            print('%-22s -> skipped (no named slots)' % fn); continue
        out = os.path.join(dst, 'GB ' + preset + '.dychka-bank.json')
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(bank, f, ensure_ascii=False, separators=(',', ':'))
        print('%-22s -> %s  (%d time, %d volume, %d approximated)' % (fn, os.path.basename(out), stats['time'], stats['volume'], stats['approx']))

if __name__ == '__main__':
    main()
