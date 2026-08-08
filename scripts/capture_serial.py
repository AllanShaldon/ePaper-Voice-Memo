#!/usr/bin/env python3
"""Dev helper: reset the board and dump its serial log.

Usage: capture_serial.py [seconds] [--passive]
  --passive  listen without toggling DTR/RTS, so an in-progress session
             (a recording, a network call) is not interrupted.
"""
import sys
import time

import serial

PORT = "/dev/cu.usbserial-10"
BAUD = 115200

secs = 40
passive = "--passive" in sys.argv
for a in sys.argv[1:]:
    if a.isdigit():
        secs = int(a)

p = serial.Serial(PORT, BAUD, timeout=0.2)
if not passive:
    p.setDTR(False)
    p.setRTS(True)
    time.sleep(0.2)
    p.setRTS(False)
    time.sleep(0.05)
    p.setDTR(False)
p.reset_input_buffer()

t0 = time.time()
buf = b""
while time.time() - t0 < secs:
    d = p.read(4096)
    if d:
        buf += d
        sys.stdout.write(d.decode("utf-8", "replace"))
        sys.stdout.flush()
p.close()
print("\n--- %d bytes em %ds ---" % (len(buf), secs))
