#!/usr/bin/env python3
"""Writes quiet stubs for the Java methods the game's library may look up.
For each listed class: every method in classes.dex (extracted/classes.txt, `dexdump -d`) whose name
also occurs as a string in the library (extracted/strings.txt) gets a function that answers the
type's zero value and says so in the log the first time it is called.
Usage: tools/gen_java_stubs.py <Class descriptor> <C prefix> <CLS> [name to skip ...]  > file.inc
The skipped names are the ones written by hand next to the include."""
import os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
desc, prefix, cls = sys.argv[1:4]
skip = set(sys.argv[4:])
names = set(l.rstrip('\n') for l in open(root + '/extracted/strings.txt', errors='replace'))
methods, on, cur = [], False, {}
for line in open(root + '/extracted/classes.txt', errors='replace'):
    m = re.match(r"\s*Class descriptor\s*:\s*'(.*)'", line)
    if m:
        on = m.group(1) == desc
        continue
    if not on:
        continue
    m = re.match(r"\s+(name|type|access)\s+:\s+(.*)", line)
    if not m:
        continue
    k, v = m.groups()
    cur[k] = v.strip().strip("'")
    if k == 'access':
        if cur.get('type', '').startswith('('):
            methods.append((cur['name'], cur['type'], 'STATIC' in v, 'NATIVE' in v))
        cur = {}
CT = {'I': 'jint', 'Z': 'jboolean', 'J': 'jlong', 'F': 'jfloat', 'D': 'jdouble', 'B': 'jbyte', 'S': 'jshort', 'C': 'jchar'}
def split(sig):
    out, i = [], 0
    while i < len(sig):
        j = i
        while sig[j] == '[':
            j += 1
        j = sig.index(';', j) + 1 if sig[j] == 'L' else j + 1
        out.append(sig[i:j]); i = j
    return out
def ctype(t):
    return CT.get(t, 'jstring' if t == 'Ljava/lang/String;' else 'jobject')
seen, rows = set(), []
for name, sig, static, native in methods:
    if native or name.startswith('<') or name not in names or name in skip or (name, sig) in seen:
        continue
    seen.add((name, sig))
    args, ret = re.match(r'\((.*)\)(.*)', sig).groups()
    params = ''.join(', %s a%d' % (ctype(t), i) for i, t in enumerate(split(args)))
    fn = '%s_gen_%d' % (prefix, len(rows))
    if ret == 'V':
        rt, body = 'void', ''
    elif ret == 'Ljava/lang/String;':
        rt, body = 'jobject', 'return (jobject)env->NewStringUTF("");'
    elif ret == '[B':
        rt, body = 'jobject', 'return (jobject)env->NewByteArray(0);'
    elif ret == '[I':
        rt, body = 'jobject', 'return (jobject)env->NewIntArray(0);'
    elif ret in CT:
        rt, body = CT[ret], 'return 0;'
    elif ret in split(args):
        # the same kind of object it was handed (a Bundle filled in and given back)
        rt, body = 'jobject', 'return (jobject)a%d;' % split(args).index(ret)
    else:
        rt, body = 'jobject', 'return NULL;'
    recv = 'jclass c' if static else 'jobject c'
    print('static %s %s(JNIEnv *env, %s%s) { (void)env; GEN_SAID("%s%s"); %s }' % (rt, fn, recv, params, name, sig, body))
    rows.append((fn, name, sig, static))
print('#define %s_GEN_METHODS \\' % prefix.upper())
for fn, name, sig, static in rows:
    reg = 'RegisterStatic' if static else 'Register'
    if not static:
        continue
    print('    ManagedMethod::%s<&%s>(%s::clazz, "%s", "%s"), \\' % (reg, fn, cls, name, sig))
print('    /* end */')
sys.stderr.write('%d methods (%d static)\n' % (len(rows), sum(1 for r in rows if r[3])))
