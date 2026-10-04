# 原创图片模板与上游契约

版本：Hypit 0.2.10，commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。

本项目 `templates/title-card/`、`templates/story-reel/` 的布局、文案、几何占位图、Surface、Producer 与 Companion 为本项目实现。复用 Hypit 的编译、资源解析、字体、视觉轨道与渲染器；不复制上游包到工程。

## 源码依据（相对于 Hypit 检出）

- `examples/semantic-composition/packages/chat-scene/src/activation.ts`：Structured Surface 的 window projection、fragment input、render producer 与 activation 结构。
- `packages/media/src/surface.ts:84–118`：`resolveAsset({from,mediaType,range})` 返回 artifact，记录类型为 `artifactTypes.blob`。
- `packages/media-track/src/lower.ts:187–195`、`packages/composition/src/track.ts:335–342`：图片元素用 `kind: image` 和 BlobRef artifact。
- `packages/interview-emoji-reveal/src/program.ts:201–243`：Visual IR 的 box/text/image 树、样式与 `sealVisualTrack`。
- `packages/studio/src/parameters.ts:417–495`：Companion 声明 writable 后才可能得到 edit，control 需真实声明。
- `packages/package-loader-node/src/distribution-resolution.ts:92–127`：外部工程 activation 的 `@hypit/` 导入由正在运行的 Distribution 解析，无需工程内绝对路径或 node_modules 链接。
- `bin/hypit.mjs:24–42`：启动时注册 Distribution resolver 与 TypeScript loader。

## 模板合同

title-card 为1280×720，30 fps，240 帧（8 秒），原有标题/副标题/主题色/图片路径/入场帧数五个字段。story-reel 为720×1280，30 fps，450 帧（15 秒），每个场景包含标题/副标题/主题色/图片路径/标题字号。图片限定工程 `assets/` 中的 PNG/JPEG；颜色限定六位十六进制；入场帧数1–60；文字通过 text 元素输出，不拼接 HTML。

模板图片 `assets/default.png` 为本项目通过几何公式生成的占位图。正式素材由 AssetService 按实际解码类型和 SHA-256 导入。导入只登记素材；图片路径绑定才会影响场景。M3 的“应用到当前组件”查找真实 `binding == image` 字段，经统一编辑控制器写入，可撤销/重做。

真实运行验证使用脱离上游示例目录的临时工程，并移动目录后重开。证据见 `docs/evidence/m2/`；不截图，不将 compile 或 plan 当成导出成功。

`check` 只接受 `--workspace`，不接受 `--runtime`；`plan` 与 `studio` 同时传 workspace/runtime。已据 CLI help 修正旧文档的过度概括。

## 1.1.0 图片构图参数（M15 / T061）

两套模板及各自 Node Author Package 的版本为1.1.0；Hypit module version 仍为 `1`，SVML 导入保持 `@qvw/title-card@1`、`@qvw/story-reel@1`。原有工程保留自己复制的包，不自动升级。新包接受没有新增属性的旧源文件，渲染保持居中 cover。

| SVML literal / Inspector binding | Producer options | 缺省值 | 接受值与作用 |
| --- | --- | --- | --- |
| `image-fit` | `imageFit` | `cover` | `cover`（铺满裁切）或 `contain`（完整显示）；写入图片 Visual IR 的 `object-fit` |
| `image-position-x` | `imagePositionX` | `50` | 0–100 的有限十进制数，图片水平方向百分比 |
| `image-position-y` | `imagePositionY` | `50` | 0–100 的有限十进制数，图片垂直方向百分比 |

位置组合为 `object-position: <x>% <y>%`。0/50/100 分别对应左/中/右或上/中/下；采用 CSS 图片定位语义，只有图片与容器存在尺寸差异的轴会出现视觉位移。完整显示可能留空，铺满可能裁切，图片容器尺寸不变。

Surface vocabulary 将三个属性声明为可选 literal。解析器只在缺失时使用默认值；空串、引用、NaN/Infinity、超范围数、非十进制输入以及 CSS 注入文本均拒绝。直接调用 Producer 使用的渲染函数同样验证 fit 枚举及 number 类型、有限性和范围，禁止靠字符串拼接混入 CSS。

新模板的每个 Card 显式写入 `image-fit="cover" image-position-x="50" image-position-y="50"`。Companion 将三个 binding 声明为 writable；图片适配使用 select，选项为 `{value:'cover',label:'铺满裁切'}`、`{value:'contain',label:'完整显示'}`；位置使用 number，`unit:'%'`、`number:{minimum:0,maximum:100,step:1}`。数字存储为0–100，未使用 scale。旧源文件未写入这些属性时，仅保证默认编译与渲染；不承诺其 Inspector 自动新增可写字段。

固定上游源码依据（均相对于配置中的 Hypit 检出，commit 同上）：

- `packages/markup/src/element.ts:28–34`：`textAttribute` 仅对缺失值使用 fallback，拒绝非文本及空文本。
- `packages/studio-adapter/src/index.ts:129–144`：带 value/label 的 select options，以及 number minimum/maximum/step 的真实结构；`:190–205` 声明 Inspector control/options/unit/number。
- `packages/studio/src/parameters.ts:405–425`：真实源属性范围及 writable literal 决定绑定可写性；`:480–495` 从绑定生成 Inspector value 与 edit，保留 Companion options。
- `packages/visual-ir/src/style.ts:107–108`：Visual IR 支持 object-fit/object-position；`:167` 接受 cover/contain；`:184–221` 校验样式名称和值。

验证证据：

- `docs/evidence/m15/framing-red.log`：实现前两个模板的10项新增测试失败，原有4项通过。
- `docs/evidence/m15/framing-green.log`：16项 Node 测试通过，包含默认兼容、cover/contain、边界/小数焦点、非法值拒绝、真实 Companion 结构和固定 Hypit Visual IR 样式校验器。
- `docs/evidence/m15/framing-hypit-check.log`：复制到临时工程的两套模板分别完成 fresh/legacy/contain-decimal 的真实 `hypit check`（exit0）；101 越界分别被拒绝（exit1）。使用仅含本地 media/hyperframes endpoint 的 Runtime 启动 Studio，真实 `GET /__studio/session` 均 HTTP200，三个字段的 control/options/value/edit 均核对通过；两个自启 Studio 已停止。

上述证据证明源文件编译、Visual IR 样式契约与真实 Inspector 可写契约；Qt 编辑、实际画面差异及导出/解码由 T062 独立验证。

### Agent 故事工程生成器的版本兼容

`StoryTemplate::prepare` 只接受已知的 story-reel 1.0.0/1.1.0，不放宽为任意主版本1。1.1.0 的三个生成场景均显式包含 cover/50/50 构图属性，因此 Agent 创建工程后也能取得真实可写字段。1.0.0 的旧包不识别新增属性，生成器保留旧属性集合，避免升级工作台后破坏旧模板的编译。

质量审查发现生成器原先硬编码1.0.0，导致安装模板升为1.1.0后 Agent 创建在编译前即失败。`framing-story-red.log` 记录现有创建流程及新增版本测试的真实失败；修复后 `framing-story-green.log` 记录 story_template、agent_controller 两个 CTest 目标通过。新增测试检查所有三个 Card 的显式默认属性、1.0/1.1兼容，以及未知小版本/主版本/缺失版本的拒绝。

`framing-story-generated-hypit.log` 记录临时 C++ 驱动链接实际 qvw_agent 并调用生产 `StoryTemplate::prepare`，生成源文件经真实 Hypit check exit0；随后真实 Studio HTTP200，全部三个场景的九个构图字段均有正确值、control 和 edit。仅使用本地 runtime，无模型调用；自启 Studio 已停止。
