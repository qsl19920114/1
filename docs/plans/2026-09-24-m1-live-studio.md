# M1：连接真实 Studio 的实施与验收

范围：延续 tasks.json 的 T009 → T010 → T011，达到 G1。以当前特性分支及已有未提交基础设施代码为起点，不更改 Hypit 或其他 Qt 作业项目。

- [x] T009：修复相对路径基准；完善 JSONL 日志（命令参数数组和退出码）；异步自检固定 Hypit 版本；可读错误及可选择配置文件。
- [x] T010：独立 StudioProcess 通过 QProcess 启动；带齐 run/workspace/runtime；解析 stdout 的实际 Local 地址；限时启动和仅清理自己创建的进程。
- [x] T011：StudioClient 异步 GET 原始 Snapshot；防止旧请求覆盖新工程；Qt 打开/关闭工程与刷新；组件选择和属性只读展示；嵌入完整 Studio，明确原生写回仍属 M3。
- [x] 验证：配置/日志/版本检查、HTTP 错误/取消/非法 JSON、端口回退/进程停止的 QtTest；真实本地 runtime（仅 media.local 和 hyperframes.local）端到端启动、会话、截图与退出清理；保留另一进程证明不误杀。
- [x] 更新 STATUS.md、tasks.json、README 和证据。只在真实验收通过后提升 G1。

接口依据：docs/API_CONTRACT.md；Hypit 0.2.10 packages/studio/start.ts:149–173（Vite 启动与 stdout 地址），studio-adapter/src/index.ts:191–226（属性声明）。
