# M7 真实视频创作与产品交互

用户选择深色创作工作台；本轮新增FrameLab 1.1.0。G7 PASS，T033–T036 done。原M0–M6证据与1.0.0发布物保留。

| 范围 | 证据 | 结果与边界 |
|---|---|---|
| 本地视频发现 | sample-inventory.json | 三份实际MP4哈希一致；8秒540×960/30fps，三个都完整解码成功；界面合并一个示例 |
| 全量回归 | ctest-final.log | 24/24，141.51秒；最后UI/启动修改的相关3套复测见ctest-quality-final.log |
| 视频导入 | importer-final.log | 32项通过；fake process负向/时间戳/取消/路径/不可变复制测试，不冒充真实渲染 |
| 原创模板 | template-final.log | Node 5项通过；生产模板真实视频运行见product-run.json |
| Qt创作 | product-e2e-final.log、product-run.json | 真实生产控制器/UI信号与实际播放/拖动/键盘/滚轮控件；素材登记和自动绑定、原生定位、模式保护、文案编辑、导出、成片播放器和重开通过；26.639秒 |
| 真实成片 | film-verification.json | 实际1280×720/30fps/8秒H.264，无音轨，完整ffmpeg解码退出0；本地路径见product-run.json |
| 规格审查 | spec-initial.md、spec-review.md | 初始缺失样例提示、artifact模式误判修复后源码复审PASS |
| 质量审查 | quality-initial.md/json/html、quality-review.md/json/html | 键盘/滚轮定位P2修复；最后窄范围状态/启动复核PASS，无剩余高置信度P0–P2 |
| 模式/键盘RED | transport-mode-red.log、keyboard-seek-red.log | 初次真实复现失败保留；同症状GREEN在最终Qt产品测试中验证 |
| 停止观察RED | stopped-observation-red.log | 曾阻止关闭/切换；修复后ProductWindow相关回归通过，不发取消Build信号 |
| 运行包 | release.json、link-audit.json、helper-entitlements.txt | 82个Mach-O无外部Qt链接，资源齐全，严格深度签名通过；ad hoc本地签名，未公证 |
| 许可材料 | license-verification.json | 248份输入与manifest哈希一致；新代码没有增加Qt模块或新第三方依赖 |
| 搬移 | relocation.json与relocated-*.json/log | 4种真实成功、5种缺依赖/版本错误正确失败；中文空格路径、最小初始PATH、非仓库CWD，自有Studio退出 |
| 纯视频启动 | packaged-video-startup.json/log | 最终1.1.0包打开此次作品，实际video就绪、imagesReady=false/mediaReady=true，不借图片存在判定视频成功 |

本地交付：`.workbench/deliverables-m7/`包含成片与工程，`.workbench/release-macos-arm64-1.1.0/`包含运行包。媒体与Hypit外部源码不进入Git仓库。测试使用本地media/hyperframes Provider，没有付费请求。用户不需要截图验收；仅生成私有Qt窗口QA帧，证据不把截图当验收要求。

限制：模板固定前8秒、原音静音；输入H.264 MP4/恒定30fps/至少8秒，首240帧真实时间戳从0开始按30fps排列。Windows、另一台干净机器和真实模型未验收。运行包包含Qt，Hypit0.2.10、Node和FFmpeg/ffprobe为外部依赖。

提交前仅对日志/HTML末尾空白做规范化；原始文本副本保存在忽略目录`.workbench/evidence-raw/m7/`。实测JSON、视频哈希和结果不变。
