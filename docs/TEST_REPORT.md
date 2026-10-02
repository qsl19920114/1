# 1.0.0 测试报告

日期：2026-10-02。实际平台macOS15.7.7 arm64、Qt6.11.2、CMake4.4.3、Node25.8.2、FFmpeg/ffprobe9.0.2，Hypit0.2.10固定commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。

## 已执行的功能验证

| 验证 | 结果 | 证据 |
|---|---|---|
| M6最终完整CTest | 21/21通过，123.03秒 | evidence/m6/ctest-final.log |
| 资源、配置、日志、进程环境、缓存RED/GREEN | 初始缺行为失败，新增行为通过 | evidence/m6/*-red.log、*-green.log |
| 启动器及日志修复定向回归 | 最终3/3组通过，13.53秒 | evidence/m6/quality-infra-green.log |
| 真实完整创作演示 | 实际Studio、六次确认编辑、真实MP4、终态缓存删除、保留输入和成片、重开指纹一致 | evidence/m6/walkthrough-test.log、walkthrough.json |
| 操作视频与样例媒体 | H.264/MP4；成片1280×720、30fps、240帧/8秒；录像全片解码 | evidence/m6/demo-ffprobe.json、film-ffprobe.json |

单测使用进程/HTTP fixture核对失败、未知结果、越权路径、重入等分支；真实演示与历史M2–M5 E2E独立确认后端实际执行，二者不互相替代。首轮质量审查发现官方shell入口误用Node、多实例日志轮转停写及提前关窗误报PASS。三项均保留RED/GREEN回归证据；最终完整回归、真实演示和发布验证使用修复后的代码。

## 发布验证

最终Release重新构建（`build-release-quality.log`），新主程序复制进此前已部署Qt的.app后，使用`package_macos.py --repair-existing`再次检查依赖、签名并归档；该修复模式自身不重建程序。发布结果见 `evidence/m6/release.json`、`link-audit.json`、`relocation.json` 和 `package-final.log`。实际82个Mach-O全部检查通过，minimum版本为14.0/15.0，平台插件、WebEngine helper/权限、ICU/pak/snapshot资源齐全，深度严格签名有效。248份实际归属/许可材料与随包manifest的大小和SHA256一致，最终ZIP校验和见SHA256SUMS.txt。

首次macdeployqt部署实际留下QtDBus安装ID及遗漏框架，并有签名校验错误，保留在 `package-initial.log`。第二轮静态检查通过后，搬移实际暴露共享库的@executable_path依赖无法用于嵌套WebEngine helper，失败日志保留在 `relocated-initial.log/json`。最终统一为@rpath并为每个Mach-O加入相对loader搜索路径，重新签名后搬移的新建/重开与编译预览通过。失败尝试未计入PASS。

搬移验证在中文空格目录和非仓库CWD进行，初始PATH只有系统四目录，去掉Qt/DYLD路径覆盖。应用自己的解析器再加入常用工具位置；默认同级外部Hypit、显式配置及官方shell入口的默认/显式Node两种配置分别测试。显式Node采用basename为custom-node的路径，验证实际指定程序执行。缺Hypit、版本不符及缺Node/ffmpeg/ffprobe均需非零退出并留有可读FAIL报告，结果见relocation.json。四次成功退出后独立检查自有Studio PID已不存在；9种场景中四次真实成功、五次依赖错误符合预期。

## 内容检查及边界

实际导出媒体抽帧核对标题、主题色和图片；入场初始帧为空是模板动画，演示操作实际点击Studio播放后展示已入场内容。录像与媒体规格来自真实工具输出，时间为本机单次观察，不宣称P95或实时性能。

本次发布验证在开发机上搬移执行，静态检查禁止Qt非系统库指向Homebrew路径；未在另一台无Qt机器验证。主平台包本地ad hoc签名，未公证。Windows、真实模型和公开再分发授权/完整源码分发审核未验收；不计入G6课程主平台范围。外部Hypit/Node/媒体/渲染浏览器不随.app部署。

## 质量修复回归

- `shell-launcher-red/green.log`：配置自动解析Node后，官方POSIX shell原先被当成JS而失败；统一启动策略后，Probe/Studio/导出使用一致调用规则。官方固定脚本的默认方式和显式Node方式均实际执行通过；自定义shell不擅自绕过。
- `log-concurrency-red/green.log`：构造时争锁、轮转缺文件窗口、崩溃残留和多个进程追加均覆盖。QLockFile保护初始化及轮转/追加，短时竞争不永久停写；10个QtTest用例通过。
- `early-close-red/green.log`：测试直接运行生产main并提前关闭真实窗口，原先退出0/PASS但预览未就绪；修复后退出9、FAIL、验证标志为false且清理成功。该回归纳入完整CTest。

首轮审查结果保存在 `quality-initial.json/html/md`，最终规格与质量复审另见 `spec-review.md` 和 `quality-review.json/html/md`。

最终规格复审PASS，最终质量复审PASS，首轮三项问题全部关闭、剩余确定P0–P2为0。G6/T029–T032已依据上述实际证据登记，完整任务状态见`tasks.json`。
