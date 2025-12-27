# FX data build tool (arrays output)
# Generates C/C++ header with byte arrays instead of .bin files

VERSION = "1.15-arrays"

import sys
import os
import re
import platform

constants = [
    ("dbmNormal",    0x00),
    ("dbmOverwrite", 0x00),
    ("dbmWhite",     0x01),
    ("dbmReverse",   0x08),
    ("dbmBlack",     0x0D),
    ("dbmInvert",    0x02),

    ("dbmMasked",              0x10),
    ("dbmMasked_dbmWhite",     0x11),
    ("dbmMasked_dbmReverse",   0x18),
    ("dbmMasked_dbmBlack",     0x1D),
    ("dbmMasked_dbmInvert",    0x12),

    ("dbmNormal_end",    0x40),
    ("dbmOverwrite_end", 0x40),
    ("dbmWhite_end",     0x41),
    ("dbmReverse_end",   0x48),
    ("dbmBlack_end",     0x4D),
    ("dbmInvert_end",    0x42),

    ("dbmMasked_end",              0x50),
    ("dbmMasked_dbmWhite_end",     0x51),
    ("dbmMasked_dbmReverse_end",   0x58),
    ("dbmMasked_dbmBlack_end",     0x5D),
    ("dbmMasked_dbmInvert_end",    0x52),

    ("dbmNormal_last",    0x80),
    ("dbmOverwrite_last", 0x80),
    ("dbmWhite_last",     0x81),
    ("dbmReverse_last",   0x88),
    ("dbmBlack_last",     0x8D),
    ("dbmInvert_last",    0x82),

    ("dbmMasked_last",              0x90),
    ("dbmMasked_dbmWhite_last",     0x91),
    ("dbmMasked_dbmReverse_last",   0x98),
    ("dbmMasked_dbmBlack_last",     0x9D),
    ("dbmMasked_dbmInvert_last",    0x92),
]

def println(s):
    sys.stdout.write(s + "\n")
    sys.stdout.flush()

try:
    toolspath = os.path.dirname(os.path.abspath(sys.argv[0]))
    sys.path.insert(0, toolspath)
    from PIL import Image
except Exception as e:
    sys.stderr.write(str(e) + "\n")
    sys.stderr.write("PILlow python module not found or wrong version.\n")
    sys.stderr.write("Make sure the correct module is installed or placed at {}\n".format(toolspath))
    sys.exit(-1)

bytes_out = bytearray()
symbols = []
header_lines = []
label = ""
indent = ""
blkcom = False
namespace = False
include = False
t = 0
path = ""
saveStart = -1

def writeHeader(s):
    header_lines.append(s)

def addLabel(lbl, length):
    symbols.append((lbl, length))
    writeHeader(f"{indent}constexpr uint24_t {lbl} = 0x{length:06X};")

def rawData(filename):
    global path
    with open(path + filename, "rb") as f:
        return bytearray(f.read())

def includeFile(filename):
    global path
    println(f"Including file {path + filename}")
    with open(path + filename, "r") as f:
        return f.readlines()

def imageData(filename):
    global path, symbols, indent
    filename = path + filename

    spriteWidth = 0
    spriteHeight = 0
    spacing = 0
    elements = os.path.basename(os.path.splitext(filename)[0]).split("_")
    lastElement = len(elements) - 1
    i = lastElement
    while i > 0:
        sub = list(filter(None, elements[i].split("x")))
        if len(sub) == 2 and sub[0].isnumeric() and sub[1].isnumeric():
            spriteWidth = int(sub[0])
            spriteHeight = int(sub[1])
            if i < lastElement and elements[i + 1].isnumeric():
                spacing = int(elements[i + 1])
            break
        i -= 1

    img = Image.open(filename).convert("RGBA")
    pixels = list(img.getdata())

    transparency = any(px[3] < 255 for px in pixels)

    if spriteWidth > 0:
        hframes = (img.size[0] - spacing) // (spriteWidth + spacing)
    else:
        spriteWidth = img.size[0] - 2 * spacing
        hframes = 1

    if spriteHeight > 0:
        vframes = (img.size[1] - spacing) // (spriteHeight + spacing)
    else:
        spriteHeight = img.size[1] - 2 * spacing
        vframes = 1

    size = ((spriteHeight + 7) // 8) * spriteWidth * hframes * vframes
    if transparency:
        size *= 2

    out = bytearray([spriteWidth >> 8, spriteWidth & 0xFF, spriteHeight >> 8, spriteHeight & 0xFF])
    out += bytearray(size)

    idx = 4
    fy = spacing
    frames = 0

    for v in range(vframes):
        fx = spacing
        for h in range(hframes):
            for y in range(0, spriteHeight, 8):
                for x in range(0, spriteWidth):
                    b = 0
                    m = 0
                    for p in range(8):
                        b >>= 1
                        m >>= 1
                        if (y + p) < spriteHeight:
                            px = pixels[(fy + y + p) * img.size[0] + fx + x]
                            if px[1] > 64:
                                b |= 0x80
                            if px[3] > 64:
                                m |= 0x80
                            else:
                                b &= 0x7F
                    out[idx] = b
                    idx += 1
                    if transparency:
                        out[idx] = m
                        idx += 1
            frames += 1
            fx += spriteWidth + spacing
        fy += spriteHeight + spacing

    lbl = symbols[-1][0] if symbols else ""
    if lbl:
        if lbl.upper() == lbl:
            writeHeader(f"{indent}constexpr uint16_t {lbl}_WIDTH  = {spriteWidth};")
            writeHeader(f"{indent}constexpr uint16_t {lbl}HEIGHT  = {spriteHeight};")
            if frames > 1:
                writeHeader(f"{indent}constexpr uint8_t  {lbl}_FRAMES = {frames};")
        elif "_" in lbl:
            writeHeader(f"{indent}constexpr uint16_t {lbl}_width  = {spriteWidth};")
            writeHeader(f"{indent}constexpr uint16_t {lbl}_height = {spriteHeight};")
            if frames > 1:
                writeHeader(f"{indent}constexpr uint8_t  {lbl}_frames = {frames};")
        else:
            writeHeader(f"{indent}constexpr uint16_t {lbl}Width  = {spriteWidth};")
            writeHeader(f"{indent}constexpr uint16_t {lbl}Height = {spriteHeight};")
            if frames > 255:
                writeHeader(f"{indent}constexpr uint16_t  {lbl}Frames = {frames};")
            elif frames > 1:
                writeHeader(f"{indent}constexpr uint8_t  {lbl}Frames = {frames};")
        writeHeader("")

    return out

def format_c_array(data: bytes, name: str) -> str:
    lines = []
    lines.append(f"const uint8_t {name}[] PROGMEM = {{")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ("," if i + 16 < len(data) else ""))
    lines.append("};")
    return "\n".join(lines)

def parse_and_build(script_path: str):
    global bytes_out, symbols, header_lines, label, indent, blkcom, namespace, include, t, path, saveStart

    filename = os.path.abspath(script_path)
    path = os.path.dirname(filename) + os.sep
    headerfilename = os.path.splitext(filename)[0] + ".h"

    with open(filename, "r") as f:
        lines = f.readlines()

    println(f"FX data build tool version {VERSION} (arrays)\nUsing Python version {platform.python_version()}")
    println(f"Building FX data using {filename}")

    lineNr = 0
    while lineNr < len(lines):
        parts = [p for p in re.split(r"([ ,]|[\\'].*[\\'])", lines[lineNr]) if p.strip() and p != ","]
        for i in range(len(parts)):
            part = parts[i]

            if part[:1] == "\t":
                part = part[1:]
            if part[:1] == "{":
                part = part[1:]
            if part[-1:] == "\n":
                part = part[:-1]
            if part[-1:] == ";":
                part = part[:-1]
            if part[-1:] == "}":
                part = part[:-1]
            if part[-1:] == ";":
                part = part[:-1]
            if part[-1:] == ".":
                part = part[:-1]
            if part[-1:] == ",":
                part = part[:-1]
            if part[-2:] == "[]":
                part = part[:-2]

            if blkcom:
                p = part.find("*/", 2)
                if p >= 0:
                    part = part[p+2:]
                    blkcom = False
                else:
                    continue

            if not blkcom:
                if part[:2] == "//":
                    break
                if part[:2] == "/*":
                    p = part.find("*/", 2)
                    if p >= 0:
                        part = part[p+2:]
                    else:
                        blkcom = True
                        continue

                if part == "=":
                    pass
                elif part == "const":
                    pass
                elif part == "PROGMEM":
                    pass
                elif part == "align":
                    t = 0
                elif part in ("int8_t", "uint8_t"):
                    t = 1
                elif part in ("int16_t", "uint16_t"):
                    t = 2
                elif part in ("int24_t", "uint24_t"):
                    t = 3
                elif part in ("int32_t", "uint32_t"):
                    t = 4
                elif part == "image_t":
                    t = 5
                elif part == "raw_t":
                    t = 6
                elif part in ("String", "string"):
                    t = 7
                elif part == "include":
                    include = True
                elif part == "datasection":
                    pass
                elif part == "savesection":
                    saveStart = len(bytes_out)
                elif part == "namespace":
                    namespace = True
                elif namespace:
                    namespace = False
                    writeHeader(f"namespace {part}\n{{")
                    indent += "  "
                elif part == "namespace_end":
                    indent = indent[:-2]
                    writeHeader("}\n")
                    namespace = False
                elif (part[:1] == "'") or (part[:1] == '"'):
                    if part[:1] == "'":
                        part = part[1:part.rfind("'")]
                    else:
                        part = part[1:part.rfind('"')]

                    if include:
                        lines[lineNr+1:lineNr+1] = includeFile(part)
                        include = False
                    elif t == 1:
                        bytes_out += part.encode("utf-8").decode("unicode_escape").encode("utf-8")
                    elif t == 5:
                        bytes_out += imageData(part)
                    elif t == 6:
                        bytes_out += rawData(part)
                    elif t == 7:
                        bytes_out += part.encode("utf-8").decode("unicode_escape").encode("utf-8") + b"\x00"
                    else:
                        sys.stderr.write(f"PLATFORM_ERROR in line {lineNr}: unsupported string for type\n")
                        sys.exit(-1)
                elif part[:1].isnumeric() or (part[:1] == "-" and part[1:2].isnumeric()):
                    n = int(part, 0)
                    if t == 4:
                        bytes_out.append((n >> 24) & 0xFF)
                    if t >= 3:
                        bytes_out.append((n >> 16) & 0xFF)
                    if t >= 2:
                        bytes_out.append((n >> 8) & 0xFF)
                    if t >= 1:
                        bytes_out.append((n >> 0) & 0xFF)

                    if t == 0:
                        align_n = n
                        if align_n <= 0:
                            continue
                        r = len(bytes_out) % align_n
                        if r:
                            bytes_out += b"\xFF" * (align_n - r)
                elif part[:1].isalpha():
                    for j in range(len(part)):
                        if part[j] == "=":
                            addLabel(label, len(bytes_out))
                            label = ""
                            rest = part[j+1:]
                            parts.insert(i+1, rest)
                            break
                        elif part[j].isalnum() or part[j] == "_":
                            label += part[j]
                        else:
                            sys.stderr.write(f"PLATFORM_ERROR in line {lineNr}: Bad label: {label}\n")
                            sys.exit(-1)

                    if (label != "") and (i < len(parts) - 1) and (parts[i+1][:1] == "="):
                        addLabel(label, len(bytes_out))
                        label = ""

                    if label != "":
                        for sym, val in constants:
                            if sym == label:
                                if t == 4:
                                    bytes_out.append((val >> 24) & 0xFF)
                                if t >= 3:
                                    bytes_out.append((val >> 16) & 0xFF)
                                if t >= 2:
                                    bytes_out.append((val >> 8) & 0xFF)
                                if t >= 1:
                                    bytes_out.append((val >> 0) & 0xFF)
                                label = ""
                                break

                    if label != "":
                        for sym, off in symbols:
                            if sym == label:
                                if t == 4:
                                    bytes_out.append((off >> 24) & 0xFF)
                                if t >= 3:
                                    bytes_out.append((off >> 16) & 0xFF)
                                if t >= 2:
                                    bytes_out.append((off >> 8) & 0xFF)
                                if t >= 1:
                                    bytes_out.append((off >> 0) & 0xFF)
                                label = ""
                                break

                    if label != "":
                        sys.stderr.write(f"PLATFORM_ERROR in line {lineNr}: Undefined symbol: {label}\n")
                        sys.exit(-1)
                elif len(part) > 0:
                    sys.stderr.write(f"PLATFORM_ERROR unable to parse {part} in element: {str(parts)}\n")
                    sys.exit(-1)

        lineNr += 1

    if saveStart >= 0:
        data_bytes = bytes(bytes_out[:saveStart])
        save_bytes = bytes(bytes_out[saveStart:])
    else:
        data_bytes = bytes(bytes_out)
        save_bytes = b""

    println(f"Saving FX arrays header file {headerfilename}")

    with open(headerfilename, "w") as f:
        f.write("#pragma once\n\n")
        f.write(f"/**** FX data header generated by fxdata-build-arrays.py tool version {VERSION} ****/\n\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <avr/pgmspace.h>\n\n")
        f.write("using uint24_t = uint32_t;\n\n")
        f.write("constexpr uint16_t FX_DATA_PAGE  = 0;\n")
        f.write(f"constexpr uint24_t FX_DATA_BYTES = {len(data_bytes)};\n\n")
        if save_bytes:
            f.write("constexpr uint16_t FX_SAVE_PAGE  = 0;\n")
            f.write(f"constexpr uint24_t FX_SAVE_BYTES = {len(save_bytes)};\n\n")

        for line in header_lines:
            f.write(line + "\n")

        f.write("\n")
        f.write(format_c_array(data_bytes, "fxdata"))
        f.write("\n\n")
        if save_bytes:
            f.write(format_c_array(save_bytes, "fxsave"))
            f.write("\n")

    println(f"Done. Data bytes: {len(data_bytes)}" + (f", Save bytes: {len(save_bytes)}" if save_bytes else ""))

def main():
    if (len(sys.argv) != 2) or (os.path.isfile(sys.argv[1]) != True):
        sys.stderr.write("FX data script file not found.\n")
        sys.exit(-1)
    parse_and_build(sys.argv[1])

if __name__ == "__main__":
    main()
