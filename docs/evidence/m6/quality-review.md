# M6 最终质量审查

**最终判定：PASS。剩余确定 P0–P2：0。**

审查日期：2026-10-02；基线 `991e620`，分支 `qt-video-workbench`。首轮问题报告保留在 `quality-initial.json/html/md`。本次按 `superpowers:requesting-code-review` 与 `bits-code-guard` 复核三项修复和共享策略，已调用 start.py/finish.py 流程。

## 首轮问题关闭

| 原问题 | 修复与关闭依据 | 现有验证证据 |
| --- | --- | --- |
| P1：自动 Node 执行破坏合法 shell launcher | AppConfig 区分解析出的 nodePath 与 nodeExplicit；RuntimePaths::hypitInvocation 统一决策。JS 使用解析 Node，默认 shell/其他可执行入口直接执行；显式 Node 只对名称、404字节长度及完整SHA匹配的固定官方 wrapper 转入真实目录 bin/hypit.mjs。未知自定义入口明确失败。Probe、Studio、普通导出及缓存清理四处调用均传参数并消费错误。审查者独立核对固定官方 wrapper 的长度与SHA。 | shell-launcher-red/green.log、explicit-node-red/green.log、quality-infra-green.log；真实默认/显式Node官方shell搬移预览 |
| P2：多实例日志轮转使另一实例永久停写 | constructor 的文件探测以及 append 的 rotate/open/write/flush 使用同一路径 QLockFile；QFile 关闭后才释放锁。100ms竞争只记录警告并保留可重试状态；下一次成功追加清空错误。持锁允许缺失当前文件重建，涵盖正常轮转和前一进程崩溃。 | log-concurrency-red.log 旧失败；green 为10/10，覆盖竞争窗口、构造恢复、崩溃后重建及300份多进程JSON记录 |
| P2：启动验证提前关窗返回 PASS | verificationSucceeded 只在真实Snapshot、页面、compiled composition及非空已加载图片通过的成功路径设置。提前正常quit转退出9并保存FAIL原因，清理失败优先退出7；finished后的页面/JS回调不再修改结果。StartupEarlyCloseTest直接包含生产main，通过慢自检与100ms关闭真实窗口验证退出9、FAIL、未完成标记和清理结果。 | early-close-red.log 明确显示旧退出0/PASS；early-close-green.log及最终CTest中的生产回归通过 |

三项均已关闭，未发现新的确定 P0–P2。分组与跨组校验确认新接口、nodeExplicit传播和错误消费一致；没有用清理成功替代预览成功，也没有为显式Node绕过自定义shell入口的副作用。

## 审查覆盖与验证范围

首轮和本次合计覆盖43个本地变更文件、约1562行新增/删除，包含未跟踪 RuntimePaths、发布Python脚本、bundle配置、WalkthroughDemo、单测及新增生产入口回归和用户指南。首轮已完成发布/CMake、C++17/Qt6资源与子进程、缓存路径安全和用户文档审查；本次重点复核修复及直接调用契约，按逻辑、业务语义、安全、并发、健壮性和实际性能风险筛选确定问题。发布脚本Python3.9兼容；生成媒体、下载许可原文及历史证据不作为源码变更统计。

本次读取并核对 `ctest-final.log` 的21/21、123.03秒，`quality-infra-green.log` 的3/3、13.53秒，以及对应RED/GREEN。另核对最终真实walkthrough日志为3/3、44.461秒；九种真实搬移场景、82个Mach-O、248份许可材料、签名及ZIP由独立最终规格审查再次核验，详见 `spec-review.md`。规格报告不替代本质量审查。

本审查只读产品代码、直接契约与已有运行证据，未修改产品、未执行构建/打包/测试或启动Runtime，未发外部评论，未打开报告。首轮及本次start/finish遥测均因网络DNS不可用自动跳过；本地审查、结构化缺陷与HTML报告生成成功。Windows、另一台无Qt机器、公证、真实模型和公开再分发授权审查仍按文档明示边界记录，不计入已验证范围。

G6/tasks登记、提交及推送由主代理在本PASS之后完成。

[HTML报告](quality-review.html) ｜ [结构化缺陷清单](quality-review.json) ｜ [首轮问题](quality-initial.md) ｜ [最终规格审查](spec-review.md)
