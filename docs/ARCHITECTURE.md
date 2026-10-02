# 当前架构与工程格式

UI 使用领域 DTO；main 仅连接信号。DocumentController 管理持久化工程和素材，ProjectController 管理异步 Hypit 自检、Studio 生命周期和 Snapshot，EditorController 管理原生编辑与历史。UI 不解析 Hypit 内部 JSON。

依赖由 CMake targets 约束：UI → Domain，Controllers → Services / Backend / Infrastructure；Services → Domain / Infrastructure + Qt Core/Gui；Backend → Domain / Infrastructure + Network。

## 工程格式 v1

工程清单固定为 `workbench.qvw.json`，`format` 为 `qt-video-workbench.project@1`。保存名称、模板 id/version、Source/Run/Runtime 相对路径、素材索引和大小策略。rootPath 只在内存中存在；迁移目录后从清单位置重新确定。Source 是唯一编辑事实，不在清单中复制时间线或属性值。

创建只接受应用内可信模板，向新目录或空目录复制。不能覆盖非空目录或沿模板符号链接复制外部文件。失败仅清理本次创建的文件。元数据用 QSaveFile 原子替换，损坏或未知版本的既有文件不覆盖。

图片按内容检测、解码并保存为 `assets/<sha256>.png|jpg`。原始文件不修改；相同内容去重；已存在的不同内容目标不覆盖。登记失败时不发布半完成的内存索引。相对路径与符号链接不得逃出工程。

## 运行与写回边界

启动器与外部 Hypit 从版本锁解析。模板的 `@hypit/` imports 由 Distribution resolver 解析；工程内不含上游仓库或机器绝对依赖路径。

Studio Snapshot 是编译结果。图片导入与场景绑定是两个动作，绑定与原生属性、撤销/重做统一经过 EditorController。原生表单只启用真实 edit 声明与支持的控件；number/boolean 按接口要求发 JSON 数字/布尔值，快照中的字符串字面量只在边界转换。

EditorController 串行执行预检 GET、写入、确认 GET。预检同时核对 revision 与源码指纹；只在写后 revision 和目标值吻合时推进历史。409 刷新并重置历史；422 核对服务端回滚后保留有效历史；超时或未知结果不重放写入。关闭或切换工程取消旧代次请求，防止回调污染新工程。

单文件 Source 编辑只允许 `source.files` 白名单。SourceEditGuard 保存磁盘预像并校验工程路径；PUT 202 后，只有相同 revision 的明确编译失败才恢复预像。恢复前再次检查文件仍为本次写入，外部变化不覆盖。该内容检查不是跨进程文件锁；多编辑器竞态仍由版本与回读检测处理。合法增删 import 可以改变编译依赖文件集合。

## M4 导出链

`ExportController` 持有单项导出状态；`JsonProcess` 仅观察有界异步CLI，取消进程不代表取消Worker任务。`ExportWorkspace` 将当前工程冻结为独立UUID目录，核对Snapshot源码预像与所有复制输入hash，任务记录保存源版本/指纹/固定Hypit版本/精确Build ID。未知提交保留active状态，只查询独立Runtime的唯一Build，无法确定时拒绝新提交。

plan要求本地Runtime、完整providers、本地请求计数与预检通过；status把Work和Result分开，已完成Work但Result尚未保存继续观察。get只取当前Build的final.video。`MediaValidation`先ffprobe后全片decode，最后QSaveFile原子交付。停止或关闭保留Worker及记录，取消指定Build后继续查终态；本项目不以CLI exit0显示成片成功。

## M5 提案边界

`IProposalProvider`只返回单项属性提案；默认`DemoProposalProvider`为显式模拟，不发模型请求。外部JSON来源不由其自述决定，导入后统一标来源未核验。`ProposalService`严格核对schema、值类型、真实可写字段、revision/源码指纹，以及图片素材白名单。

`ProposalController`管理待确认提案；生成不写入，确认时重新校验，随后只调用现有`EditorController::edit`。快照、工程、就绪状态或编辑过程变化会使旧提案失效。界面以纯文本显示当前值/拟修改值/来源，避免把不可信提案内容作为HTML。

## M6 运行资源与发布

`RuntimePaths`根据 executable 定位相对资源；macOS 为.app内Contents/Resources，其余平台为可执行文件旁resources（仅macOS已验收）。构建复制原创templates；发布脚本复制相对布局的默认配置及第三方材料。打包默认配置优先于开发仓库/CWD搜索，无绝对模板编译常量。

`AppConfig`限制普通配置文件64KiB、类型/路径长度与NUL，解析可选绝对tools路径；各QProcess继承同一应用环境。子进程PATH加入显式工具目录及常用安装位置，系统环境不改。统一启动策略用解析到的Node执行JS入口，默认直接执行shell启动器；显式Node与固定版本官方shell同时配置时，核对官方脚本内容后使用其相邻JS入口。自定义shell不擅自绕过，无法满足显式Node约束时返回可读错误。媒体校验使用解析到的ffprobe/ffmpeg。Hypit Distribution仍为固定版本的外部依赖。

`LogWriter`限制消息/参数/单条JSON，5MiB轮转到.1/.2；既有备份为symlink或特殊文件时拒绝轮转。初始化和轮转/追加共用跨进程QLockFile；最多等100毫秒，竞争时记录警告并保留后续写入能力，成功后清除临时错误。轮转后当前文件缺失可在持锁时重建；真正不安全路径或I/O失败仍明确报错。界面日志保留有限block。`clearFinishedCache`拒绝活动、未知、缺ID或版本不符任务，再按status→activity→runtime down确认精确终态与停止；各步骤重验当前任务记录、UUID目录、输入hash与全树无链接。仅删除当前冻结目录及记录。其他Build即便已完成也保守拒绝；停止观察和应用关闭不删除Worker数据。

发布脚本部署Qt/WebEngine，逐个实际Mach-O检查非系统链接与RPATH、架构和minimum deployment target；本地签名包含WebEngine helper原权限，并校验plugin、ICU/pak/snapshot资源。资源/Qt随包，Hypit/Node/媒体/渲染浏览器不在ZIP内。`--verify-startup`使用真实Studio验证Snapshot、已编译composition、至少一张实际加载图片和退出清理，以JSON报告记录结果；验证完成前关闭窗口返回退出码9和FAIL，退出清理成功不代替预览验证成功。
