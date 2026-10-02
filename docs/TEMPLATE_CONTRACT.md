# 原创 title-card 模板与上游契约

版本：Hypit 0.2.10，commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。

本项目 `templates/title-card/` 的布局、文案、几何占位图、Surface、Producer 与 Companion 为本项目实现。复用 Hypit 的编译、资源解析、字体、视觉轨道与渲染器；不复制上游包到工程。

## 源码依据（相对于 Hypit 检出）

- `examples/semantic-composition/packages/chat-scene/src/activation.ts`：Structured Surface 的 window projection、fragment input、render producer 与 activation 结构。
- `packages/media/src/surface.ts:84–118`：`resolveAsset({from,mediaType,range})` 返回 artifact，记录类型为 `artifactTypes.blob`。
- `packages/media-track/src/lower.ts:187–195`、`packages/composition/src/track.ts:335–342`：图片元素用 `kind: image` 和 BlobRef artifact。
- `packages/interview-emoji-reveal/src/program.ts:201–243`：Visual IR 的 box/text/image 树、样式与 `sealVisualTrack`。
- `packages/studio/src/parameters.ts:417–495`：Companion 声明 writable 后才可能得到 edit，control 需真实声明。
- `packages/package-loader-node/src/distribution-resolution.ts:92–127`：外部工程 activation 的 `@hypit/` 导入由正在运行的 Distribution 解析，无需工程内绝对路径或 node_modules 链接。
- `bin/hypit.mjs:24–42`：启动时注册 Distribution resolver 与 TypeScript loader。

## 模板合同

1280×720，30 fps，240 帧（8 秒），标题/副标题/主题色/图片路径/入场帧数五个字段。图片限定工程 `assets/` 中的 PNG/JPEG；颜色限定六位十六进制；入场帧数1–60；文字通过 text 元素输出，不拼接 HTML。

模板图片 `assets/default.png` 为本项目通过几何公式生成的占位图。正式素材由 AssetService 按实际解码类型和 SHA-256 导入。导入只登记素材；图片路径绑定才会影响场景。M2 原生面板仍只读，可在 Studio 中编辑已有声明字段，原生写回/撤销属于 M3。

真实运行验证使用脱离上游示例目录的临时工程，并移动目录后重开。证据见 `docs/evidence/m2/`；不截图，不将 compile 或 plan 当成导出成功。

`check` 只接受 `--workspace`，不接受 `--runtime`；`plan` 与 `studio` 同时传 workspace/runtime。已据 CLI help 修正旧文档的过度概括。
