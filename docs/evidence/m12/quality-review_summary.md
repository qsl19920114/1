# M12 代码质量复审

最终 PASS：当前变更未发现剩余确定 P0–P2 缺陷。

使用 bits-code-guard 分组审查历史/目标复用与素材/导入恢复，并独立核对共享 MainWindow、main、接口和 CMake。含未跟踪新文件与测试，比较基线 1091da9。初审两项发现保存在 quality-initial.json。

- P1 已关闭：恢复旧任务期间禁止目标复用，避免异步恢复重新显示旧审阅；AgentPanel 与主窗口两层保护，RED/GREEN 及产品窗口行为测试通过。
- P2 已关闭：合法目标 JSON 转义后超过 16 KiB 而丢失；改为单条 64 KiB、历史总量 3 MiB 与最多 200 条、文件 4 MiB；引号/控制字符目标与淘汰后存取一致性通过。
- 规格兼容性 P2 同时关闭：复用目标限制与生成器统一为 8192 UTF-8 字节，5000/8192 ASCII 和中文边界通过。

独立审查者只读源码并核对断言；实际构建与运行由主代理执行，证据为 goal-limit-green.log、restore-reuse-green.log、workspace-final.log、product-final.log、ctest-quality-final.log。未把模拟测试计作真实模型请求。
