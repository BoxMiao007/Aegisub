i18n = import('i18n')
# This should only be used to compile the translations during a build.
# Meson's i18n module is not sufficient to generate a pot file:
# It misses translatable strings not directly found in either
# C++ source, desktop or appdata file. This affects strings
# of the Windows installer (iss), from Lua scripts and JSON files.
# Until a solution is found, POT updates should continue to use make_pot.sh.
# Similarly, since meson insists on regenerating the POT file before updating
# PO files, PO files should be updated using update_po.sh
msgfmt = find_program('msgfmt', required: false)
python = import('python').find_installation()
if msgfmt.found()
    i18n.gettext('aegisub',
                 args: [
                    '-k_', '-kSTR_MENU', '-kSTR_DISP', '-kSTR_HELP', '-kCOMMAND_GROUP:5,',
                    '-kfmt_tl', '-kfmt_plural:2,3',
                    '--sort-by-file'
                 ],
                 install_dir: localedir)
else
    warning('msgfmt not found; compiling translation catalogs with tools/po2mo.py at install time')
endif
# 便携打包使用 meson install --no-rebuild，i18n 目标可能尚未构建；
# 安装时统一用仓库脚本重编译全部 .po，保证新增/修改的翻译一定进入产物。
meson.add_install_script(
    python,
    meson.current_source_dir() / 'compile_catalogs.py',
    get_option('localedir'))
