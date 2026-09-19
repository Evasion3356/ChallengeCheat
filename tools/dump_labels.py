"""Resolve every GXT label referenced by challenges_sp.meta / goals_sp.meta in all 13
RDR2 languages, from the extracted global.yldb files (see docs on GXT extraction).

global.yldb is a RAGE resource (RSC8): an in-memory dump, virtual base 0x50000000
== file offset 0. Each label is a hash node {u32 joaat(label), pad}; its string
record found via the node's pointer (see resolve()).
"""
import re, struct, sys, json, xml.etree.ElementTree as ET
from pathlib import Path

GXT = Path(r"D:\Backup\Stuff\RDR2 Shit\GXT")
META = Path(r"D:\Backup\Stuff\RDR2 Shit")
LANGS = ['american','french','german','spanish','italian','brazilian','polish','russian',
         'japanese','korean','chinesetrad','chinesesimp','mexican']  # ids 0..12 (verify vs Localization.h)
BASE = 0x50000000

def joaat(s):
    h = 0
    for c in s.lower().encode():
        h = (h + c) & 0xffffffff; h = (h + (h << 10)) & 0xffffffff; h ^= h >> 6
    h = (h + (h << 3)) & 0xffffffff; h ^= h >> 11
    return (h + (h << 15)) & 0xffffffff

def labels_from_meta():
    out = {}
    for f in ('challenges_sp.meta', 'goals_sp.meta'):
        for el in ET.parse(META / f).iter():
            if el.tag.endswith('Label') and el.text and el.text.strip():
                out.setdefault(el.text.strip(), set()).add(f'{f}:{el.tag}')
    return out

def read_rec(b, p, want_len=None):
    """chars at p+16; length (incl. NUL) is either given (layout A, the record header
    at p is NOT reliable there) or read from the header at p+8 (layout B)."""
    if p < 0 or p + 16 > len(b): return None
    ln = want_len if want_len is not None else struct.unpack_from('<Q', b, p + 8)[0]
    if not 1 <= ln < 4096 or p + 16 + ln > len(b): return None
    raw = b[p + 16:p + 16 + ln]
    if raw[-1] != 0 or 0 in raw[:-1]: return None
    try: return raw[:-1].decode('utf-8')
    except UnicodeDecodeError: return None

def resolve(b, o):
    """Unified rule: the u64 at node+8 points at a cell; the string record {ptr,len}
    is at cell+0x20; chars at ptr+16 (len includes the NUL)."""
    cell = struct.unpack_from('<Q', b, o + 8)[0]
    if not BASE <= cell < BASE + len(b): return None
    rp = cell - BASE + 0x20
    if rp + 16 > len(b): return None
    p, ln = struct.unpack_from('<QQ', b, rp)
    if not BASE <= p < BASE + len(b): return None
    return read_rec(b, p - BASE, ln)

def find_all(b, pat):
    return [m.start() for m in re.finditer(re.escape(pat), b) if b[m.start()+4:m.start()+8] == b'\0\0\0\0']

def main():
    labs = labels_from_meta()
    res = {l: {} for l in labs}
    stats = {}
    for lang in LANGS:
        b = (GXT / f'global_{lang}.yldb').read_bytes()
        miss = amb = 0
        for l in labs:
            vals = {v for o in find_all(b, struct.pack('<I', joaat(l))) if (v := resolve(b, o)) is not None}
            if len(vals) == 1: res[l][lang] = vals.pop()
            elif vals: res[l][lang] = sorted(vals, key=len)[-1]; amb += 1
            else: miss += 1
        stats[lang] = (miss, amb)
    print('label count', len(labs)); [print(k, v) for k, v in stats.items()]
    json.dump({'labels': {l: {'uses': sorted(labs[l]), 'text': res[l]} for l in sorted(labs)}},
              open(GXT / 'labels.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

if __name__ == '__main__': main()
