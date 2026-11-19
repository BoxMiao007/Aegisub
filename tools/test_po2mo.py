#!/usr/bin/env python3
"""Smoke tests for tools/po2mo.py.

Run with: python tools/test_po2mo.py
"""
import gettext
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import po2mo  # noqa: E402


def compile_po(text):
    fd, po_path = tempfile.mkstemp(suffix=".po")
    os.close(fd)
    mo_path = po_path + ".mo"
    with open(po_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    po2mo.write_mo(po2mo.parse_po(po_path), mo_path)
    os.unlink(po_path)
    return mo_path


def test_plural_context_and_fuzzy():
    mo = compile_po(
        'msgid ""\n'
        'msgstr ""\n'
        '"Content-Type: text/plain; charset=UTF-8\\n"\n'
        '"Plural-Forms: nplurals=2; plural=(n != 1);\\n"\n'
        '\n'
        'msgid "1 file"\n'
        'msgid_plural "%d files"\n'
        'msgstr[0] "one file"\n'
        'msgstr[1] "%d files"\n'
        '\n'
        'msgctxt "menu"\n'
        'msgid "Open"\n'
        'msgstr "Open (menu)"\n'
        '\n'
        '#, fuzzy\n'
        'msgid "Do not use"\n'
        'msgstr "fuzzy value"\n'
    )
    try:
        with open(mo, "rb") as f:
            t = gettext.GNUTranslations(f)
        assert t.ngettext("1 file", "%d files", 1) == "one file"
        assert t.ngettext("1 file", "%d files", 3) == "%d files"
        assert t.gettext("Open") == "Open"  # unqualified lookup ignores context
        assert t.gettext("Do not use") == "Do not use"  # fuzzy entry is skipped
    finally:
        os.unlink(mo)


def test_project_catalog():
    po_path = os.path.join(HERE, "..", "po", "zh_CN.po")
    fd, mo = tempfile.mkstemp(suffix=".mo")
    os.close(fd)
    po2mo.write_mo(po2mo.parse_po(po_path), mo)
    try:
        with open(mo, "rb") as f:
            t = gettext.GNUTranslations(f)
        assert t.gettext("Spoiler Shield") == "防剧透遮罩"
        assert t.ngettext("1 frame", "%s frames", 2) == "%s 帧"
    finally:
        os.unlink(mo)


if __name__ == "__main__":
    test_plural_context_and_fuzzy()
    test_project_catalog()
    print("po2mo tests passed")
