# M6 规格审查

审查基线：`991e620`。范围：未提交的 M6 代码、`docs/plans/2026-10-02-m6-release.md`、`tasks.json` 中 T029–T032，以及 PROJECT_PLAN 的 G6 和交付边界。

## 最终判定：PASS

审查日期：2026-10-02。只读复核生产实现、最终文档、实际发布产物及运行证据；未并发构建或启动真实 Runtime。T029–T032 的实现与验收材料符合当前 M6 计划，没有剩余确定的规格遗漏。G6/tasks 登记、提交及推送由主代理在后续质量审查通过后完成，不把待登记状态当作实现失败。

| 要求 | 代码/材料位置 | 当前结果 |
|---|---|---|
| T029 配置有界、日志轮转及界面日志有界 | AppConfig、LogWriter、MainWindow；RuntimePathsTest、LogRotationTest | 已核对实现及 `ctest-final.log` 的 20/20 回归 |
| T029 只清理自有终态缓存，活动/未知任务及输入保留 | ExportController 的 status/activity/runtime-down 链、ExportWorkspace 的持久记录/UUID/冻结输入及无链接目录核对；ExportCacheTest | 已核对负向覆盖及最终 `walkthrough.json` 的真实终态缓存删除、输入/成片保留 |
| T030 bundle 相对资源及子进程工具解析 | RuntimePaths、AppConfig、CMake、main、各 QProcess 适配器 | 已见实现及显式 Node 调用补丁；对应回归通过 |
| T030 Qt/WebEngine 链接、资源、签名、许可、ZIP/SHA256 | package_macos.py、collect_licenses.py、THIRD_PARTY；package-final.log、link-audit.json、helper-entitlements.txt、license-materials.json、release.json | 实际 82 个 Mach-O、资源与深度严格签名通过；248 份许可材料及最终 ZIP hash 经审查者再次核对 |
| T031 真实 Snapshot/编译预览/图片/退出清理 | main 的 --verify-startup/--create-project/report-out；relocated-default/explicit.json | 两处要求图片非空；修复后的两次真实启动均 Snapshot/page/composition/images/cleanup 为 true，成功 PID 独立确认已退出 |
| T031 中文空格路径、非仓库 CWD、最小初始 PATH、默认/显式配置及缺依赖 | relocation.json、七组各自 JSON/log | 两种成功、五种依赖错误符合预期；默认 bundle 模板新建，显式 custom-node 重开同一源码指纹；外部 Hypit 通过声明的链接布局提供 |
| T032 完整操作视频及真实成片 | WalkthroughDemo 连续抓取自己的真实 MainWindow；生产信号驱动创建/导入/编辑/撤销/重做/提案确认/导出/清理/重开 | 最终测试 3/3、47.811 秒；JSON/ffprobe 为 229 帧、45.8 秒演示，8 秒/240 帧/1280×720/H.264 成片；重开源码指纹一致 |
| T032 指南、架构、测试、版本、原创与复用边界 | README、STATUS、ARCHITECTURE、TEST_PLAN、USER_GUIDE、THIRD_PARTY、DEMO_GUIDE、TEST_REPORT、evidence/README | 文档描述与实际证据、1.0.0 资源布局及验收边界一致；随包 USER_GUIDE/THIRD_PARTY 与源码版本字节一致 |

## 已修复的确定遗漏

1. 原 `main.cpp` 图片就绪表达式以及 `WalkthroughDemo::previewReady` 对 `querySelectorAll('img')` 结果仅做 `every`，空集合也返回 true。只读复核已确认两处补入 `imgs.length > 0`，并保留 `complete && naturalWidth > 0`；最终真实演示与搬移验证覆盖修复后的路径，遗漏已关闭。

## 审查者独立核对

- 重新计算实际 ZIP 的 SHA256，为 `307ad7fe1ed1e5b32549e25fcd962ac451b5718c80db29b67756438f96721585`，与 release.json 和 SHA256SUMS.txt 一致。
- 逐一读取实际包内 248 份许可材料，大小与 SHA256 全部匹配 license-materials.json；两个随包指南与源码文档字节一致。
- 对实际最终 .app 再执行 `codesign --verify --deep --strict --verbose=2`，退出 0，报告 valid on disk / satisfies its Designated Requirement。
- 对实际 82 个 Mach-O 再执行发布脚本只读 `audit(app)`：结果与已保存 link-audit.json 完全一致，minimum targets 为 14.0/15.0，无非系统绝对库依赖或外部 RPATH。
- 逐一读取七种搬移场景报告：两个 PASS 为退出 0，真实 Snapshot、编译预览、已加载图片及退出清理均通过；五个 FAIL 为明确非零并保留错误原因。
- 已核对完整 CTest 日志为 20/20、112.37 秒；最终 walkthrough-test.log 为 3/3、47.811 秒；最新媒体参数为演示 45.8 秒/229 帧/5fps、成片 8 秒/240 帧/30fps。此审查未重新运行这些功能测试。

首轮 package 与搬移的真实失败日志保留；QtDBus 安装 ID、缺失框架和嵌套 helper 共享库加载问题已由最终闭包补齐、相对链接及重签名修复，旧失败结果未混入最终 PASS。

## 边界核对

默认模型明确为本地模拟，真实模型没有配置；没有宣称 Windows、公证或另一台干净机器已验收。Hypit、Node、FFmpeg/ffprobe 和 Runtime 所选渲染浏览器明确为外部依赖。演示描述为程序驱动生产 UI 信号的连续真实窗口录制，未声称手工鼠标录制。许可材料不宣称已完成公开再分发授权审查。
