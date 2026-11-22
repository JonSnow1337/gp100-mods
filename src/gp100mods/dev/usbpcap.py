#!/usr/bin/env python3
"""Minimal pcapng + USBPcap (linktype 249) reader. Read-only. Used for GP-100 update captures.
API: read_packets(path) -> list of dicts {t, bus, dev, ep, ep_in, xfer, status, func, info, stage, data, irp}"""
import struct, sys

def read_packets(path):
    d = open(path, "rb").read()
    pos = 0; pk = []; tsres = 1e-6
    while pos + 12 <= len(d):
        btype, blen = struct.unpack_from("<II", d, pos)
        body = d[pos + 8: pos + blen - 4]
        if btype == 0x00000001:                       # IDB: look for if_tsresol (opt 9)
            o = 8
            while o + 4 <= len(body):
                code, ln = struct.unpack_from("<HH", body, o)
                if code == 0: break
                if code == 9: tsres = 10.0 ** -(body[o + 4] & 0x7f) if not body[o + 4] & 0x80 else 2.0 ** -(body[o + 4] & 0x7f)
                o += 4 + ((ln + 3) & ~3)
        elif btype == 0x00000006:                      # EPB
            _, tsh, tsl, cap, orig = struct.unpack_from("<IIIII", body, 0)
            t = ((tsh << 32) | tsl) * tsres
            p = body[20:20 + cap]
            if len(p) >= 27:
                hl, irp, status, func, info, bus, dev, ep, xfer, dlen = struct.unpack_from("<HQIHBHHBBI", p, 0)
                stage = p[27] if (xfer == 2 and hl >= 28) else None
                pk.append(dict(t=t, bus=bus, dev=dev, ep=ep & 0x7f, ep_in=bool(ep & 0x80), xfer=xfer, status=status,
                               func=func, info=info, stage=stage, data=bytes(p[hl:hl + dlen]) if dlen else b"", irp=irp, hl=hl))
        pos += blen
    return pk

XFER = {0: "iso", 1: "intr", 2: "ctrl", 3: "bulk"}

if __name__ == "__main__":
    pk = read_packets(sys.argv[1])
    print(len(pk), "packets; t0 =", pk[0]["t"], "span %.1fs" % (pk[-1]["t"] - pk[0]["t"]))
