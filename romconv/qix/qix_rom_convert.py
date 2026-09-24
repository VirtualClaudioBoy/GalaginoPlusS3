#!/usr/bin/env python3
"""Build the Qix ROM header from the qix.zip owned by the user."""
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[2]
ZIP = ROOT / "romszip" / "qix.zip"
OUT = ROOT / "source" / "src" / "machines" / "qix" / "qix_rom.h"
NVRAM = Path(r"E:\Download\Arcade Time Capsule 2.0.2\RetroVRArcade\Plugins\save\mame2010\nvram\qix.nv")
NVRAM_OUT = OUT.with_name("qix_nvram.h")


def emit_array(out, name, data):
    out.write(f"const unsigned char {name}[] PROGMEM = {{\n")
    for pos in range(0, len(data), 16):
        out.write("  " + ", ".join(f"0x{x:02x}" for x in data[pos:pos + 16]) + ",\n")
    out.write("};\n\n")


with ZipFile(ZIP) as archive:
    names = set(archive.namelist())
    required = {f"u{x}" for x in range(12, 20)} | {f"u{x}" for x in range(4, 11)} | {"u27"}
    missing = required - names
    if missing:
        raise SystemExit("qix.zip is missing: " + ", ".join(sorted(missing)))
    main = b"".join(archive.read(f"u{x}") for x in range(12, 20))
    video = b"".join(archive.read(f"u{x}") for x in range(4, 11))
    sound = archive.read("u27")

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", newline="\n") as out:
    out.write("#ifndef QIX_ROM_H\n#define QIX_ROM_H\n#include <Arduino.h>\n\n")
    emit_array(out, "qix_main_rom", main)
    emit_array(out, "qix_video_rom", video)
    emit_array(out, "qix_sound_rom", sound)
    out.write("#endif\n")

print(f"Wrote {OUT} ({len(main)} + {len(video)} + {len(sound)} bytes)")

if NVRAM.exists():
    nvram = NVRAM.read_bytes()
    if len(nvram) != 0x400:
        raise SystemExit(f"Unexpected qix.nv size: {len(nvram)}")
    with NVRAM_OUT.open("w", newline="\n") as out:
        out.write("#ifndef QIX_NVRAM_H\n#define QIX_NVRAM_H\n#include <Arduino.h>\n\n")
        emit_array(out, "qix_nvram_default", nvram)
        out.write("#endif\n")
    print(f"Wrote {NVRAM_OUT} ({len(nvram)} bytes)")
