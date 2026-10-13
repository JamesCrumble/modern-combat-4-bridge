#!/usr/bin/env python3
"""Guest-function profile from QEMU_GUEST_PROF lines in a game log.

guest_prof.py <log> [first_report [last_report]]

Maps sampled guest PCs to functions of libModernCombat4.so (text_base from the log), of glibc and libpthread in
the armhf sysroot (bases derived from the logged addresses of memset and pthread_mutex_lock) and of the port
executable build/mc4 (base derived from the logged address of main()); everything else is
reported by 64 KB region (libc/SDL/Mesa in the armhf sysroot).
"""
import bisect
import collections
import os
import re
import struct
import sys

ROOT = os.environ.get('MC4_ROOT') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def funcs(path):
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum = struct.unpack_from('<HH', d, 0x2E)
    secs = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
    out = []
    for s in secs:
        if s[1] not in (2, 11):     # .symtab, or .dynsym in a stripped library
            continue
        st = secs[s[6]]
        for i in range(s[5] // 16):
            name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', d, s[4] + i * 16)
            if info & 15 == 2 and value:
                o = st[4] + name
                out.append((value & ~1, max(size, 4), d[o:d.index(b'\0', o)].decode('latin1')))
    out.sort()
    return out


class Mod:
    def __init__(self, path, base, size):
        self.f = funcs(path)
        self.starts = [x[0] for x in self.f]
        self.base, self.size = base, size

    def lookup(self, pc):
        off = pc - self.base
        if not 0 <= off < self.size:
            return None
        i = bisect.bisect_right(self.starts, off) - 1
        if i >= 0 and off < self.f[i][0] + self.f[i][1] + 64:
            return self.f[i][2]
        return '?+0x%x' % off


log = open(sys.argv[1], errors='replace').read().splitlines()
tb = re.search(r'text_base=0x([0-9a-f]+) size=(\d+)', '\n'.join(log))
mb = re.search(r'port main\(\) is at 0x([0-9a-f]+)', '\n'.join(log))
mods = []
if tb:
    mods.append(('game', Mod(ROOT + '/mc4data/lib/armeabi-v7a/libModernCombat4.so', int(tb.group(1), 16), int(tb.group(2)))))
if mb:
    nf = funcs(ROOT + '/port/build/mc4')
    m = [x for x in nf if x[2] == 'main']
    if m:
        base = int(mb.group(1), 16) - m[0][0]
        mods.append(('port', Mod(ROOT + '/port/build/mc4', base, 0x2000000)))

SYSROOT = ROOT + '/port/android/build/sysroot/lib/arm-linux-gnueabihf/'
la = re.search(r'libc addrs: .*memset=0x([0-9a-f]+).*mutex_lock=0x([0-9a-f]+)', '\n'.join(log))
if la:
    for tag, lib, sym, addr in (('libc', 'libc.so.6', 'memset', int(la.group(1), 16)),
                                ('pthread', 'libpthread.so.0', 'pthread_mutex_lock', int(la.group(2), 16))):
        try:
            lf = funcs(SYSROOT + lib)
        except OSError:
            continue
        m = [x for x in lf if x[2] == sym]
        if m:
            mods.append((tag, Mod(SYSROOT + lib, (addr & ~1) - m[0][0], max(x[0] + x[1] for x in lf) + 4096)))

reports = [l for l in log if 'QEMU_GUEST_PROF:' in l]
reports = [l[l.index('QEMU_GUEST_PROF:'):] for l in reports]
lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
hi = int(sys.argv[3]) if len(sys.argv) > 3 else len(reports)
agg = collections.Counter()
total = 0
for l in reports[lo:hi]:
    total += int(re.search(r'PROF: (\d+)', l).group(1))
    for a, n in re.findall(r'0x([0-9a-f]+)=(\d+)', l):
        pc, n = int(a, 16), int(n)
        name = None
        for tag, m in mods:
            r = m.lookup(pc)
            if r:
                name = tag + ':' + r
                break
        agg[name or 'other:%08x' % (pc & 0xFFFF0000)] += n
print('reports %d..%d, %d samples (only the top addresses of each report are counted)' % (lo, hi, total))
for name, n in agg.most_common(35):
    print('%6.1f%%  %s' % (100.0 * n / max(total, 1), name[:120]))
