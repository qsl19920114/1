# 演示与提交材料

## 最新交付：1.5 Qt × Agent 实验

- `.workbench/deliverables-m13/index.html`：本地实验报告与7个可播放视频。
- `.workbench/deliverables-m13/experiment-report.docx`：可编辑 Word 报告；姓名、学号自行填写。
- `videos/qt-agent-walkthrough.mp4`：实际 Qt 窗口连续录像，最终2次真实模型请求、审阅/批准、素材与标题修改及导出。
- `videos/qt-agent-film.mp4`：本次工程生成的8秒静音成片。其余5个视频为明确标注来源的Hypit示例。
- `.workbench/启动FrameLab.command`：启动1.5应用并打开演示工程。

录像由 `tests/integration/AgentWorkbenchDemo.cpp` 驱动正式控件/信号，复用生产桥接和真实Codex/Hypit；录制保留模型等待时间。不是手工鼠标演示，不代表模型理解了视频画面。报告中的图取自真实录像，仅辅助讲解。

复现：先按使用指南配置本地依赖与当前Codex登录，执行 `python3 scripts/showcase/prepare_hypit_samples.py` 准备官方示例，然后运行 `build/tests/agent_workbench_demo`，最后运行 `python3 scripts/showcase/build_experiment_report.py --delivery .workbench/deliverables-m13`。报告生成器仅接受通过的真实演示；官方视频来源/哈希与解码证据在 `source-manifest.json`。保持整个交付文件夹一起移动，HTML与Word的视频链接使用相对路径。

以下保留历史1.0演示说明；旧模拟提案不等同于当前真实Agent入口。

## 历史本地交付目录

`.workbench/deliverables/完整操作演示.mp4` 是连续录制本项目真实 Qt 窗口的操作视频；`.workbench/deliverables/校园光影-演示成片.mp4` 是这次工程经 Hypit build/get 和媒体校验得到的8秒视频。二者用途不同。工程位置记录在 `docs/evidence/m6/walkthrough.json`，可直接打开其中的 `workbench.qvw.json` 继续编辑。

录制由 `tests/integration/WalkthroughDemo.cpp` 自动驱动生产 MainWindow 信号及真实 Controller，不是假后端、网页截图拼出的成片，也不声称是手工鼠标操作录像。帧来自本应用窗口，不采集桌面；每200ms采集一次，5fps编码为H.264。阶段时间、帧数和实际结果在JSON中，完整录像经全片解码。没有用录像或截图替代功能断言。

## 视频顺序

1. 环境诊断，从原创模板新建“校园光影”工程，读取真实Studio会话。
2. 播放预览到入场动画之后，导入本地原创绘制的PNG素材。
3. 使用原生编辑链修改标题、主题色和图片slot；回读确认。
4. 撤销图片绑定，再重做。
5. 生成带“本地模拟”标识的标题提案，确认前工程不变。
6. 显式确认，真实写回同一工程并保存。
7. 冻结输入、计划、构建、精确获取final.video；参数及全片解码通过才交付MP4。
8. 确认终态及自有Worker停止后清理冻结缓存，工程输入和成片保留。
9. 关闭Studio并重开，源码指纹和标题一致，图片加载完成。

视频中的Studio名称和上游标识保留。提案生成是受限规则模拟，真实模型未配置；提案的执行、预览及导出均使用真实本地Hypit。

## 复现

准备使用指南中的外部依赖后：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build --parallel 4
build/tests/walkthrough_demo
```

每次创建新的带时间戳演示工程，最终操作视频和样例成片写入上述固定交付位置。临时录制帧自动清理；不删除既有工程。

建议课程提交包含源码仓库、运行包ZIP与SHA256、操作视频、样例成片、使用/架构/测试说明和第三方依赖说明。应用为macOS arm64本地签名包；外部Hypit等依赖准备方法见 `USER_GUIDE.md`。最终证据索引见 `evidence/m6/README.md`。

## M7 视频创作演示

运行FrameLab1.1.0，左侧「示例」播放本地测试视频，再「用此视频创作…」。编辑标题/副标题，使用原生播放、逐帧、定位条（支持键盘和滚轮），导出完成后在Qt内播放成片。当前示例来源三个相同副本已合并，模板取前8秒静音；自己的视频也可经校验导入后应用到视频组件。

复现生产自动化流程：

```bash
./build/tests/product_e2e_test -o docs/evidence/m7/product-e2e-final.log,txt
python3 scripts/release/verify_relocated.py \
  --app .workbench/release-macos-arm64-1.1.0/QtVideoWorkbench.app \
  --evidence-dir docs/evidence/m7
```

Qt测试驱动生产UI信号及真实播放定位控件，不是模拟Hypit回应；`product-run.json`记录可重开工程与真实成片的位置。M6历史完整操作视频仍保留，M7新增实际视频作品与新运行包；本轮无需截图验收。
