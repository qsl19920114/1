# M11 · FrameLab 1.3 创作体验证据

对照用户确认的六项优化，源码范围从 `42cae11` 开始。构建与完整回归、独立规格/质量复审均通过。

| 范围 | 证据 | 已观察结果 |
| --- | --- | --- |
| 构建 | build.log | 全部目标构建成功 |
| 完整回归 | ctest-final.log | 34/34，通过，179.72 秒 |
| 场景、拖放、多选、历史和布局 | product-experience-green.log | 9/9，通过 |
| 多选错误修复 | selection-red.log、product-experience-green.log | 先复现误替换 B，修复后使用实际选中的 A |
| 创作流程 | flow-revise-panel-green.log、flow-quality-verification.md | 28/28；审阅可返回输入并撤销旧批准 |
| 真实 Qt + Hypit | experience-e2e.json、experience-e2e.log | 3/3，25.608 秒；3 场景，定位 330 帧，恢复 340 帧；真实图片/视频批量 2/3 成功，单独报告缺失项；原文件恢复、事务替换、预览核验及导出全片解码 |
| 实际模型 | real-model-probe.json、real-model-probe.log | 当前 Codex 登录，1 条实际公开消息，193 字符，最终结构校验通过；没有图片上传或伪造流式输出 |
| 规格审查 | spec-review.md | 最终 PASS |
| 质量审查 | quality-report.html、quality-review_summary.md | 全量变更及新增文件，37 文件/1198 行，最终无 P0–P2 |

首轮真实联调记录保留在 experience-first.*。flow-quality-initial-final_comments.json 保留审阅返回入口的初审发现。所有素材、工程和生成 MP4 的实际位置见 experience-e2e.json；交付文件位于 `.workbench/deliverables-m11/`。

发布包和搬移启动结果在 package.log、release.json、packaged-native.json、delivery-integrity.json 中单独记录。版本依赖边界不变：Qt 随包，Hypit/Node/FFmpeg/浏览器/Codex 外置；macOS 本机验证，本地签名，未公证。

文本日志仅规整行尾空白，正文、结果和错误保留。发布验收只重跑本轮实际搬移原生启动；未重复未变更的旧版九种配置失败矩阵。
