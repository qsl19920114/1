# M12 / FrameLab 1.4 创作效率验证

本轮实现历史查找与完整目标复用、素材查找与可见选择保护、导入逐项结果与未完成项恢复。基于 1091da9；继承 1.3 暗色工作台和当前 Codex 登录方案。

最终门禁：G12 PASS；T048–T051 完成。最后两项质量修复后的完整回归 `ctest-quality-final.log` **36/36 PASS，184.03 秒**。`ctest-final.log` 与 `ctest-release.log` 保留此前两轮通过记录，不代替最终回归。

## 实际验证

- `filter-red.log`：素材搜索入口缺失时的 RED。
- `recovery-api-red.log`：控制器恢复接口缺失时的编译 RED。
- `targeted-first.log`：初次行为回归；素材、恢复和结果面板通过；长中文目标记录被丢弃的 RED。
- `recovery-reset-red.log`：恢复清空信号中关闭工程仍发布旧 projectLoaded 的 RED。
- `ctest-final.log`：完整 CTest；36/36 PASS（183.69 秒，兼容性修复前）。
- `efficiency-e2e.log/json`：实际 Qt、Hypit、FFmpeg 联调；3/3 PASS（26.108 秒）。PNG/实际 MP4/缺失 PNG 的首批为 2/3，创建缺失图片后点击重试为 1/1，成功项未重复；素材筛选仅显示一项，目标复用未产生 generate/approve；实际素材替换、330 帧场景定位、340 帧重开恢复、预览指纹、导出与全片解码通过。
- `experience-run.jsonl`：真实后端与导出层级日志；没有伪造任务进度或模型调用。

## 范围说明

模型适配器未变更，本轮未新增真实模型请求；目标复用行为通过信号断言确认不会自动生成/批准。1.3 的真实 Codex 登录调用证据仍在 m11，本轮不重复计为模型验证。导入恢复只在当前工程/当前应用会话保留；新批次替换旧批次列表；关闭或重开清除。没有截图验收。

## 复审与交付

- `goal-limit-red.log/green.log`：修复 4096 字符复用边界与现有 8192 UTF-8 字节生成契约不一致；ASCII/中文边界通过。
- `restore-reuse-red.log/green.log`：恢复旧任务期间拒绝新目标复用，避免随后重新展示旧审阅；AgentPanel 最终 43/43。
- `escaped-goal-red.log` / `workspace-final.log`：合法目标 JSON 转义导致历史记录丢弃的 RED；最终单条 64 KiB、历史 3 MiB/200 条、全文件 4 MiB 的有界存储 6/6 PASS。
- `product-final.log`：11/11，包括主窗口恢复期间复用按钮保护。
- `spec-review.md`：规格复核 PASS；`quality-initial.json` 保留初审两项发现，`quality-review_summary.md`、`quality-final.json`、`quality-review.html` 和三份分组/交叉复核说明记录最终无剩余确定 P0–P2。
- `package-build.log` 为首包过程；macdeployqt 初次签名诊断由脚本补齐依赖、重新签名后通过。`package-final-build.log`/`package-final.log` 为最后两项修复后的重建/重新部署/严格签名与 ZIP。
- `packaged-startup.json` / `packaged-native.json`：最终 1.4.0 实际搬移中文空格路径、非仓库 CWD/最小初始 PATH 启动 PASS；预览指纹一致，退出后自有 Studio 不存在。
- `release.json` / `delivery-integrity.json`：82 个 Mach-O、248 份许可材料、严格本地签名、ZIP SHA 和可执行文件字节一致性通过。

本地运行入口 `.workbench/启动FrameLab.command` 已指向 1.4.0 与本轮真实验证工程。运行包 `.workbench/release-macos-arm64-1.4.0/`，工程及成片 `.workbench/deliverables-m12/`，旧版本保留。Qt 随包；Hypit/Node/FFmpeg/渲染浏览器/Codex 为外部依赖。本轮仅当前 macOS arm64 本机/ad hoc 签名验收，未公证或验收其他平台。
