"""Scan LINE.exe process memory for byte/str patterns. Usage: python -I memscan.py <pid> pat1 pat2 ...
Prints only region base + address + a short window around each hit (no full dumps)."""
import ctypes, ctypes.wintypes as w, sys, re

k = ctypes.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.restype = w.HANDLE
k.VirtualQueryEx.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
k.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


class MBI(ctypes.Structure):
    _fields_ = [('Base', ctypes.c_void_p), ('AllocBase', ctypes.c_void_p), ('AllocProt', w.DWORD),
                ('Pad', w.DWORD), ('Size', ctypes.c_size_t), ('State', w.DWORD),
                ('Protect', w.DWORD), ('Type', w.DWORD), ('Pad2', w.DWORD)]


def regions(h):
    addr = 0
    mbi = MBI()
    while addr < 0x7FFFFFFFFFFF:
        if not k.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
            break
        if mbi.State == 0x1000 and not (mbi.Protect & 0x101):  # committed, not NOACCESS/GUARD
            yield mbi.Base, mbi.Size, mbi.Protect, mbi.Type
        addr = (mbi.Base or 0) + mbi.Size


def read(h, base, size):
    buf = ctypes.create_string_buffer(size)
    n = ctypes.c_size_t()
    if k.ReadProcessMemory(h, ctypes.c_void_p(base), buf, size, ctypes.byref(n)):
        return buf.raw[:n.value]
    return b''


def scan(pid, pats, win=48, limit=40):
    h = k.OpenProcess(0x0410, False, pid)
    regs = [re.compile(re.escape(p.encode()) + b'|' + re.escape(p.encode('utf-16le'))) for p in pats]
    hits = 0
    for base, size, prot, typ in regions(h):
        if size > 0x10000000:
            continue
        data = read(h, base, size)
        for rx in regs:
            for m in rx.finditer(data):
                a = base + m.start()
                seg = data[max(0, m.start() - 8):m.end() + win]
                txt = seg.replace(b'\x00', b'').decode('latin1')
                txt = ''.join(c if 32 <= ord(c) < 127 else '.' for c in txt)
                print('%012x prot=%03x type=%x  %s' % (a, prot, typ, txt))
                hits += 1
                if hits >= limit:
                    return
    print('hits:', hits)


if __name__ == '__main__':
    scan(int(sys.argv[1]), sys.argv[2:])
