#!/usr/bin/env python3
"""Reassemble SysEx messages from USB-MIDI bulk traffic in a USBPcap capture. Read-only."""
from gp100mods.dev.usbpcap import read_packets

def sysex_messages(pk, ep=3):
    """yield (t, direction, bytes) for every complete F0..F7 message on bulk endpoint ep."""
    cur = {True: None, False: None}
    for p in pk:
        if p["xfer"] != 3 or p["ep"] != ep or not p["data"]:
            continue
        # USBPcap: for IN the payload is on the completion, for OUT on the submit; both carry data when non-empty
        d = p["data"]; ein = p["ep_in"]
        for i in range(0, len(d) - 3, 4):
            cin = d[i] & 0xF; b = d[i + 1:i + 4]
            if cin == 4:                                  # sysex start/continue, 3 bytes
                if b[0] == 0xF0: cur[ein] = bytearray()
                if cur[ein] is not None: cur[ein] += b
            elif cin in (5, 6, 7):                        # sysex end with 1/2/3 bytes
                n = cin - 4
                if cur[ein] is None and b[0] == 0xF0: cur[ein] = bytearray()
                if cur[ein] is not None:
                    cur[ein] += b[:n]
                    yield (p["t"], "IN" if ein else "OUT", bytes(cur[ein])); cur[ein] = None
