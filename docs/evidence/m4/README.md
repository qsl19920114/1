# M4 构建与成片验证

复现：仓库根目录执行 `cmake --build build -j4`、`ctest --test-dir build --output-on-failure`、`./build/tests/export_e2e_test`。真实集成使用固定版本 Hypit、本地 Worker 与媒体工具，需要本机监听和执行权限。

导出输入先冻结于工程 `.workbench/builds/<uuid>`。任务记录保存 revision、源码指纹、Build ID、Output 和目的地；关闭只停止观察，恢复不重新提交。最终成功需要精确 Output 获取、ffprobe 参数检查及 ffmpeg 全片解码，随后才原子写入目的地。

测试产物与范围：

- `*-red.log` / `*-green.log`：JSON子进程、工作目录冻结/恢复、媒体校验、控制器状态机的测试先失败及通过证据。
- `build.log` / `ctest-final.log`：完整构建与回归结果。
- `e2e.log` / `e2e.json`：原创工程原生标题/颜色/图片修改，真实计划与构建，停止观察与重开恢复同一个Build，原工程后续编辑不改变冻结输入，MP4交付及真实取消；fixture自己的Author activation语法错误触发真实plan失败，未提交Build、未交付、未发成功信号。
- `ffprobe.json`：实际成片的流与容器参数。
- `ctest-concurrent-build.log`：主代理曾误把最终重建和测试同时启动，测试读到旧二进制，缺少同名文件新回归且取消提示测试失败。该记录不作为通过证据；构建结束后按顺序运行的最终结果另存 `ctest-quality-final.log`。

真实集成的 Worker 仅用于本次冻结目录；测试结束显式停止这些独立 Runtime。应用停止观察时不停止 Worker。模拟接口测试与真实运行日志分别保留，不用模拟结果代替真实门禁，不截图。

实际成片存于本地 `.workbench/deliverables/校园摄影社-验证成片.mp4`（不加入Git）。ffprobe记录H.264、1280×720、30fps、8秒/240帧；生产MediaValidation完成全片解码。人工抽查首帧/第120帧/末帧：首帧符合入场透明动画，中间与结尾中文标题可读、橙色主题与导入图片存在，无布局裁切。抽帧仅检查媒体内容，不作为Qt截图验收。
