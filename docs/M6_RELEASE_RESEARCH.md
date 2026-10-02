# M6 macOS 发布研究

研究日期：2026-10-02。范围：现有源码、本机 Qt 6.11.2 与固定版本 Hypit 的只读检查。**没有执行发布构建、macdeployqt 部署、发布包运行或 G6 验收。** G4 完成后再实施。M4 文件正在实现，本文的行号按研究时状态记录。

## 1. 发布目标与依赖边界

目标包为 macOS arm64 的 `qt-video-workbench.app`，包含 Qt 动态框架、Qt 插件、WebEngine helper/资源、原创模板和默认配置。应用自身不能依赖源码目录中的模板、构建目录或开发机 Qt 前缀。

Hypit、Node、FFmpeg/ffprobe 和 HyperFrames 使用的渲染浏览器继续为明确的外部运行依赖。`macdeployqt` 负责 Qt 应用依赖；它不会把 Hypit、Node 或整个视频渲染环境变成应用内置功能。禁止把上游源码或 node_modules 加进本项目 Git，禁止修改全局 PATH/shell 配置。

| 项目 | 实际证据 | 发布处理 |
|---|---|---|
| Qt | `/opt/homebrew/opt/qt` → `../Cellar/qt/6.11.2`；`qtpaths --qt-version` 为 6.11.2 | 构建显式指定此前缀，随后部署至 bundle |
| Qt 工具 | `/opt/homebrew/opt/qt/bin/macdeployqt`；本机 `-help` 成功打印用法但退出 1 | 部署脚本不要把 help 的退出码当成工具不可用 |
| Qt 安装形态 | `/opt/homebrew/opt/qtbase`、`qtwebengine`、`qtdeclarative` 等均为分模块 6.11.2；`qtpaths --query QT_INSTALL_PREFIX` 返回 `/opt/homebrew` | 审查全部实际 install names，不能假设每个框架都位于同一个 qt 目录 |
| Hypit | `../hypit`，0.2.10，commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6` | 外部固定 Distribution；实际工作树仅有未跟踪 `output/`，未发现跟踪源码修改 |
| Node | `/opt/homebrew/bin/node` → 25.8.2；上游声明 >=22.15.0 | 显式配置或进程级 PATH 定位 |
| FFmpeg/ffprobe | `/opt/homebrew/bin/ffmpeg`、`ffprobe`，9.0.2 | 外部依赖；两者都必须在启动诊断中验证 |
| 渲染浏览器 | Hypit 的 `packages/provider-hyperframes-local/package.json` 声明 Chrome Headless Shell 152.0.7928.2 | 此浏览器与 QtWebEngineProcess 是两个独立依赖 |
| 已准备浏览器 | `~/.cache/hyperframes/chrome/chrome-headless-shell/mac_arm-152.0.7928.2/chrome-headless-shell-mac-arm64/chrome-headless-shell` 存在 | 目标机须准备其所选 Runtime；此研究没有启动浏览器或验证渲染 |
| 签名 | `security find-identity -v -p codesigning`：0 valid identities found | 可做 ad hoc 本机验收；没有正式签名身份或公证证据 |

现有应用 Mach-O 的 `LC_BUILD_VERSION` 为 minos 15.0，QtWebEngineCore 为 minos 14.0；本机为 macOS 15.7.7。发布时记录新产物的实际 minos，不能仅凭 Qt 的最低系统版本宣称应用能在更旧系统运行。

## 2. 需要实施的最小变更

### 2.1 Bundle 与资源定位

`app/CMakeLists.txt:52–59` 创建主程序后：

- `MACOSX_BUNDLE` 当前为 `FALSE`，应在 Apple 平台改为 `TRUE`。
- `QVW_TEMPLATE_DIR="${PROJECT_SOURCE_DIR}/templates"` 当前编译进用户机器上的绝对源码路径，应删除。
- 使用一个资源定位 helper，由 `QCoreApplication::applicationDirPath()` 的 `../Resources` 定位 bundle 资源。模板为 `Contents/Resources/templates/title-card`。
- 构建/安装阶段复制完整原创 `templates/` 树：`template.json`、SVML、SVRun、SVS、runtime profile、默认图片和 `packages/title-card/{package.json,activation.js,render.js}` 均需保留。`ProjectStore::create` 需要真实可遍历目录，不能只把模板塞进 qrc 后继续传文件目录路径。
- 开发构建也复制资源到约定运行目录。可保留相对布局的开发回退，不能继续把绝对源码路径编译进发布主程序。
- 设置固定 bundle identifier、显示名称和版本；版本来自项目版本，避免 CMake 与 `main.cpp` 各自长期维护不同版本。

测试目标中的 `QVW_SOURCE_DIR` 属于开发测试定位，不能将这些测试可执行文件作为正式应用运行依赖或附加至发布包。

### 2.2 默认配置与显式配置

`AppConfig.cpp:30–34` 目前只向上搜索 `config/version-lock.json`，不能自动发现 `Contents/Resources/config/version-lock.json`。建议解析顺序为显式 `--config` → bundle 的 Resources/config → 现有开发目录向上搜索。默认资源查找必须先于当前工作目录回退，否则移入其他仓库目录后可能读错配置。

保持 `AppConfig.cpp:76–82` 已有相对路径语义：配置文件所在目录先上一级，再解析 `hypit.distributionPath`。因此默认 bundle 配置可以使用：

```json
{
  "hypit": {
    "distributionPath": "../../../hypit",
    "launcher": "bin/hypit.mjs",
    "version": "0.2.10"
  }
}
```

对于 `/release/qt-video-workbench.app/Contents/Resources/config/version-lock.json`，解析基准是 `/release/qt-video-workbench.app/Contents/Resources`，`../../../hypit` 正好指向 `/release/hypit`。它仍为应用同级的外部 Distribution。缺失时显示配置入口和缺少的依赖。

`--config /somewhere/config/version-lock.json` 的原有基准仍为 `/somewhere`。文档必须说明这一语义，不要不加区分地改成相对于配置文件本身。用户指定的绝对依赖路径可以作为安装配置；不能把当前用户的路径硬编码到 C++ 或发布构建定义。

不宜未经处理直接复制整份开发环境报告型 version-lock 到默认运行配置：其中有当前机器的 Qt 前缀、历史证据和不适用于 bundle 的 `../hypit`。版本证据可作为发布文档；bundle 配置需按发布布局生成。

### 2.3 子进程 PATH 与媒体工具

上游 `bin/hypit.mjs:1` 是 `#!/usr/bin/env node`。当前 `HypitProbe.cpp:67–70` 和 `StudioProcess.cpp:89–99` 直接执行 launcher 并继承进程环境，没有为 Node 配置 PATH。Finder 启动与终端启动的环境可能不同，此问题必须在发布验收中覆盖。

最小方案是给应用及所有相关 QProcess 使用统一的进程环境 helper：保留系统环境，按显式配置与标准安装目录定位外部工具，并仅对当前应用/子进程设置 PATH。macOS 可以检查 `/opt/homebrew/bin`、`/usr/local/bin` 和系统目录，不修改用户 shell 配置。诊断打印实际选择的可执行文件，缺依赖时返回可读错误。

需要贯通版本自检、Studio、M4 CLI 命令和媒体验证。若新增显式 `nodePath`，可以统一用绝对 Node 执行 `[launcher, ...arguments]`；否则进程级 PATH 必须能够满足 launcher 的 env-node shebang。FFmpeg 与 ffprobe 用解析后的绝对路径传给 `MediaValidation`，其 `start` 当前会检查路径是否为真实可执行文件。

与上游已核对的 Runtime profile 契约：

- `provider-media-local/src/activation.ts:21–27` 支持 `ffmpegPath`、`ffprobePath`；缺省值为裸命令名。
- `provider-hyperframes-local/src/activation.ts:25,42–60` 支持 `nodePath`、`chromePath`、`browserVersion`、`browserCacheDirectory`、`ffmpegPath`、`ffprobePath` 等；Node 缺省为 `process.execPath`。
- `runtime-host-node/src/index.ts:354–360` 将带斜杠的相对可执行路径按 Runtime 的 dataRoot 解析，裸命令名仍由 PATH 解析。不要把相对于项目的假设套到这些配置项。
- `provider-hyperframes-local/src/browser.ts:54–79` 只由 Profile 选择浏览器；`chromePath` 和 `browserVersion` 互斥，默认缓存目录由用户主目录动态生成。
- `templates/title-card/hypit.runtime.json` 当前只有两个本地 endpoint，相对 dataRoot 为 `.hypit/runtimes/local`。保留本地 Provider 的限制。

## 3. 打包步骤建议

以下是**待实施命令**，研究阶段未运行。前提：上述 bundle/资源定位和无截图验证模式已实现，G4 已通过。

```bash
cmake -S . -B build-release \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DQVW_BUILD_PROBES=OFF
cmake --build build-release --parallel 4

mkdir -p .workbench/release-macos-arm64
ditto build-release/app/qt-video-workbench.app \
  .workbench/release-macos-arm64/qt-video-workbench.app

# 在最终签名前，放齐原创模板、发布配置与实际第三方说明/许可证。
/opt/homebrew/opt/qt/bin/macdeployqt \
  .workbench/release-macos-arm64/qt-video-workbench.app \
  -verbose=2 -always-overwrite -codesign=-
```

新 staging 路径必须由脚本检查，避免把旧包的残留资源误认为新包完整。`ditto` 适合保留 macOS bundle 与 framework 的目录/符号链接结构。发布包与生成媒体放在已忽略的 `.workbench/` 或专用忽略目录，不能把几百 MiB 的 runtime/framework 误加入 Git。

当前应用为 Widgets + QWebEngineView，没有本项目的 QML 文件；不需要为了部署 Studio 网页传 `-qmldir`。不要使用 `-no-plugins`，必须带平台插件与所需图片插件。

Qt 6.11 的 CMake 也已提供 `qt_generate_deploy_app_script` + `install(SCRIPT ...)`：本机 `Qt6CoreMacros.cmake:3846–3994` 会在 macOS bundle 情况调用部署 API，`Qt6CoreDeploySupport.cmake:690–699,786–797` 转到 macdeployqt。可以选择安装流程；最小脚本方案直接用上面的 macdeployqt，避免重复部署。部署 API 会让 macdeployqt 生成 qt.conf，不要提前创建一个不一致的 qt.conf。

## 4. WebEngine 资源与签名检查

本机文件来自：

```text
/opt/homebrew/opt/qtwebengine/lib/QtWebEngineCore.framework/
  Helpers/QtWebEngineProcess.app/
    Contents/MacOS/QtWebEngineProcess
    Contents/Resources/QtWebEngineProcess.entitlements
  Resources/
    icudtl.dat
    qtwebengine_resources.pak
    qtwebengine_devtools_resources.pak
    qtwebengine_resources_100p.pak
    qtwebengine_resources_200p.pak
    v8_context_snapshot.arm64.bin
    qtwebengine_locales/
```

macdeployqt 后检查 deployed `Contents/Frameworks/QtWebEngineCore.framework` 的实际结构和上述资源；工具版本可能采用 Framework Resources/Helpers 的链接布局，不能只看根目录就断言缺失。还需检查 `Contents/PlugIns/platforms/libqcocoa.dylib`、图片插件和 `Contents/Resources/qt.conf`。文件存在只证明已复制；真实 QWebEngineView 运行验证另做。

本机 helper 的 entitlements 文件实际包含：

```text
com.apple.security.cs.allow-unsigned-executable-memory
com.apple.security.cs.disable-library-validation
com.apple.security.cs.allow-jit
com.apple.security.cs.disable-executable-page-protection
```

[Qt 6.11 WebEngine 部署文档](https://doc.qt.io/qt-6.11/qtwebengine-deploying.html) 要求签名后的 helper 至少保留其随附 entitlements。部署后对 helper 读取签名中的 entitlements；不能只因原始 plist 存在就认为已写入签名。

```bash
codesign --verify --deep --strict --verbose=2 \
  .workbench/release-macos-arm64/qt-video-workbench.app
codesign -d --entitlements - \
  .workbench/release-macos-arm64/qt-video-workbench.app/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app
```

若需要修正 helper 签名或 install names/RPATH，先修正、再按 helper/嵌套代码 → framework → 外层 app 的顺序最终签名并验证。不要把 `codesign --deep` 当成可以随意覆盖所有嵌套 entitlements 的签名策略。

本机 macdeployqt 默认 ad hoc 签名，`-codesign=-` 可明确记录这一选择。`-sign-for-notarization=<identity>` 需要正式身份并启用 hardened runtime/timestamp；本次没有身份、提交公证或 Gatekeeper 跨机器验证。不能把本机 ad hoc 成功写成已公证发行。[Qt 6.11 macOS 部署说明](https://doc.qt.io/qt-6.11/macos-deployment.html) 也说明 macdeployqt 的 entitlements 自动选择规则，app 的 Resources 根目录最多保留一个供该规则使用的 `.entitlements` 文件。

## 5. 链接与搬移验收

### 5.1 静态链接审查

当前开发主程序的 install names 包含 `/opt/homebrew/opt/qtbase`、`qtwebengine`、`qtdeclarative`、`qtwebchannel`、`qtpositioning`，且 LC_RPATH 为 `/opt/homebrew/opt/qt/lib`。这些不能作为最终 Qt 运行依赖保留。

对 bundle 内每个实际 Mach-O（主程序、framework、plugin、helper 和第三方 dylib）运行 `otool -L` 与 `otool -l`。忽略重复的 framework 符号链接路径，但必须覆盖其实际目标文件：

- Qt 与随包第三方库的 install names 应解析到 bundle 内的 `@rpath`、`@loader_path` 或 `@executable_path`。
- `/System/Library` 与 `/usr/lib` 是系统依赖。
- 不得残留指向开发机 `/Users/...`、build tree 或 `/opt/homebrew/...` 的外部 Qt/第三方运行库依赖和 LC_RPATH。若 macdeployqt 未清掉无用的开发 RPATH，按实际 load command 修正，再重签。
- 检查主程序没有绝对 `QVW_TEMPLATE_DIR`。Qt 自身可能保留构建元数据/默认安装前缀字符串，不能用对整个 app 的粗略 strings 搜索替代 install-name/RPATH 审查。
- FFmpeg、Node、Hypit 的配置路径属于已声明外部工具，需在报告中列出，不能混同为随包 Qt 链接遗漏。

本机不允许移动/重命名全局 Qt 安装来“测试无 Qt 环境”。先做递归链接审查和搬移运行，再记录真正无 Qt 的其他目标机器验证是否完成。

### 5.2 `--verify-startup`：无截图运行模式

`main.cpp:31,46,76–82` 的现有 `--selftest` 强制要求 `--out` 并执行 window.grab/save，不适用于本轮用户要求。新增独立 `--verify-startup`，复用已有 deadline、snapshot、loadFinished 和 JS composition-ready 检查，但成功时直接进入关闭/退出清理流程，不捕获任何画面。

带 `--project` 的完整验收至少要求：

1. 从搬移后的 bundle 成功找到模板/default config，或正确处理显式 `--config`；配置日志列出实际工具位置。
2. 使用外部固定 Hypit 启动真实 Studio，读取实际 GET Snapshot，版本/指纹与工程匹配。
3. QWebEngineView 的真实 Studio 页面加载成功；现有同源 iframe 中 `[data-composition-id]` 就绪。若模板使用图片，进一步核对自然尺寸/加载状态，避免把空 composition 容器当作完整预览。
4. 超时、配置/依赖/HTTP/编译/网页失败都返回非零，不进入截图流程。
5. 正常结束关闭自有 Studio 进程组并等待退出，报告中记录退出清理结果；不得仅凭观察进程结束推断所有子进程均已清理。

建议 JSON 证据包含 Qt 版本、资源基准、配置文件、外部依赖位置、project manifest、真实 Studio URL、snapshot revision/fingerprint、pageLoaded、compiledCompositionReady、ownedPid、cleanup 结果、exit code 和 `screenshots:false`。`--verify-startup` 未打开真实工程时可以记录 shell/依赖启动验证，但不能写成完整预览通过。

### 5.3 搬移场景与命令

先在独立目录创建工程，或由后续无截图验证入口调用真实 `DocumentController::create` 从 bundle 模板创建。不能用原源码模板路径创建项目后，就声称验证了 bundle 的“新建”功能。现有 `ProjectE2ETest` 与 `EditorE2ETest` 仍依赖编译进测试程序的 QVW_SOURCE_DIR，可借鉴过程，但其通过不能替代发布主程序验收。

将 `.app`、外置 `config/` 和工程一起移动到含中文空格的目录；从 `/private/tmp` 等非仓库工作目录运行。外置配置可保留现有基准，用 `distributionPath:"runtime/hypit"`；测试目录的 `runtime/hypit` 可以是指向已准备外部固定 Distribution 的链接。报告必须标注该外部链接，不能因此宣称搬移了完整 Hypit 环境。默认 bundle 配置的 `../../../hypit` 场景另单独覆盖。

以下示意假定上述 `--verify-startup` 和依赖定位已实现：

```bash
cd /private/tmp
/usr/bin/env PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  '/private/tmp/M6 中文 空格/qt-video-workbench.app/Contents/MacOS/qt-video-workbench' \
  --verify-startup \
  --config '/private/tmp/M6 中文 空格/config/version-lock.json' \
  --project '/private/tmp/M6 中文 空格/项目/workbench.qvw.json' \
  --session-out '/private/tmp/M6 中文 空格/session.json' \
  --log '/private/tmp/M6 中文 空格/verify.jsonl'
```

这一最小初始 PATH 用于确认应用自身可通过已实现的标准位置/显式配置找到外部工具。不要给启动命令偷偷加原源码目录或 Qt 插件目录，也不要用 `QTWEBENGINEPROCESS_PATH`/`QTWEBENGINE_RESOURCES_PATH` 指回全局 Qt 来弥补部署遗漏。

还应覆盖缺 Hypit、错版本、缺 Node/ffmpeg/ffprobe 时的可读错误与非零验证退出；应用启动/预览成功、MP4 导出成功、成片全片解码通过分别记录。已有终态导出任务包含原工程绝对路径，`ExportWorkspace.cpp:115,122` 会核对 projectRoot；搬移后的历史任务恢复不能无条件声称可继续，应依据 T029 的最终策略验收。

验证结束后制作 zip 或 dmg、记录 SHA-256。先完成资源/链接/签名检查，再归档；保留部署日志、递归链接清单、无截图运行日志/JSON、版本信息和包校验和于 `docs/evidence/m6/`。

## 6. 来源与许可证材料

本节记录实际文件内容对应的发布材料需求，不作许可适用性或商业授权判断。

### Hypit

实际 `../hypit/LICENSE:1–26` 自称 modified Apache 2.0，并附有关于多租户托管/服务、为商业获益向第三方再分发，以及 CLI、run report、manifest 和其衍生用户界面中名称/LOGO/版权信息的附加条件。不能在第三方说明里简化成无条件 Apache-2.0，也不能隐藏真实嵌入 Studio 的上游标识。

发布材料应复制固定 commit 的完整原始 LICENSE，保留其版权行，并记录上游 URL、0.2.10、完整 commit、外部依赖的角色和本项目原创边界。LICENSE:24 引用 Apache 2.0 的其他权利/限制；该许可证再分发条款包括许可证副本、修改标识、相关来源归属与存在 NOTICE 时的归属材料。上游本次检出的源码文件搜索未发现独立 NOTICE；不要编造一个上游 NOTICE。未修改/未打包上游源码时仍可随说明附其原始 LICENSE，让交付者能核对实际条款。

### Qt/WebEngine 与其他组件

本机 Qt 的公开头文件（如 qglobal.h、qwidget.h、qwebenginepage.h）写明 Commercial/LGPL-3.0/GPL-2.0/GPL-3.0 的多许可选择。不要据此自行宣称所有被打包模块已采用某个统一许可。需按实际部署结果收集 Qt 模块、第三方动态库与插件的归属/许可文本。

可用本机材料：

- `/opt/homebrew/opt/qtwebengine/LICENSE.Chromium`：实际为 Chromium 顶层 BSD 风格版权/再分发文本，其 binary 条款要求在文档或其他分发材料重现版权、条件与免责声明。
- `/opt/homebrew/opt/qtwebengine/share/qt/sbom/qtwebengine-6.11.2.spdx`，以及各已部署 Qt 模块的 SBOM。
- 各 Homebrew 模块根目录 `sbom.spdx.json`，可辅助列出包与第三方组件，但不能替代实际许可文本。
- QtWebEngine 的 SBOM 明确注明 WebEngineCore 未列全所消费的第三方依赖，因此 `LICENSE.Chromium` + SBOM 不能被写成所有 Chromium notices 已完备。
- 外部 Chrome Headless Shell 的 `LICENSE.headless_shell` 存在；若未来选择分发该浏览器，需保留其完整多组件 notice 文件。本方案维持外部依赖。
- 本机 FFmpeg 配置实际含 `--enable-gpl --enable-version3`；不能给它贴上“所有 FFmpeg 二进制都是 LGPL”的说明。本方案不把该二进制加入 app。

建议在最终签名前将已核对的 `ThirdParty/` 材料复制到 `Contents/Resources/licenses/`，同份第三方说明也放入源码文档。完整 Qt 与 Chromium 第三方许可材料的收集尚未完成，留待 T030；不能把一个来源表当作所有许可文本已具备的证据。

## 7. 官方参考

- [Qt 6.11：Qt for macOS – Deployment](https://doc.qt.io/qt-6.11/macos-deployment.html)：MACOSX_BUNDLE、平台插件、macdeployqt、签名选项与 entitlements。
- [Qt 6.11：Deploying Qt WebEngine Applications](https://doc.qt.io/qt-6.11/qtwebengine-deploying.html)：helper、资源包、snapshot/locales 和 macOS helper 签名。
- 固定 Hypit 来源：`https://github.com/hypit-ai/hypit`，以本机上述 commit 的源码与 LICENSE 为证据；不使用网页最新分支推断已固定接口。

官方 Qt 文档在 2026-10-02 用 ego-browser 提取文字核对，没有截图；TaskSpace 22 已关闭。浏览器报告 Ego Lite 0.5.0.32 有更新，本研究未升级或修改全局环境。
