# M13 / FrameLab 1.5 证据索引

## 功能与真实执行

- `handoff-red.log` / `handoff-green.log`：素材交接权限、精确选择、只填目标与范围、旧批准撤销及面板保护。
- `build.log` / `ctest-first.log`：正式代码完整构建及36/36 CTest（184.52秒）。
- `configure-version.log` / `build-version.log` / `version-startup-test.log`：1.5版本重配、完整构建及启动关闭回归。
- `agent-workbench-demo.log` / `.json`：最终真实演示3/3，86.581秒；2次当前Codex请求、2次批准、工程源码批准前不变、素材与标题写回、预览一致、导出/获取/全片解码、Studio清理。
- `agent-workbench-initial.log` / `.json` / `recording-correction.json`：首次功能通过但录制Retina尺寸有误，保留失败观察并修正重录；不作为交付录像。

## 官方示例与报告

- `catalog.json` / `preparation-manifest.json`：4个不同官方视频的固定来源、哈希、尺寸、探测与全片解码。原有3个文件相同SHA，合并成1个样例。上游源码未修改。
- `report-decode-red.log` / `report-decode-green.log`：审查发现演示媒体未用SHA绑定旧解码证据，修复为交付时实际完整解码。
- `report-delivery.json`：报告生成及本地媒体/Word结构/HTML相对链接验证。
- `spec-review.md` / `spec-final.md`：实现与最终交付规格审查。
- `quality-review.md` / `quality-review.html` / `quality-review-summary.md`：质量审查和发现关闭情况。

## 发布

- `package-build.log` / `release.json`：1.5.0重新构建打包。
- `packaged-native.json` / `packaged-startup.json` / `packaged-native.log`：实际搬移中文空格目录、非仓库CWD、最小初始PATH启动，视频就绪、同预览指纹和进程清理。
- `delivery-integrity.json` / `integrity.log`：82个Mach-O、248份许可材料、深度严格本地签名、ZIP SHA/程序字节核对。

报告和媒体未提交到Git，保存在 `.workbench/deliverables-m13/`。示例原素材与新成片分别标明；视频像素没有发送给模型。录制由脚本驱动真实窗口，不是人工鼠标录制。视频采集按实际时间，无加速等待。仅本机macOS arm64验证；跨机、Windows、公证和外部依赖自动安装不在本轮验收范围。
