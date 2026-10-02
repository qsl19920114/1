# 当前架构与工程格式

UI 使用领域 DTO；main 仅连接信号。DocumentController 管理持久化工程和素材，ProjectController 管理异步 Hypit 自检、Studio 生命周期和 Snapshot，EditorController 管理原生编辑与历史。UI 不解析 Hypit 内部 JSON。

依赖由 CMake targets 约束：UI → Domain，Controllers → Services / Backend / Infrastructure；Services → Domain + Qt Core/Gui；Backend → Domain + Network。

## 工程格式 v1

工程清单固定为 `workbench.qvw.json`，`format` 为 `qt-video-workbench.project@1`。保存名称、模板 id/version、Source/Run/Runtime 相对路径、素材索引和大小策略。rootPath 只在内存中存在；迁移目录后从清单位置重新确定。Source 是唯一编辑事实，不在清单中复制时间线或属性值。

创建只接受应用内可信模板，向新目录或空目录复制。不能覆盖非空目录或沿模板符号链接复制外部文件。失败仅清理本次创建的文件。元数据用 QSaveFile 原子替换，损坏或未知版本的既有文件不覆盖。

图片按内容检测、解码并保存为 `assets/<sha256>.png|jpg`。原始文件不修改；相同内容去重；已存在的不同内容目标不覆盖。登记失败时不发布半完成的内存索引。相对路径与符号链接不得逃出工程。

## 运行与写回边界

启动器与外部 Hypit 从版本锁解析。模板的 `@hypit/` imports 由 Distribution resolver 解析；工程内不含上游仓库或机器绝对依赖路径。

Studio Snapshot 是编译结果。图片导入与场景绑定是两个动作，绑定与原生属性、撤销/重做统一经过 EditorController。原生表单只启用真实 edit 声明与支持的控件；number/boolean 按接口要求发 JSON 数字/布尔值，快照中的字符串字面量只在边界转换。

EditorController 串行执行预检 GET、写入、确认 GET。预检同时核对 revision 与源码指纹；只在写后 revision 和目标值吻合时推进历史。409 刷新并重置历史；422 核对服务端回滚后保留有效历史；超时或未知结果不重放写入。关闭或切换工程取消旧代次请求，防止回调污染新工程。

单文件 Source 编辑只允许 `source.files` 白名单。SourceEditGuard 保存磁盘预像并校验工程路径；PUT 202 后，只有相同 revision 的明确编译失败才恢复预像。恢复前再次检查文件仍为本次写入，外部变化不覆盖。该内容检查不是跨进程文件锁；多编辑器竞态仍由版本与回读检测处理。合法增删 import 可以改变编译依赖文件集合。

M4 再实现 build/status/get 与可解码验证；此前不把 plan 或 HTTP 成功显示为导出成功。
