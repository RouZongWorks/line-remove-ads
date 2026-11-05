"""usage: python -I callers.py <pid> <func-addr-hex>...  -> list direct `call rel32` sites"""
import sys, struct, re
sys.path.insert(0, '.')
from memscan import k, regions, read

pid = int(sys.argv[1])
targets = set(int(a, 16) for a in sys.argv[2:])
h = k.OpenProcess(0x0410, False, pid)
rx = re.compile(b'\xe8', re.S)
for base, size, prot, typ in regions(h):
    if typ != 0x1000000 or not prot & 0xF0 or size > 0x20000000:
        continue
    d = read(h, base, size)
    for m in rx.finditer(d):
        i = m.start()
        if i + 5 > len(d):
            continue
        t = base + i + 5 + struct.unpack_from('<i', d, i + 1)[0]
        if t in targets:
            print('%x -> %x' % (base + i, t))
