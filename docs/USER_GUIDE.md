# Qt 视频工作台使用指南

## 平台与依赖

本次主平台为 macOS arm64，最低构建目标15.0；实际验收机15.7.7。应用包自带Qt6.11.2和WebEngine，模板、配置示例及第三方说明位于Contents/Resources。包采用本地ad hoc签名，未公证；Windows和另一台无Qt机器未验收。

视频执行使用外部Hypit0.2.10（固定commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`）、Node>=22.15、FFmpeg/ffprobe和Runtime选择的渲染浏览器。这些不在.app或ZIP内，首次安装应按固定Hypit版本的上游安装说明准备其依赖、pnpm10.33.0及本地hyperframes浏览器。开发机实际Node25.8.2、FFmpeg9.0.2；没有修改全局shell配置。

将准备好的Hypit Distribution放在.app同级的`hypit`目录（含bin/hypit.mjs及其依赖），或用“选择配置…”指定自己的版本锁。发布默认配置为Resources/config/version-lock.json；distributionPath按该config目录上一级(Resources)解析，`../../../hypit`就是.app的同级目录。显式配置的相对路径仍按其config目录上一级解析。

可选工具配置：

```json
{
  "hypit": {"version":"0.2.10","distributionPath":"runtime/hypit","launcher":"bin/hypit.mjs"},
  "tools": {"node":"/absolute/path/node","ffmpeg":"/absolute/path/ffmpeg","ffprobe":"/absolute/path/ffprobe"}
}
```

省略tools时，从本应用子进程PATH及macOS常用安装位置解析。仅影响应用进程，不改用户PATH。缺依赖或版本不符会在日志中显示原因。Qt运行库无需另行安装。

`hypit.launcher`可使用JS入口`bin/hypit.mjs`，也可使用固定版本官方shell入口`hypit`。JS入口由解析到的Node执行；shell入口默认直接执行并继承应用PATH。若官方shell同时配置了`tools.node`，应用核对固定脚本内容后用指定Node执行其相邻JS入口。自定义shell与显式Node组合无法保证实际Node选择时会给出错误，可改为JS入口。

`tools.ffmpeg`/`tools.ffprobe`指定工作台成片校验所用程序。Hypit Provider 的渲染/媒体程序由工程Runtime的endpoints config中的`ffmpegPath`/`ffprobePath`选择，缺省在子进程PATH中查找；需要固定其他媒体工具链时同时配置Runtime。这两层配置分别记录各自实际调用。

## 完整创作流程

1. 新建工程，选择名称和目录。工程包含本项目原创1280×720、30fps、8秒图片标题卡、源码与运行配置。
2. 导入PNG/JPEG；按内容解码、hash命名并复制到工程，原图不改。选择素材和组件，再点“应用到选中组件的图片”。导入与绑定是两个动作。
3. 右侧原生属性修改标题、主题色、副标题或入场帧数。编辑串行执行，后端回读确认才进入历史；可撤销/重做。外部冲突要求刷新，未知结果不自动重放。
4. 也可输入`标题改为校园摄影社`、`主题色改为#e47735`或`图片使用第1张`生成模拟提案。核对当前值、拟修改值及来源后确认；手动编辑、刷新、关闭或换工程会使提案失效。默认未连接真实模型；外部JSON来源未核验。
5. 保存工程元数据。源码一直是视频内容的权威事实；单文件源码编辑只允许当前会话白名单，明确编译失败时经内容核对恢复。
6. 点击导出MP4。应用冻结本次输入，显示计划、源码版本、BuildID及阶段。完成Work后等待Result保存；精确Output获取、参数与全片解码全部通过才写到选择的目的地。
7. 停止观察保留后台Build；恢复观察查询同一任务。提交尚无ID时仅从独立Runtime查找唯一Build，无法确定时不重提。取消针对当前ID并核对终态。原工程后续编辑不会改变这次冻结成片。
8. 关闭再打开workbench.qvw.json继续编辑。可以搬移整个工程目录；历史导出任务的绝对工程根和目的地需要重新核对，不把旧成片视作新源码的导出。
9. “清理当前终态缓存”只处理已确认终态任务的自有冻结目录；活动、未知或被修改的任务拒绝删除。成片、工程输入与其他任务不删。停止观察不等于清理。

## 日志与诊断

任务面板显示错误原因；JSONL日志包含参数数组及退出码，单条有界并轮转。用户可用--log指定文件。多个实例写同一日志时共用跨进程锁；超过100毫秒的锁竞争会警告本条未保存，随后继续尝试，不永久停止日志。CLI退出、构建完成、Output获取和成片可解码分别验证，任何一层失败都不显示导出成功。

命令行无截图检查：

```bash
'QtVideoWorkbench.app/Contents/MacOS/qt-video-workbench' \
  --verify-startup --create-project '/absolute/path/校园 工程' \
  --report-out '/absolute/path/startup.json' --log '/absolute/path/workbench.jsonl'
```

也可用--project指定已有清单，--config指定外部配置。验证必须读到真实Snapshot、已编译预览和加载完成的图片，退出时清理自有Studio；非零退出及FAIL报告保留原因。验证完成前关闭窗口会返回退出码9和FAIL报告。不截图。

## 交付材料

源码与开发计划、可复现构建脚本、发布.app/ZIP与SHA256、真实样例成片、完整原生操作演示、测试报告及第三方材料。实际产物与最终门禁以STATUS.md及docs/evidence/m6为准；本指南不把尚未运行的发布步骤写成通过。
