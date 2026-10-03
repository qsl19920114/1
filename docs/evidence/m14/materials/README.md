# T058 人物素材准备证据

这一步只准备三张既有静态图片，供本项目本地模板绑定替换。尚不证明 Qt 审批链、视频导出或录屏验收；这些由人物演示另行记录。未执行上游 swap-host、任何生成 Provider 或视频编码。

运行：

```sh
python3 scripts/showcase/prepare_character_materials.py
python3 docs/evidence/m14/materials/verify_materials.py
```

前提：本机已经存在 `.workbench/showcase/downloads/media.tar.gz`、固定版本 Hypit 检出及 FFmpeg。脚本不联网下载。默认输出 `.workbench/character-showcase/materials/`（Git 忽略），图片保持归档原始字节。

| 输出文件 | 归档素材 | 实际尺寸 | 人工检查 |
| --- | --- | --- | --- |
| `portrait-baseline.png` | `assets/presenter-a464d354ce0d.png` | 1152 × 2048 | 白色水手帽、白色服装、长发、话筒，单人面部清晰 |
| `portrait-variant-1.png` | `assets/final-half/host.png` | 1152 × 2048 | 黑色尖钉帽、刘海、黑色服装、话筒，与前者可明显区分 |
| `portrait-variant-2.png` | `assets/final-half/drinking-illustration.png` | 1024 × 1536 | 短发像素风虚构人物、珊瑚色衬衫、粉色背景 |

归档成员均以 `./productions/explainer/` 为前缀。已用 `view_image` 打开原始三图检查；另检查了 `montage/01.png` 至 `04.png`，它们是多人、多来源拼贴，因此没有入选。

## 来源和归属

[provenance.json](provenance.json) 记录官方归档 URL、归档及图片 SHA-256、成员路径、输出路径、尺寸、来源证据文件 hash 和固定 commit。`catalog.json` 的 `samples[].path` 相对目录可直接用于展示素材列表。

固定 Hypit commit：`1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`，版本 `0.2.10`。证据来自本机固定源码：

- `examples/complex-explainer/README.md:9-12` 给出官方完整媒体归档 URL；`:102-106` 说明素材类别及归属。
- `examples/complex-explainer/productions/explainer/ASSET-PROVENANCE.md:6` 明确生产主持人图片及声音由 AI 生成，归属上游项目提供或委托的素材。白帽图在 `authors/assets.svml:29` 绑定为 `presenter-reference`。
- 黑帽图在同一 `authors/assets.svml:63` 绑定为 `ranking-host`。公开素材说明没有单独给出此图的生成凭据；保留官方提供素材归属，不声称已独立确认它的生成历史。
- `ASSET-PROVENANCE.md:7-15` 明确第三张图是新生成的虚构人物插画，生成日期为 2026-09-14。
- `ASSET-PROVENANCE.md:9` 明确网站抓图、产品标记及其他参考样例保留实际来源，不能把整个归档统称 AI 生成。本项目没有获取额外权利声明，也不把上游素材署为本项目原创。

## 上游 swap-host 的外部依赖

`examples/ranking-football/swap-host.svrun:4-5` 选择 `swap-host.svml` 并输出 `final.video`。对应源码 `:8,39-40` 导入 `@hypit/gpt-image@1`、声明 `gpt:Image`；`:9,133-144` 导入 `@hypit/seedance@1`、声明 `seedance:ReferenceVideo`。运行配置 `examples/ranking-football/hypit.runtime.json:11-20` 包含 `@hypit/provider-hypihub`、`https://hypit.ai` 及 platform credential `hypihub.oauth`。

所以原版生成工作流需要外部服务与凭据。本地演示复用以上静态图片替换模板的图片绑定；不证明已重生成人物讲话视频、换声音或完成视频换脸。脚本校验上述源码等于固定 Git commit 中的字节，既不执行这些声明，也不修改上游目录。

## 本次验证

- [test-red.log](test-red.log)：脚本缺失时，两个行为测试真实失败。
- [test-green.log](test-green.log)：真实归档测试覆盖字节保留、路径穿越、绝对路径、符号链接、硬链接、目录冒充、重复和缺失成员、大小超限；输出路径测试覆盖符号链接和上游目录保护，2 个测试通过。
- [preparation.log](preparation.log)：三张原图的 hash、尺寸与完整 FFmpeg 解码均通过。FFmpeg 仅解码到 null sink。
- [reproducibility.log](reproducibility.log)：再次运行后，三张 PNG、两份本地 JSON 及仓库证据 JSON 均逐字节一致。

解包使用显式成员白名单及有界读取，不调用 `extract` / `extractall`；拒绝归档成员路径穿越和被选中的非普通文件。压缩归档限制 600 MiB、累计声明展开大小限制 2 GiB、单图限制 20 MiB、成员数限制 20000。三张图片 hash 固定为本次实际人工检查的输入，后续归档若改变将失败，不会静默改用未经检查素材。
