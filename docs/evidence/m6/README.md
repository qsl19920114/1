# M6 证据索引

本轮无截图门禁。G6已登记PASS；最终规格与质量复审均PASS，首轮三项质量问题全部关闭，见spec-review.md及quality-review.md/json/html。

| 内容 | 证据与结果 |
|---|---|
| 最终构建/回归 | build-debug.log、ctest-final.log：21/21 PASS、123.03秒 |
| 基础设施RED/GREEN | infrastructure、process-environment、explicit-node、export-cache各*-red/green日志；质量修复后3组13.53秒通过（quality-infra-green.log） |
| 真实完整闭环 | walkthrough-test.log：3/3 PASS、44.461秒；walkthrough.json：6次编辑、导出、终态清理、重开及Studio退出 |
| 媒体 | demo-ffprobe.json：42.4秒/212帧/5fps；film-ffprobe.json：8秒/240帧/30fps、1280×720、H.264；均全片解码 |
| Qt部署与签名 | build-release-quality.log（重建）及package-final.log（复制新主程序后重验/签名/归档）、link-audit.json（82个Mach-O）、helper-entitlements.txt；可执行库部署minimum14.0/15.0 |
| 实际许可材料 | license-collection.log、license-materials.json：248份，126个Qt官方归属页；包内大小/SHA256核对通过 |
| 最终搬移 | relocation-run.log、relocation.json：9种场景符合预期；四种真实成功与五种明确失败 |
| 默认配置创建 | relocated-default.json/log/jsonl：bundle模板、Snapshot、composition、图片、退出清理PASS |
| 显式配置重开 | relocated-explicit.json/log/jsonl：custom-node实际执行、相同源码指纹、预览及清理PASS |
| 官方shell启动器 | relocated-shell、relocated-shell-explicit JSON/log：默认Node直启和显式custom-node均真实预览成功 |
| 三项质量修复 | shell-launcher、log-concurrency、early-close各RED/GREEN日志；首轮问题保存在quality-initial报告 |
| 错误依赖 | missing-hypit、wrong-version、missing-node、missing-ffmpeg、missing-ffprobe JSON/log：各自FAIL是预期结果 |
| 发布信息 | release.json、SHA256SUMS.txt；实际.app/ZIP在.workbench/release-macos-arm64 |

首轮macdeployqt留下安装ID与漏框架/签名问题，见package-initial.log。依赖闭包修复在旧失败产物验证（deployment-closure-test.log，补3个Qt框架、82个Mach-O）；随后的helper实际加载失败见relocated-initial.json/log。最终统一共享库@rpath并重签名后真实预览通过；失败尝试没有用于PASS。

搬移地点是临时中文空格目录，CWD非仓库，初始PATH为`/usr/bin:/bin:/usr/sbin:/sbin`并去掉Qt/DYLD覆盖。准备好的外部Hypit通过同级或runtime/hypit链接提供，**不随.app/ZIP分发**。独立检查成功运行的Studio PID已退出；验证发生在本开发机，另一台无Qt机器未验证。

录像连续采集自己的真实Qt窗口，通过生产MainWindow信号驱动及实际Studio transport播放；媒体成片来自真实Hypit输出。二者与自动断言分开。提案生成仅本地模拟，真实模型未配置。早期M0–M5证据和失败日志保留为历史。

入库日志和HTML报告仅去除行末空白；有变化的原始输出保留在本地`.workbench/evidence-raw/m6/`。断言、退出码和报告内容未改写。
