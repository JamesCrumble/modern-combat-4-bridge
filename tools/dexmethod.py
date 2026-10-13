#!/usr/bin/env python3
"""Prints methods from a `dexdump -d` listing (extracted/classes.txt).
Usage: tools/dexmethod.py <class substring> [method regex] [--calls]
  --calls  only the invoke/const-string/field lines (the shape of a method without the register noise)"""
import re, sys
args = [a for a in sys.argv[1:] if not a.startswith('--')]
calls = '--calls' in sys.argv
cls_pat = args[0]
meth_pat = re.compile(args[1] if len(args) > 1 else '.')
import os
path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'extracted', 'classes.txt')
head = re.compile(r"^[0-9a-f]+:\s+\|\[[0-9a-f]+\] (.*)$")
show = False
for line in open(path, errors='replace'):
    m = head.match(line)
    if m:
        name = m.group(1)
        c, _, rest = name.partition(':')
        show = cls_pat in c.rsplit('.', 1)[0] and bool(meth_pat.search(c.rsplit('.', 1)[1]))
        if show:
            print('\n=== ' + name)
        continue
    if not show or '|' not in line:
        continue
    body = line.split('|', 1)[1].rstrip()
    if calls and not re.search(r'invoke|const-string|[si](get|put)', body):
        continue
    print(' ' + body)
