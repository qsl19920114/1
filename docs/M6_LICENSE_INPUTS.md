# M6 本机许可证输入盘点

日期：2026-10-02。仅做本地文件、缓存 bottle 清单、Mach-O 链接和 Qt WebEngine 资源的只读研究；没有下载、构建、运行 Qt 应用、复制许可到发布包或修改上游。本文记录可用材料和缺口，**不表示公开分发授权审核通过**。

## 1. 本机材料能否收齐

可以直接取得通用 GNU LGPLv3/GPLv3 全文、已安装候选 Homebrew 库的大多数原始许可文件、固定 Hypit LICENSE、Qt 各模块 SBOM 和 Chromium 顶层 BSD 文本。

目前不能据此声称所有材料完整。主要缺口为：

1. **Qt 6.11.2 所含 Chromium 的完整多组件 notices 未找到。** QtWebEngine 的 SBOM 明确注明其消费的第三方依赖未列全。仅顶层 LICENSE.Chromium 或其他浏览器的 notices 无法补成准确清单。
2. **FreeType 2.14.3 的 FTL.TXT/GPLv2.TXT 及部分模块许可未随本机 bottle 安装。** `LICENSE.TXT` 是许可选择说明，引用这些文件；不能把说明文件当作所有被引用文本均已复制。
3. 若最终需要按许可条件提供相应源码、构建材料或其他说明，本机的已安装二进制/bottle/SBOM 本身不能作为这些源码材料已具备的证据。

后续优先实际尝试 QtWebEngine 内置 credits 页面取证；不可用时记录缺口，按对应 Qt/Chromium 版本从官方源码补齐。FreeType 可以后续从官方 2.14.3 源码收集。本研究不执行这些下载或取证运行。

## 2. Qt 模块范围与材料

从现有应用的 Qt 框架、cocoa 平台插件和本机所有 imageformats 插件出发，递归读取 `otool -L` 和 LC_RPATH，解析绝对路径、`@rpath` 与 `@loader_path` 依赖。候选范围为 64 个实际二进制，无未解析依赖，涉及以下模块。**这是部署前候选清单；正式许可清单应由最终 app 的真实部署结果生成。**

| 本机模块 | 候选运行内容 | 可复制的本机材料 |
|---|---|---|
| qtbase 6.11.2 | Core、DBus、Gui、Network、OpenGL、PrintSupport、Widgets、cocoa/基础图片插件 | `/opt/homebrew/opt/qtbase/share/qt/sbom/qtbase-6.11.2.spdx` 与模块根目录 `sbom.spdx.json` |
| qtdeclarative 6.11.2 | Qml、QmlMeta、QmlModels、QmlWorkerScript、Quick、QuickWidgets | `/opt/homebrew/opt/qtdeclarative/share/qt/sbom/qtdeclarative-6.11.2.spdx` 与根目录 SBOM |
| qtwebengine 6.11.2 | WebEngineCore、WebEngineWidgets；全量图片插件候选还引入 Pdf | `/opt/homebrew/opt/qtwebengine/share/qt/sbom/qtwebengine-6.11.2.spdx`、同目录 `qtpdf-6.11.2.spdx`、根目录 SBOM、`LICENSE.Chromium` |
| qtwebchannel 6.11.2 | WebChannel | `/opt/homebrew/opt/qtwebchannel/share/qt/sbom/qtwebchannel-6.11.2.spdx` 与根目录 SBOM |
| qtpositioning 6.11.2 | Positioning | `/opt/homebrew/opt/qtpositioning/share/qt/sbom/qtpositioning-6.11.2.spdx` 与根目录 SBOM |
| qtsvg 6.11.2 | Svg 与 svg 图片插件（候选） | `/opt/homebrew/opt/qtsvg/share/qt/sbom/qtsvg-6.11.2.spdx` 与根目录 SBOM |
| qtimageformats 6.11.2 | icns、jp2、mng、tiff、wbmp、webp 等插件（候选） | `/opt/homebrew/opt/qtimageformats/share/qt/sbom/qtimageformats-6.11.2.spdx` 与根目录 SBOM |

上述 Qt 安装目录未发现模块顶层 LGPLv3/GPLv3 全文文件。qtbase 的 CMake 第三方脚本 COPYING 和 Wayland 协议许可属于各自具体组件，不能当成 Qt 运行库的完整许可说明。qttools 的安装材料可用于构建来源记录，但不能仅因 Homebrew receipt 列了 qttools 就声称发布应用运行时使用其所有工具。

通用 GNU 文本已有本机来源，可以原样复制并注明文件来源，不必下载：

| 文本 | 本机文件 | 大小 / SHA-256 |
|---|---|---|
| LGPL v3 | `/opt/homebrew/opt/ffmpeg/COPYING.LGPLv3` | 7651 bytes / `da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768` |
| GPL v3 | `/opt/homebrew/opt/ffmpeg/COPYING.GPLv3` | 35147 bytes / `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903` |
| LGPL v2.1 | `/opt/homebrew/opt/glib/LGPL-2.1-or-later.txt` | 本机完整通用文本；必要时同时保留具体组件自己的 COPY/NOTICE 文件 |
| GPL v2 | `/opt/homebrew/opt/ffmpeg/COPYING.GPLv2` | 本机完整通用文本 |

LGPLv3 本身明确包含 GPLv3 的条款并加补充许可，因此不能只复制短 LGPLv3 文件而漏掉配套 GPLv3。此文件来源选择不表示把 FFmpeg 二进制打包进 app，也不表示为 Qt 或应用作了许可选择。

另有 `/Applications/Qt Creator.app/Contents/Resources/debugger/LICENSE.GPL3-EXCEPT`，实际为 The Qt Company GPL Exception 1.0 两项例外加 GPLv3 全文，36363 bytes。它可以作为存在的本机文本来源记录；不应未经具体组件核对，把该 exception 当作所有 Qt runtime 的统一附加许可。Qt Creator 的 generic-highlighter/syntax/licenses 也有 GNU 文本，但其 LGPLv3 排版存在 `licensedocument` 连写，优先使用上表 FFmpeg 提供的原始文本。

## 3. Chromium notices 与内置 credits 的证据

`/opt/homebrew/opt/qtwebengine/LICENSE.Chromium` 实际只有 **1481 bytes、27 行**，SHA-256 为 `f34787ef0342c614b667186a6ec2f5d6b9d650e30142a2788a589a89743e88e9`；它是 Chromium 顶层版权、BSD 风格条件和免责声明，不是多组件 notices。

缓存 bottle 清单也只有该顶层文件：

```text
~/Library/Caches/Homebrew/downloads/
  3b48b79df35fb6eaa049aa5d4cb1c40cad0525701991fe67b87203f43008501c--qtwebengine--6.11.2.arm64_sequoia.bottle.1.tar.gz
    qtwebengine/6.11.2/LICENSE.Chromium
```

已只读解析 QtWebEngineCore.framework/Resources 下两份 PAK：

| 资源文件 | 条目数 | 成功读取方式 | 相关发现 |
|---|---:|---|---|
| qtwebengine_resources.pak | 1631 | 806 plain、779 gzip、46 Brotli；无未读条目 | 未发现 `chrome://credits`、`credits.html`、Apache/GNU 多组件 notices 文本 |
| qtwebengine_devtools_resources.pak | 878 | 55 plain、1 gzip、822 Brotli；无未读条目 | 4 个 DevTools/Lighthouse JS 资源带 Apache license header；不是 Chromium 总 notices |

PAK 的当前本机格式为 version 5；条目表使用 little endian uint16 ID + uint32 offset。Brotli 条目以 `1e 9b` 开头，随后 6 bytes little endian 的未压缩长度，数据从 offset 8 开始。本次用本机 libbrotlidec 对每条资源解码，只检查内容，没有修改或导出 PAK。

本机 QtWebEngineCore Mach-O 中未找到原样 `chrome://credits`、`CreditsUI`、`ChromeUICredits`、`credits.html` 或 `license.html` 字符串；Headers/share/probes 中也未发现该入口的已验证契约。这些负向搜索**不能证明页面一定不可用**，但目前没有足够证据承诺它存在。

本机其他完整 notices 来源及限制：

- `~/.cache/hyperframes/chrome/chrome-headless-shell/mac_arm-152.0.7928.2/chrome-headless-shell-mac-arm64/LICENSE.headless_shell`：1855737 bytes、34832 行；SHA-256 `fa3c4920c528c1cb14b4bfb480b1e8a71add17585b4776ce1f258cd14587b00b`。属于外部 Chrome Headless Shell 152.0.7928.2，不能替代 Qt 内部 Chromium 的准确版本/组件 notices。
- `/Applications/Visual Studio Code.app/Contents/Resources/LICENSES.chromium.html`：19926309 bytes；以及 Trae CN 的同名文件：15101917 bytes。属于各自应用的 Chromium 分发，不能直接贴成 Qt 6.11.2 notices。

### 后续由 root 执行的真实取证方案

使用**本机 Qt 6.11.2 的 QWebEnginePage**，保持 renderer/helper 正常运行，不使用外部 Chrome：

1. 记录 `qVersion()`、`qWebEngineVersion()`、`qWebEngineChromiumVersion()`、`qWebEngineChromiumSecurityPatchVersion()`；后三个函数已在本机 `QtWebEngineCore.framework/Headers/qtwebenginecoreglobal.h:23–26` 核对存在。
2. 连接 `QWebEnginePage::loadingChanged`，记录 status、URL、errorDomain、errorCode、errorString；设置有界超时，再 `load(QUrl("chrome://credits/"))`。
3. 在成功加载后用 `runJavaScript` 取 `{url:location.href,title:document.title,text:document.body.innerText}`，并用 `toHtml` 保存原始 HTML。QWebEnginePage 本机头文件已核对这两个 API 与 loadFinished/loadingChanged。
4. 验证最终 URL 确为请求的内置 credits 页面，内容不是空白、通用错误页、重定向页或只有 Chromium 顶层版权。记录字节数、文本 SHA-256、组件标题数量和多种完整许可文本的存在；对少量开头/中间/结尾内容人工核对。仅 `loadFinished(true)` 不能证明 notices 完整。
5. 成功时按实际内置页面导出 body.innerText + HTML，并记录它对应本机 Qt/Chromium 版本。失败时把 URL、错误码、正文及退出码作为“不可用/未取得”的证据，继续保留缺口；不能用外部浏览器 notices 把该步骤标为通过。

JavaScript 取证表达式：

```javascript
JSON.stringify({
  url: location.href,
  title: document.title,
  text: document.body ? document.body.innerText : ""
})
```

如内页不存在，后续精确源码收集应依据上述 API 实际报告的 Chromium 版本、Qt 6.11.2 的源代码版本与构建信息，不能使用浏览器 152 的版本替代。当前研究没有执行该内页加载或导出。

## 4. 候选 Homebrew dylib 的复制输入

下表全部为已核对存在的本机文件。基准为 `/opt/homebrew/opt/<formula>/`；`opt` 指向已安装 Cellar 版本。建议保留原始文件名，按 formula/version 建子目录。最终只收集实际部署闭包中的组件，不能以 receipt 的全部 runtime_dependencies 代替真实链接清单。

| formula / 当前版本 | 候选 dylib 示例 | 基准下的可复制许可/归属文件 |
|---|---|---|
| brotli 1.2.0 | libbrotlidec、libbrotlicommon | `LICENSE` |
| dbus 1.16.2_1 | libdbus-1 | `COPYING`、`AUTHORS` |
| double-conversion 3.4.0 | libdouble-conversion | `LICENSE`、`COPYING`、`AUTHORS` |
| freetype 2.14.3 | libfreetype | `LICENSE.TXT`、`README`；引用文本有缺口，见下一节 |
| gettext 1.0 | libintl | `COPYING`、`AUTHORS`；按实际 libintl 的声明保留通用 LGPL 配套文本 |
| glib 2.90.0 | libglib-2.0、libgthread-2.0 | `LGPL-2.1-or-later.txt`、`README.md`；安装目录未找到 AUTHORS 文件 |
| graphite2 1.3.15 | libgraphite2 | `COPYING`、`LICENSE`、`README.md` |
| harfbuzz 14.4.0 | libharfbuzz | `COPYING`、`AUTHORS`；COPYING 自身提醒部分子组件许可见源码子目录 |
| icu4c@78 78.3 | libicudata、libicui18n、libicuuc | `LICENSE`、`license.html`；`share/icu/78.3/LICENSE` 也存在 |
| jasper 4.2.9 | libjasper | `LICENSE.txt`、`COPYRIGHT.txt` |
| jpeg-turbo 3.2.0 | libjpeg | `LICENSE.md` **及** `share/doc/libjpeg-turbo/README.ijg`；前者明确引用后者的 IJG 条款 |
| libb2 0.98.1 | libb2 | `COPYING` |
| libmng 2.0.3_1 | libmng | `LICENSE` |
| libpng 1.6.58 | libpng16 | `LICENSE`、`AUTHORS` |
| libtiff 4.7.2 | libtiff | `LICENSE.md`；`share/doc/tiff-4.7.2/LICENSE.md` 也存在 |
| little-cms2 2.19.1 | liblcms2 | `LICENSE`、`AUTHORS` |
| md4c 0.5.3 | libmd4c | `LICENSE.md` |
| openssl@3 3.6.4 | libssl、libcrypto | `LICENSE.txt`、`AUTHORS.md`；LICENSE.txt 包含完整 Apache 2.0 文本 |
| pcre2 10.48 | libpcre2-16、libpcre2-8 | `LICENCE.md`、`COPYING`、`AUTHORS.md`；share/doc/pcre2 也有相同命名材料 |
| webp 1.6.0 | libwebp、libwebpdemux、libwebpmux、libsharpyuv | `COPYING`、`AUTHORS` |
| xz 5.8.4 | liblzma | `COPYING`、`COPYING.0BSD`、`COPYING.LGPLv2.1`、`COPYING.GPLv2`、`COPYING.GPLv3`、`AUTHORS`；根据 liblzma 的实际声明解释，保留这些输入不表示应用用到 xz 工具 |
| zstd 1.5.7_1 | libzstd | `LICENSE`、`COPYING` |

安装 receipt 与当前 opt 版本可能不同（如 gettext/glib 的安装记录历史），复制索引应记录**实际被部署 dylib 的 resolved Cellar 路径、版本、文件 hash**。上表是本次实际 opt 指向与文件内容的盘点。

可选扩大范围时，LZ4 的 `/opt/homebrew/opt/lz4/LICENSE` 只是目录许可说明；完整库 BSD-2-Clause 文本在 `include/lz4.h` 的开头。当前上述 Mach-O 候选闭包没有 liblz4，不能仅因 Homebrew receipt 提到它就宣称已随包部署。

## 5. FreeType 的具体缺口

实际版本为 **2.14.3**，对应 `/opt/homebrew/Cellar/freetype/2.14.3`。

`LICENSE.TXT` 明确指向：

- `docs/FTL.TXT`（FreeType Project License）。
- `docs/GPLv2.TXT`，并说明 GPL 后续版本选项。
- BDF/PCF driver 的 `src/bdf/README`、`src/pcf/README`。
- gzip 模块的 `src/gzip/zlib.h` 及另外的 Old MIT 声明。

这些具体文件未在安装目录或本机缓存 bottle 中找到。缓存路径为：

```text
~/Library/Caches/Homebrew/downloads/
  bd059c70f6b8a078b04d5b7cb18d1cc51599e39dbeb2d1a87724e47dbf5fd972--freetype--2.14.3.arm64_sequoia.bottle.tar.gz
```

本机 `include/freetype2/freetype/freetype.h` 含 David Turner、Robert Wilhelm、Werner Lemberg 的 1996–2026 版权行，但仍指回 LICENSE.TXT，不能补齐 FTL 全文与各 driver 许可。

外部 Headless Shell 的 LICENSE.headless_shell:9203 起有完整通用 FreeType Project LICENSE（2006-Jan-27）；它可以提供“本机有该文本”的线索，但不证明已覆盖此 FreeType 2.14.3 构建及其模块归属。最终建议从对应 2.14.3 官方源码收集上述文件，并在索引中记录实际来源。

## 6. 最小后续复制方案

这是给 root 后续收集脚本的输入建议，不是已执行步骤：

```text
Contents/Resources/licenses/
  INPUTS.json                 # 源文件、resolved 版本、hash、角色、缺口
  Hypit/0.2.10/LICENSE        # 固定 commit 的原始附加条件全文，注明外部依赖
  GNU/LGPL-3.0.txt            # 本机 FFmpeg COPYING.LGPLv3 原样副本
  GNU/GPL-3.0.txt             # 本机 FFmpeg COPYING.GPLv3 原样副本
  GNU/LGPL-2.1.txt            # 本机 GLib 通用文本
  Qt/6.11.2/LICENSE.Chromium  # 顶层文本；明确不是所有 notices
  Qt/6.11.2/sbom/             # 实际部署模块的 Qt SPDX 与 Homebrew SBOM
  Qt/6.11.2/Chromium-notices.txt/html  # 仅真实取证/准确来源取得后放入
  Homebrew/<formula>/<version>/<原文件名>
  GAPS.md                    # 尚未取得的版本对应 notices/源码材料
```

索引应区分随包 runtime、仅外部运行依赖、构建工具和参考文本来源。保留本机目录下实际的 LICENSE/COPYING/AUTHORS 文件，并对它们引用的其他文件逐项记录“取得/缺失”。只有文件名不含 LICENSE 的 `README.ijg` 也必须进入复制清单。

先根据正式 app 的递归 Mach-O 清单确定真实 formula/version，再按本表收集并核对缺口。复制许可证文本和 SBOM 不等于把所有义务、源码材料或商业授权自动处理完毕；最终发布说明应准确写出本机已取得的材料及仍未完成的部分。
