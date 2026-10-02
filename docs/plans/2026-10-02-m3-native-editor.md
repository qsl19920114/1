# M3 原生编辑执行计划

沿用用户已批准的 PROJECT_PLAN：原生编辑统一经过 EditorController，真实 Source 权威，不截图验收。

## 实施与验证顺序

- [x] T015：StudioWriter异步POST；仅本机地址、串行写入、取消代次隔离、严格成功响应。写前GET确认当前revision，写后GET确认新值；UI五类控件发出编辑请求。
- [x] T016：409刷新并清空历史；422读取服务端回滚结果，保留合理历史起点；超时/不明响应只刷新，不自动重试或宣称成功。
- [x] T017：编辑历史仅在成功确认后推进；撤销/重做沿相同mutation链；素材绑定使用真实字段binding标识和相同控制器。
- [x] T018：只允许Snapshot列出的单文件Source；保留预像，写后检查编译；自动恢复前确认磁盘仍为本次内容，外部改动不覆盖。
- [x] T019：真实原创工程验证标题/颜色/图片变化、撤销重做、422回滚、409外部修改、受保护Source与关闭重开；检查编译预览和实际资源，不截图。
- [x] 完成规格与质量审查，构建及测试，更新文档/任务；改动已准备提交并推送当前分支。

## 数据与状态

领域Snapshot新增sourceFiles和编译源指纹；InspectorField新增binding，用于可靠定位模板slot。UI不解析Hypit JSON。

EditorController独立持有只用于编辑的StudioClient/StudioWriter。主窗口生命周期绑定当前Studio URL；关闭或切换先取消旧读写。编辑开始锁定原生控件，拒绝并行请求；没有自动重放队列。预检、写入、后验/恢复均异步。完整成功以前不移动历史指针。

Studio合法无操作可返回相同revision（server.ts454–455）；参数写入响应200，Source写入响应202。Source写入202不代表编译成功。实际错误、取消和未知结果分开记录。
