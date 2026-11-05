"""List NUL-delimited ASCII / UTF-16 strings in LINE's MEM_IMAGE regions matching a regex.
usage: python -I imgstr.py <pid> <regex>   (case-insensitive, de-duplicated, first address shown)"""
import sys, re
sys.path.insert(0, '.')
from memscan import k, regions, read

pid, pat = int(sys.argv[1]), sys.argv[2]
h = k.OpenProcess(0x0410, False, pid)
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
rx_a = re.compile(rb'(?<=\x00)[\x20-\x7e]{4,200}(?=\x00)')
rx_w = re.compile(rb'(?<=\x00\x00)(?:[\x20-\x7e]\x00){4,200}(?=\x00\x00)')
want = re.compile(pat, re.I)
seen = {}
for base, size, prot, typ in regions(h):
    if typ != 0x1000000 or size > 0x20000000:
        continue
    d = read(h, base, size)
    for rx, dec in ((rx_a, 'ascii'), (rx_w, 'utf-16le')):
        for m in rx.finditer(d):
            s = m.group().decode(dec)
            if want.search(s) and s not in seen:
                seen[s] = (base + m.start(), dec)
for s, (a, dec) in sorted(seen.items(), key=lambda kv: kv[1][0]):
    print('%x %s %s' % (a, 'A' if dec == 'ascii' else 'W', s[:150]))
print(len(seen), 'strings', file=sys.stderr)
