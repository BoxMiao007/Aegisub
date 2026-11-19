#!/usr/bin/env python3
"""Install-time fallback for systems without the gettext `msgfmt` tool.

Meson's i18n module delegates compilation to msgfmt and silently produces no
catalog when it is missing, which leaves development and portable Windows
builds without any translations. When msgfmt is unavailable, po/meson.build
registers this script instead; it compiles every language listed in LINGUAS
with tools/po2mo.py and installs the result as <locale>/<lang>/aegisub.mo.

Usage (invoked by meson install):
    python compile_catalogs.py <localedir>
where <localedir> is the configured locale directory, relative to the prefix.
"""
import importlib.util
import os
import sys


def _load_po2mo():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "..", "tools", "po2mo.py")
    spec = importlib.util.spec_from_file_location("po2mo", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    if len(sys.argv) != 2:
        print("Usage: compile_catalogs.py <localedir>", file=sys.stderr)
        return 2

    localedir = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    destdir = os.environ.get("MESON_INSTALL_DESTDIR_PREFIX", os.environ.get("MESON_INSTALL_PREFIX", ""))
    dest_root = localedir if os.path.isabs(localedir) else os.path.join(destdir, localedir)

    po2mo = _load_po2mo()
    with open(os.path.join(here, "LINGUAS"), encoding="utf-8") as f:
        languages = [line.strip() for line in f if line.strip() and not line.startswith("#")]

    installed = 0
    for lang in languages:
        po_path = os.path.join(here, lang + ".po")
        if not os.path.isfile(po_path):
            continue
        out_dir = os.path.join(dest_root, lang)
        os.makedirs(out_dir, exist_ok=True)
        po2mo.write_mo(po2mo.parse_po(po_path), os.path.join(out_dir, "aegisub.mo"))
        installed += 1

    print("installed %d translation catalogs under %s" % (installed, dest_root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
