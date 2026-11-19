#!/usr/bin/env python3
"""Compile a gettext .po file into a binary .mo file.

Used by this fork to produce translated `aegisub.mo` files in environments
where the gettext `msgfmt` tool is unavailable (notably the Windows dev
toolchain). Supports singular entries, plurals, contexts and fuzzy flags:

- fuzzy entries are omitted, matching msgfmt's default behaviour;
- plural entries are stored as `msgid\\0msgid_plural` with the translations
  joined by NUL bytes, which is what gettext expects at runtime;
- context entries are stored as `msgctxt\\x04msgid`.

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
            out.append(mapping.get(n, n))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _string_value(line: str) -> str:
    first = line.find('"')
    if first < 0:
        return ""
    return _unescape(line[first + 1:line.rfind('"')])


class _Entry:
    def __init__(self):
        self.ctx = None
        self.msgid = None
        self.msgid_plural = None
        self.msgstr = None
        self.msgstr_plural = {}
        self.fuzzy = False
        self.obsolete = False


def parse_po(path):
    """Return a list of translated entries as (key, value) byte pairs."""
    entries = []
    cur = _Entry()
    field = None  # ctx | id | id_plural | str | str_plural
    plural_index = None

    def flush():
        nonlocal cur, field, plural_index
        if cur.obsolete or cur.fuzzy or cur.msgid is None:
            cur, field, plural_index = _Entry(), None, None
            return

        key = cur.msgid.encode("utf-8")
        if cur.ctx is not None:
            key = cur.ctx.encode("utf-8") + b"\x04" + key

        if cur.msgid_plural is not None:
            key += b"\x00" + cur.msgid_plural.encode("utf-8")
            forms = [cur.msgstr_plural[i] for i in sorted(cur.msgstr_plural)]
            if not forms or all(f == "" for f in forms):
                cur, field, plural_index = _Entry(), None, None
                return
            value = b"\x00".join(f.encode("utf-8") for f in forms)
        else:
            if not cur.msgstr:
                cur, field, plural_index = _Entry(), None, None
                return
            value = cur.msgstr.encode("utf-8")

        entries.append((key, value))
        cur, field, plural_index = _Entry(), None, None

    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n").rstrip("\r")
            if line.startswith("#~"):
                cur.obsolete = True
                continue
            if line.strip() == "":
                flush()
                continue
            if line.startswith("#"):
                if line.replace(" ", "").startswith("#,") and "fuzzy" in line:
                    cur.fuzzy = True
                continue
            if line.startswith("msgctxt "):
                cur.ctx = _string_value(line)
                field = "ctx"
            elif line.startswith("msgid_plural "):
                cur.msgid_plural = _string_value(line)
                field = "id_plural"
            elif line.startswith("msgid "):
                cur.msgid = _string_value(line)
                field = "id"
            elif line.startswith("msgstr["):
                plural_index = int(line[line.index("[") + 1:line.index("]")])
                cur.msgstr_plural[plural_index] = _string_value(line)
                field = "str_plural"
            elif line.startswith("msgstr "):
                cur.msgstr = _string_value(line)
                field = "str"
            elif line.startswith('"'):
                piece = _string_value(line)
                if field == "ctx":
                    cur.ctx += piece
                elif field == "id":
                    cur.msgid += piece
                elif field == "id_plural":
                    cur.msgid_plural += piece
                elif field == "str":
                    cur.msgstr += piece
                elif field == "str_plural":
                    cur.msgstr_plural[plural_index] += piece
    flush()
    return [(k, v) for k, v in entries]


def _table(blob_parts):
    """Build (blob, [(length, offset)]) from a list of byte strings."""
    blob = b""
    table = []
    for part in blob_parts:
        table.append((len(part), len(blob)))
        blob += part + b"\x00"
    return blob, table


def write_mo(entries, out_path):
    items = sorted(entries, key=lambda kv: kv[0])
    n = len(items)

    key_blob, key_table = _table([k for k, _ in items])
    val_blob, val_table = _table([v for _, v in items])

    header_size = 7 * 4
    key_table_off = header_size
    val_table_off = key_table_off + 8 * n
    key_data_off = val_table_off + 8 * n
    val_data_off = key_data_off + len(key_blob)

    with open(out_path, "wb") as f:
        f.write(struct.pack("<7I", 0x950412de, 0, n, key_table_off, val_table_off, 0, 0))
        for length, off in key_table:
            f.write(struct.pack("<2I", length, key_data_off + off))
        for length, off in val_table:
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
