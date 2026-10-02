# M5 受约束编辑提案

执行前提为 G4 通过。沿用 PROJECT_PLAN：模型账号可选；本阶段实现可替换 Provider 与明确标注的本地模拟，不宣称接入真实模型。使用 subagent-driven-development、TDD 与规格/质量复审。

## 设计与任务

- [x] T025：`EditProposal` 只允许一项属性修改，JSON 严格白名单字段：format=`qvw.edit-proposal@1`、revision、sourceFingerprint（SHA256）、entityId、fieldId、value、origin。origin 由入口确定，导入 JSON 不可自行变成可信/真实模型来源。拒绝未知键、操作数组、Source、Shell、任意路径等。值必须匹配快照真实可写字段类型；图片仅允许当前工程登记且路径合法的素材。
- [x] T026：`ProposalController` 保存一个待确认提案，显示当前值/拟修改值/来源。创建和确认均核验 editor 空闲、当前工程、revision 与指纹；确认后仅调用现有 `EditorController::edit`。快照变化、关闭或重开即失效；不自动确认、不另设 mutation 通路，历史及失败语义沿用 M3。
- [x] T027：`IProposalProvider` 可替换接口与 `DemoProposalProvider`。演示输入仅支持“标题改为… / 主题色改为#RRGGBB / 图片使用第N张”；只选择模板声明的 title/color/image binding，不自由猜测其他字段。原生对话框标“模拟”；可从最大64KiB文件导入外部 JSON，来源标“外部 JSON，来源未核验”。缺凭据不发模型请求。
- [x] T028：严格 schema/类型/越权/未知字段/过期/只读/图片白名单/忙时确认等负向测试。真实 Studio 中生成提案，确认前源码不变；确认标题与图片后可回读，撤销恢复。所有模拟与真实执行证据分开。

## 文件分工

实现代理：domain/EditProposal、services/ProposalService 与 ProposalProvider、controllers/ProposalController 及单元测试。主代理：CMake、MainWindow/原生提案对话框、main 信号接线、真实集成、证据与状态文档。只同时运行一名实现代理；两阶段复审通过后提交推送。

## 验证

新提案单测必须先失败后通过；完整 CTest 回归；生产 EditorController 的真实提案 E2E。不截图验收。G5 仅覆盖受约束提案和真实执行链，真实模型 Provider 保留“未配置、未验收”。
