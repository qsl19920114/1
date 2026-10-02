# M5 受约束提案验证

范围为单项提案Schema、可替换Provider、原生核对/确认、同一EditorController执行与历史。默认Provider为本地规则演示（明确标“模拟”），外部JSON来源未核验；本阶段不宣称接入或验证真实模型。

单元测试红绿证据与完整回归分别记录。`proposal_e2e_test`使用真实Studio和生产控制器，核对确认前源文件/指纹不变、确认标题与图片后回读及撤销、人工编辑使旧提案失效、关闭清理。模拟提案生成与真实后端写入是不同验证内容。

复现：构建后执行 `ctest --test-dir build --output-on-failure` 与 `./build/tests/proposal_e2e_test`。真实集成需固定Hypit及本机监听权限，不截图。最终结论以实际日志及tasks.json为准。
