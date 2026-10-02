# 当前架构与工程格式

UI 使用领域 DTO；main 仅连接信号。DocumentController 管理持久化工程和素材，ProjectController 管理异步 Hypit 自检、Studio 生命周期和 Snapshot。UI 不解析 Hypit 内部 JSON。

依赖由 CMake targets 约束：UI → Domain，Controllers → Services / Backend / Infrastructure；Services → Domain + Qt Core/Gui；Backend → Domain + Network。

## 工程格式 v1

工程清单固定为 `workbench.qvw.json`，`format` 为 `qt-video-workbench.project@1`。保存名称、模板 id/version、Source/Run/Runtime 相对路径、素材索引和大小策略。rootPath 只在内存中存在；迁移目录后从清单位置重新确定。Source 是唯一编辑事实，不在清单中复制时间线或属性值。

创建只接受应用内可信模板，向新目录或空目录复制。不能覆盖非空目录或沿模板符号链接复制外部文件。失败仅清理本次创建的文件。元数据用 QSaveFile 原子替换，损坏或未知版本的既有文件不覆盖。

图片按内容检测、解码并保存为 `assets/<sha256>.png|jpg`。原始文件不修改；相同内容去重；已存在的不同内容目标不覆盖。登记失败时不发布半完成的内存索引。相对路径与符号链接不得逃出工程。

## 运行与写回边界

启动器与外部 Hypit 从版本锁解析。模板的 `@hypit/` imports 由 Distribution resolver 解析；工程内不含上游仓库或机器绝对依赖路径。

Studio Snapshot 只是编译结果。M2 新建工程后预览与原生只读属性来自真实 Snapshot；图片导入与场景绑定是两个动作。M3 将通过统一 EditorController 实现版本安全写回和历史。M4 再实现 build/status/get 与可解码验证；此前不把 plan 或 HTTP 成功显示为导出成功。
