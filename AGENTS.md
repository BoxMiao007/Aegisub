# Agent 开发约定(本 fork)

本文件面向在本仓库工作的 AI agent,由 fork 维护者维护,与上游无关。
上游同步合并时,本文件若与上游版本冲突,一律保留本文件版本,不要恢复上游内容。

## 项目是什么

Aegisub 是一款跨平台的高级字幕编辑器,供字幕制作与排版人员使用;本仓库是 BoxMiao007 的 fork,用于承载本 fork 的定制开发并长期同步上游 `TypesettingTools/Aegisub`。

## 协作模式

- 用户的话按"需求与验收期望"理解,不是技术指令;技术选型、实现细节、文件组织由 agent 决定。
- 方案不唯一且影响后续走向(技术栈、数据模型、外部服务、付费依赖)时,用简体中文给 2~3 个带取舍的选项让用户选;能安全推断的直接做,做完说明选了什么、为什么。
- 不问只有程序员才能答的技术判断题;确需用户拍板时,一次问清并附默认建议。

## 完成定义(每次交付前逐项核对)

1. 验证门全部通过:首次先 `meson setup build` 配置,构建 `meson compile -C build`(C++ 编译即类型检查),再 `meson test -C build --verbose --suite Aegisub`。这组命令来自本仓库 CI `.github/workflows/ci.yml`,为 CI 实际在跑的命令。
2. 功能实际跑通:构建后运行 `build/aegisub`(Windows 为 `build/aegisub.exe`)自测核心路径;涉及界面或输出的改动,附截图或运行结果作为交付证据。
3. 交付说明讲给不写代码的人听:做了什么、怎么看到结果(地址/命令/截图)、有什么限制和后续建议;没把握或未完成的部分明说,不藏在细节里。

## 前端调试不干扰用户

调试前端时优先采用后台化、非侵入式方式:headless 浏览器、终端/脚本验证、日志写入文件、dev server 后台运行。

- 动手前先查当前可用的工具与 skills 是否已有非侵入约定(浏览器自动化类 skills 尤其留意);有则按其执行,与本节的约束冲突时取更严格的一条。
- 不自动打开浏览器窗口、不抢窗口焦点、不改剪贴板、不弹通知、不模拟鼠标和键盘。
- 需要可视化时先给截图/日志;确需另开浏览器须使用独立 profile 并告知用户,复用用户已有浏览器前先征得同意。
- 拿不准时选干扰最小的方案。

## 提交与推送

- 仓库约定优先:涉及版本、锁文件、发布、提交格式等事项,先查本仓库是否已有明确约定,有则从其约定。
- 每个需求验证通过后自动提交(用户已授权),提交信息用简体中文写清改动意图。
- 不自动推送;用户说"推"才推送到 `origin`,绝不推送 `upstream`。
- 提交粒度:独立功能、缺陷修复或重构分别提交,避免无关改动混入同一个提交;提交前检查暂存内容,确保只含本次任务明确产生且经过检查的改动。
- 密钥、凭据等敏感内容一律不提交;`.env`、`*.pem`、`*.key` 等本地凭据产物,以及新增的构建产物、本地环境文件类型,在第一次提交前先写进 `.gitignore`(不含真实敏感值的 `.env.example` 等样例除外)。发现疑似敏感内容或无法确认安全性时,说明文件路径并停止提交,交由用户决定;不得为完成提交绕过 Git 钩子或其他安全检查。

## 版本与发布

版本号沿用本仓库既有的编号方式(即上游约定:版本串写在 `meson.build` 的 `version:` 与 `packages/desktop/aegisub.metainfo.xml.in.in` 的 `<release version=...>` 中,git 标签带 `v` 前缀),本文件不另立编号规则。fork 仓库的编号细则见「发布与版本命名」节。

发布流程:

1. 更新承载版本号的文件:`meson.build` 的 `version:` 与 `packages/desktop/aegisub.metainfo.xml.in.in` 的 `<release version=...>` 必须同步改成同一个值;可用 `tools/check-release-version.sh` 核对一致性。
2. 确认版本号改动已在提交中(未提交时先提交),再创建本地 annotated tag:`git tag -a v<版本串> -m "Release v<版本串>"`,**不推送**。
3. 停下并等待用户批准(发布闸门:未获批准不得推送 tag、不得执行 `gh release create`),报告候选版本号与理由。批准前不再产生新提交;若已产生,批准后先删掉本地 tag 重打(未推送的 tag 可以删),再推送。
4. 获批准后推送 tag 并执行 `gh release create`,Release 正文写清本次版本号与变更内容;`gh` 未认证或仓库无 Release 权限时,只推送 tag 并说明原因,不阻断发布。

验证:宣称发布完成须附 `git tag -l 'v<版本串>*'` 与 `gh release view v<版本串>` 实际输出,并确认版本号文件与 tag 一致。

## 知识沉淀(保证任何新会话都能接手)

- 重大技术决策(选型、数据模型、外部服务)在 `docs/decisions.md` 记一行:日期、决策、为什么、放弃了什么。

---

## 分支模型

- `master` 是集成分支:只接受两类提交——合并上游 `upstream/master`、合并功能分支。全局文档改动(如本文件)与发布配套改动(版本号提升、发布构建修复)可直接提交在集成分支。
- 新功能一律在 `feat/<功能名>` 短命分支上开发,完成后合回集成分支。
- 推送目标始终是 `origin`(自己的 fork),绝不向 `upstream` 推送。

## 上游同步

- 节奏:每 1~2 周或上游发版后同步一次,不长期积压。
- **合并前:**
  - 工作区必须干净;有未提交改动先提交或 stash,并向用户说明放到了哪里。
  - 显式 `git fetch upstream`,不用 `git pull`。
  - 先总结 `upstream/master` 新提交,并对照下方「功能清单」标注可能与本 fork 功能重叠的文件。
  - 若发现上游历史被重写(上次合并点不再是 `upstream/master` 的祖先),停下向用户确认,不强行合并。
- **合并中:**
  - 冲突逐个解决:先弄清两边意图再融合;本 fork 已有功能不可被丢弃或绕过;禁止为省事整体取单边。
  - 无法确定取舍时停下向用户确认,不猜。
  - 本文件(`AGENTS.md`)冲突一律保留本 fork 版本。
  - `README.md` 顶部的 `vibe-coding-declaration` 与 `vibe-feature-registry` 标记块一律整体保留,其余内容取上游。
  - 合并搞砸可 `git merge --abort` 完整回到合并前状态,已提交的内容不受影响。
- **合并后:**
  - 依赖文件有变更(`subprojects/*.wrap`、`meson.build`、`meson_options.txt`)先 `meson subprojects download` 再重新 `meson setup build` 配置并验证。
  - 验证门:构建 `meson compile -C build` + 测试 `meson test -C build --verbose --suite Aegisub`。全部通过才算完成。
  - 完整打包(慢):Windows `meson compile win-installer -C build` / `meson compile win-portable -C build`,macOS `meson compile osx-bundle -C build` + `meson compile osx-build-dmg -C build`,Linux `meson dist --include-subprojects -C build`;只在需要产出安装包时执行,不作为每次同步的验证步骤。
  - 验证门通过后做功能巡检:用 `git diff <上一个 sync 标签>..upstream/master --name-only` 列出本次同步引入的上游改动,对照「功能清单」的关键文件,输出受影响功能报告——波及哪些功能、判断依据、建议用户在应用里实际验证的点;没有历史标签时以本次合并的 merge-base 为基准。
  - 巡检完成后打标签 `sync/<YYYY-MM-DD>`;是否推送遵循「提交与推送」节策略,只能推 `origin`,绝不推 `upstream`。

## 发布与版本命名

本节只决定 fork 的版本号怎么编号,不重复发布流程:发布流程与发布闸门按协作层 `AGENTS.md` 的「版本与发布」执行。

fork 发布的版本号要能和上游区分。发布前先看上游最新 release / git 标签的命名格式——本仓库上游用 `v3.5.1` 这类带 `v` 前缀的标签,版本串主体为 `3.5.1`,沿用同一格式:

- 版本号主体取同步进来的上游代码中声明的版本号:上游已正式发布,即该发布版本号;上游未发布但代码里已更新版本号,用更新后的;上游未发布且版本号没动,用上游最新正式发布的版本号(代码里读不到版本号的仓库一律按"没动"处理)。版本号 = 该主体 + `-fork.N`,所有 fork 发布都带后缀。
- `N` 是 fork 自己的修改序号:含本 fork 修改的发布从 1 开始逐次加 1;纯同步上游的发布沿用当前 `N` 不变;还没有任何修改发布时按 0 计(只会出现在预发布,如 `3.5.1-fork.0-pre.20261118`)。发布前数一下已有 git 标签 / GitHub Release 里最大的 `fork.N` 即当前序号(本 fork 目前无任何 tag,序号从 0 起算)。
- 发布所基于的代码超出上游最新正式发布(上游推了新代码还没发版)时,该发布标记为 GitHub Pre-release,版本串末尾加 `-pre.日期`(`-pre.YYYYMMDD`,同一天多次加 `.2`、`.3`),如 `3.5.1-fork.1-pre.20261118`;基于上游正式发布的发布不加,是正式版。
- GitHub Release 标题在版本串后加 `（fork）` 或 `（upstream）`:含本 fork 修改的发布标 `（fork）`,纯同步上游的发布标 `（upstream）`,一眼区分更新来源。该标记只用于标题,不进版本串——代码内版本号、git 标签、产物文件名不带。
- 基于上游最新正式发布且没有任何本 fork 修改时,没有可发布的内容,不发。
- 例:改功能发布正式版 `3.5.1-fork.1`(标签 `v3.5.1-fork.1`,标题 `3.5.1-fork.1（fork）`) → 上游推新代码未发版、版本号没动,同步抢跑 `3.5.1-fork.1-pre.20261118`(Pre-release,标题标 `（upstream）`) → 上游代码版本号改成 3.6.0 仍未发版,再同步发布 `3.6.0-fork.1-pre.20261119`(Pre-release) → 上游正式发布 3.6.0,同步发布正式版 `3.6.0-fork.1`(标题 `3.6.0-fork.1（upstream）`) → 自己再改,发布 `3.6.0-fork.2`(标题标 `（fork）`)。

版本串全链路统一:代码内版本号(`meson.build`、`packages/desktop/aegisub.metainfo.xml.in.in`)、git 标签、GitHub Release、产物文件名都用同一个,不出现两种写法。不发布产物的仓库整节删除。

## 功能清单

本节是功能登记的**唯一事实源**:新功能合入集成分支时在此登记一行,功能下线时移除。agent 需要了解本 fork 有哪些功能时只读本节即可,不要为读功能清单去打开 README(那里只是给人看的镜像)。代码演变见 git 历史(`git log upstream/master..master --no-merges`)。

| 功能 | 一句话说明 | 关键文件/入口 | 引入提交 |
| ---- | ---------- | -------------- | -------- |
| 防剧透遮罩(Spoiler Shield) | 默认遮住视频预览(含烧录字幕),仅可调取景窗内正常显示画面;支持纯色/马赛克且马赛克块大小可调;取景窗四条边与四个角均可拖动缩放;仅影响预览,不影响导出;旧版草稿图设置自动迁移 | src/video_spoiler_shield.cpp、src/video_display.*、src/command/spoiler_shield.cpp、src/spoiler_shield_options.h、default_toolbar/menu/config.json | ea49d4c65 |
| 防剧透字幕文字遮挡(Subtitle Text Mask) | 工具栏/菜单开关；开启后网格、编辑区、原文对照、翻译助手、样式助手、拼写检查、卡拉 OK 与音频标注里的未注释台词只显示实心块，标点、空格、换行与 `{...}` 内容保持可见、位置不变；实时预览隔字遮挡正文（每行第 1 个文字正常、第 2 个遮挡），标点与 `\p` 绘图仍可见；仍可盲改原文，搜索、复制、保存、截图和导出都用真实排版/真实台词 | src/dialogue_text_mask、src/subtitles_provider_libass.cpp、src/drawn_text_mask.*、src/styled_text_mask.*、src/video_preview_mask.h、src/command/grid.cpp、subprojects/libass.wrap、subprojects/packagefiles/libass-mask-glyphs.diff、default_toolbar/menu/config.json | 853984bac |

`README.md` 顶部的 `vibe-feature-registry` 标记块是本表给人看的镜像:登记/移除功能时,同一个动作里同步更新本表和该标记块,防止两处漂移。

## 降低合并冲突的约定

- 能通过新增文件接入的功能,不修改上游既有文件。
- 提交信息写清改动意图(做什么、为什么),便于合并冲突时判断取舍。
- 本 fork 内提交信息用简体中文;如向上游提交 PR,遵循上游仓库的契约(如英文提交、Conventional Commits)。
