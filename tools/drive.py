#!/usr/bin/env python3
"""Drives a game running on this PC through its control directory (ctl/).

  drive.py tap X Y [HOLD_S]     press and release at render coordinates
  drive.py swipe X1 Y1 X2 Y2 [S]  drag over S seconds
  drive.py shot NAME [TIMEOUT]  ask for a screenshot and wait for ctl/screenshots/NAME.png
  drive.py frame                print the current frame number
  drive.py wait FRAMES|+N       wait until the frame counter reaches FRAMES (or advances by N)
  drive.py quit

Several commands can be chained with a comma:  drive.py tap 100 50 , wait +20 , shot a
"""
import json
import os
import sys
import time

ROOT = os.environ.get('MC4_ROOT') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
CTL = os.environ.get('MC4_CONTROL_DIR') or os.path.join(ROOT, 'ctl')


def send(line):
    with open(os.path.join(CTL, 'commands'), 'a') as f:
        f.write(line + '\n')


def frame():
    try:
        return json.load(open(os.path.join(CTL, 'status.json'))).get('frame', 0)
    except (OSError, ValueError):
        return -1


def wait_frame(target, timeout=600):
    end = time.time() + timeout
    while time.time() < end:
        if frame() >= target:
            return True
        time.sleep(0.05)
    return False


def run(args):
    cmd = args[0]
    if cmd == 'tap':
        x, y = int(args[1]), int(args[2])
        hold = float(args[3]) if len(args) > 3 else 0.0
        f = frame()
        send('touch 1 %d %d 0' % (x, y))
        wait_frame(f + 3, 60)
        if hold:
            time.sleep(hold)
        send('touch 0 %d %d 0' % (x, y))
        wait_frame(frame() + 2, 60)
    elif cmd == 'swipe':
        x1, y1, x2, y2 = (int(v) for v in args[1:5])
        secs = float(args[5]) if len(args) > 5 else 0.5
        send('touch 1 %d %d 0' % (x1, y1))
        steps = 12
        for i in range(1, steps + 1):
            time.sleep(secs / steps)
            send('touch 2 %d %d 0' % (x1 + (x2 - x1) * i // steps, y1 + (y2 - y1) * i // steps))
        send('touch 0 %d %d 0' % (x2, y2))
    elif cmd == 'shot':
        name = args[1]
        path = os.path.join(CTL, 'screenshots', name + '.png')
        if os.path.exists(path):
            os.remove(path)
        send('screenshot ' + name)
        end = time.time() + (float(args[2]) if len(args) > 2 else 120)
        while time.time() < end and not os.path.exists(path):
            time.sleep(0.1)
        time.sleep(0.2)
        print(path if os.path.exists(path) else 'no screenshot (timeout)', 'frame', frame())
    elif cmd == 'frame':
        print(frame())
    elif cmd == 'wait':
        t = args[1]
        target = frame() + int(t[1:]) if t.startswith('+') else int(t)
        print('reached' if wait_frame(target) else 'timeout', frame())
    elif cmd == 'sleep':
        time.sleep(float(args[1]))
    elif cmd == 'quit':
        send('quit')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    part = []
    for a in sys.argv[1:] + [',']:
        if a == ',':
            if part:
                run(part)
            part = []
        else:
            part.append(a)
