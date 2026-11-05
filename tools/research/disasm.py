"""usage: python -I dis.py <pid> <addr-hex> <bytes-before> <bytes-after>"""
import sys
sys.path.insert(0, '.')
from memscan import k, read
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

pid, addr, before, after = int(sys.argv[1]), int(sys.argv[2], 16), int(sys.argv[3]), int(sys.argv[4])
h = k.OpenProcess(0x0410, False, pid)
start = addr - before
d = read(h, start, before + after)
md = Cs(CS_ARCH_X86, CS_MODE_64)
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
for i in md.disasm(d, start):
    mark = '=>' if i.address == addr else '  '
    print('%s %x  %-28s %s %s' % (mark, i.address, i.bytes.hex(' '), i.mnemonic, i.op_str))
