#!/usr/bin/env python3
"""Convert the user's Kangaroo ROM set; validate against the MAME parent set."""
import argparse
from pathlib import Path
from zipfile import ZipFile
from zlib import crc32

ROOT = Path(__file__).resolve().parents[2]
ROMS = [
    ('tvg_75.0', 0x1000, 0x0d18c581), ('tvg_76.1', 0x1000, 0x5978d37a),
    ('tvg_77.2', 0x1000, 0x522d1097), ('tvg_78.3', 0x1000, 0x063da970),
    ('tvg_79.4', 0x1000, 0x9e5cf8ca), ('tvg_80.5', 0x1000, 0x2fc18049),
    ('tvg_81.8', 0x1000, 0xfb449bfd), ('tvg_82.12', 0x800, 0x57766f69),
    ('tvg_83.v0', 0x1000, 0xc0446ca6), ('tvg_85.v2', 0x1000, 0x72c52695),
    ('tvg_84.v1', 0x1000, 0xe4cb26c2), ('tvg_86.v3', 0x1000, 0x9e6a599f),
    ('mb8841_477m.ic29', 0x800, 0x04ca58ee),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', nargs='?', type=Path, default=ROOT/'romszip/kangaroo.zip')
    args = parser.parse_args()
    data = []
    with ZipFile(args.archive) as archive:
        for name, size, crc in ROMS:
            rom = archive.read(name)
            if len(rom) != size or crc32(rom) != crc:
                raise SystemExit('ROM size/CRC mismatch: ' + name)
            data.append(rom)
    target = ROOT/'source/src/machines/kangaroo/kangaroo_rom.h'
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open('w', newline='\n') as out:
        out.write('// Generated from the user-provided Kangaroo ROM archive.\n')
        out.write('#ifndef KANGAROO_ROM_H\n#define KANGAROO_ROM_H\n\n')
        for name, rom in [('main', b''.join(data[:6])), ('sound', data[6]), ('gfx', b''.join(data[8:12])),
                          ('prot', data[7]), ('mcu', data[12])]:
            out.write(f'const unsigned char kangaroo_{name}_rom[] = {{\n')
            for pos in range(0, len(rom), 16):
                out.write('  ' + ', '.join(f'0x{v:02x}' for v in rom[pos:pos+16]) + ',\n')
            out.write('};\n\n')
        out.write('#endif\n')
    print(f'Validated all 13 ROMs, including MB8841; wrote {target}')


if __name__ == '__main__':
    main()
