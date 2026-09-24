# M1 真实连接证据（T009–T011）

- `build.log`：CMake 编译输出。
- `ctest.log`：配置/JSONL、版本自检、Studio 进程、HTTP 会话、五类控件映射测试。
- `cli-cases.json`：离线会话两种参数形式及错误退出码回归。
- `e2e.json`：真实 Studio 的端口回退、会话数据、编译预览、所属进程组退出和无关进程保留断言。
- `live-run.log`：完整真实启动、自检、实际 URL、退出日志。
- `workbench.jsonl`：应用记录的结构化事件、命令参数数组和退出码。
- `session.json`：通过 Qt QNetworkAccessManager 实际 GET 返回的原始 JSON。
- `live-studio.png`：Qt 运行窗口的真实截图；中央嵌入完整 Studio，右侧为 Qt 原生属性只读控件。
- `offline.log` / `offline.png`：原有离线查看模式的回归样本。

复现：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 tests/integration/cli_cases.py
python3 tests/integration/live_studio.py
```

真实测试需要原生窗口与本机监听权限；受限沙箱不能代替 GUI 验收环境。运行 fixture 位于 `.workbench/真实 工程-*`（含中文与空格），通过既有 writable-probe Companion 暴露上游已有的两项参数。只启用本地媒体与 Hyperframes Provider。未修改 Hypit 检出，也没有进行付费调用。

成功范围为 M1/G1：打开与只读连接。原生写回、素材导入、自建模板和导出编排仍属后续里程碑。已验证的平台为 macOS/Qt 6.11.2；Windows 子进程树清理不在本轮结论内。
