#!/usr/bin/env python3
"""Compile a gettext .po file into a binary .mo file.

Used by this fork to produce translated `aegisub.mo` files in development
environments where the gettext `msgfmt` tool is unavailable. Only singular
entries are compiled; plural entries are skipped (they fall back to English).

Usage: python tools/po2mo.py <input.po> <output.mo>
"""
import struct
import sys


def _unescape(s: str) -> str:
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            mapping = {
                "n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\",
                "a": "\a", "b": "\b", "f": "\f", "v": "\v",
            }
            if n in mapping:
                out.append(mapping[n])
            else:
                out.append(n)
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _string_value(line: str) -> str:
    # line like: msgid "...."
    first = line.find('"')
    if first < 0:
        return ""
    return _unescape(line[first + 1:line.rfind('"')])


def parse_po(path):
    entries = {}
    cur = {"ctx": None, "id": None, "str": None, "plural": False}
    field = None  # 'ctx' | 'id' | 'str' | 'id_plural' | 'str_plural'

    def flush():
        nonlocal cur
        if cur["id"] is not None and cur["str"] is not None and not cur["plural"]:
            key = ("%s\x04%s" % (cur["ctx"], cur["id"])) if cur["ctx"] else cur["id"]
            if cur["str"] != "":
                entries[key] = cur["str"]
        cur = {"ctx": None, "id": None, "str": None, "plural": False}

    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n").rstrip("\r")
            if line.strip() == "":
                flush()
                field = None
                continue
            if line.startswith("#"):
                continue
            if line.startswith("msgctxt "):
                cur["ctx"] = _string_value(line)
                field = "ctx"
            elif line.startswith("msgid_plural "):
                cur["plural"] = True
                field = "id_plural"
            elif line.startswith("msgid "):
                cur["id"] = _string_value(line)
                field = "id"
            elif line.startswith("msgstr["):
                cur["plural"] = True
                field = "str_plural"
            elif line.startswith("msgstr "):
                cur["str"] = _string_value(line)
                field = "str"
            elif line.startswith('"'):
                piece = _string_value(line)
                if field == "ctx":
                    cur["ctx"] += piece
                elif field == "id":
                    cur["id"] += piece
                elif field == "str":
                    cur["str"] += piece
                # ignore continuations of plural/ctx we don't use
    flush()
    return entries


def write_mo(entries, out_path):
    items = sorted(((k.encode("utf-8"), v.encode("utf-8")) for k, v in entries.items()),
                   key=lambda kv: kv[0])
    n = len(items)

    key_blob = b"".join(k + b"\0" for k, v in items)
    val_blob = b"".join(v + b"\0" for k, v in items)

    key_offsets = []
    cur = 0
    for k, v in items:
        key_offsets.append((len(k), cur))
        cur += len(k) + 1

    val_offsets = []
    cur = 0
    for k, v in items:
        val_offsets.append((len(v), cur))
        cur += len(v) + 1

    header_size = 7 * 4
    key_table_off = header_size
    val_table_off = key_table_off + 8 * n
    key_data_off = val_table_off + 8 * n
    val_data_off = key_data_off + len(key_blob)

    with open(out_path, "wb") as f:
        f.write(struct.pack("<7I", 0x950412de, 0, n, key_table_off, val_table_off, 0, 0))
        for length, off in key_offsets:
            f.write(struct.pack("<2I", length, key_data_off + off))
        for length, off in val_offsets:
            f.write(struct.pack("<2I", length, val_data_off + off))
        f.write(key_blob)
        f.write(val_blob)


def main():
    if len(sys.argv) != 3:
        print("Usage: po2mo.py <input.po> <output.mo>", file=sys.stderr)
        return 2
    entries = parse_po(sys.argv[1])
    write_mo(entries, sys.argv[2])
    print("compiled %d entries -> %s" % (len(entries), sys.argv[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
