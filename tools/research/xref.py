"""Find RIP-relative references (lea r64,[rip+disp32]) to given strings in LINE's image memory.
usage: python -I xref.py <pid> key1 key2 ...   -> prints code addresses + function-ish context"""
import sys, re, struct
sys.path.insert(0, '.')
from memscan import k, regions, read

EXEC = 0xF0
pid = int(sys.argv[1])
keys = sys.argv[2:]
h = k.OpenProcess(0x0410, False, pid)
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

img = []      # (base, size, prot, bytes) for MEM_IMAGE regions
for base, size, prot, typ in regions(h):
    if typ == 0x1000000 and size < 0x20000000:
        img.append((base, size, prot, read(h, base, size)))

strs = {}     # address -> key
for base, size, prot, d in img:
    for key in keys:
        for m in re.finditer(b'\x00' + re.escape(key.encode()) + b'\x00', d):
            strs[base + m.start() + 1] = key
print('string copies in image:', len(strs), file=sys.stderr)

lea = re.compile(b'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', re.S)
refs = []
for base, size, prot, d in img:
    if not prot & EXEC:
        continue
    for m in lea.finditer(d):
        i = m.start()
        if i + 7 > len(d):
            continue
        disp = struct.unpack_from('<i', d, i + 3)[0]
        tgt = base + i + 7 + disp
        if tgt in strs:
            refs.append((base + i, strs[tgt]))
for a, key in sorted(refs):
    print('%x  %s' % (a, key))
print('refs:', len(refs), file=sys.stderr)
