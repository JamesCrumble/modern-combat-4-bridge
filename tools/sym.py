#!/usr/bin/env python3
"""Names the function each library offset falls in (extracted/syms.txt). Usage: tools/sym.py 8c4b9c ..."""
import bisect, os, subprocess, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = []
for l in open(root + '/extracted/syms.txt', errors='replace'):
    p = l.split()
    if len(p) == 4 and p[2] == 'FUNC':
        rows.append((int(p[0], 16) & ~1, int(p[1]), p[3]))
rows.sort()
keys = [r[0] for r in rows]
for a in sys.argv[1:]:
    v = int(a, 16)
    i = bisect.bisect_right(keys, v) - 1
    addr, size, name = rows[i]
    try:
        name = subprocess.run(['c++filt', name], capture_output=True, text=True).stdout.strip() or name
    except OSError:
        pass
    print('%08x  %s+0x%x%s' % (v, name, v - addr, '' if v - addr < max(size, 1) else '  (past its end)'))
