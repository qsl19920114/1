# M15 / FrameLab 1.7 证据索引

## 组件与模板

- `component-red.txt`、`component-browser-green.txt`、`component-browser-summary.json`：组件搜索/筛选/精确选择/激活的RED、定向回归与摘要。
- `framing-red.log` / `framing-green.log`：两套原创模板新增行为的RED和16项Node测试GREEN。
- `framing-hypit-check.log`：固定Hypit实际编译：默认、旧版缺省属性、contain+小数位置通过，越界拒绝；两套真实Studio字段存在edit。

## 实际 Qt / Agent / Hypit

- `component-framing-initial-failure.log`、`component-framing-type-failure.log`：两次方案在批准前被严格类型校验拦截。第二次原始公开结果保存在 `model-string-number-rejected.json`，其数字字段返回字符串。
- `context-red.log` / `context-green.log`：真实失败引出的回归测试；ContextBuilder增加明确valueType并说明JSON类型，保留实际raw值、不放宽提案校验。
- `component-framing-first-pass.log`：首次流程通过。最终驱动进一步区分参数拒绝和实际源码编译回滚，不作为最终交付记录。
- `component-framing-run.log` / `component-framing.json`：最终生产Qt控件/信号驱动、当前Codex生成和明确批准、真实写入、预览同指纹、两份成片、特定非法范围诊断与源码回滚。不是模型fixture，也不是手工操作录像。
- `framing-media.json`：最终实际成片解码/尺寸、抽帧与失败操作后成片哈希复核，配合实际查看确认构图差异。

## 最终验证

- `spec-review.md`、`quality-review-summary.md`、`quality-review.html`：独立规格、分组质量与交叉审查。
- `ctest-initial.log`：首次全量回归发现受控StoryTemplate仍限制1.0.0，使story_template和Agent create失败；并出现既有export持久化测试5秒超时（5200ms足够）。`export-persistence-recheck.log`同一失败用例单独重跑3.684秒通过，未修改其超时或产品实现。
- `framing-story-generated-hypit.log` 确认生产创建器生成源码及真实Studio共9个可写构图字段；`framing-story-red.log` / `framing-story-green.log`：受控创建版本及每场景显式构图字段回归；旧版不注入新字段，未知版本拒绝。
- `ctest-final.log`：修复后的完整CTest；真实模型驱动单独显式运行，不进入自动CTest。
- `package-build.log`、`package-rebuild.log`、`package-final.log`、`packaged-startup.json`、`packaged-native.json`、`release.json`、`delivery-integrity.json`：1.7包、中文空格路径搬移、仓库外启动、预览/媒体、进程清理、签名、许可和归档一致性。

本轮全部真实驱动共4次Codex请求；前两次未批准，后两次各明确批准1次，最终交付驱动含1次。图片沿用M14明确来源的官方归档素材，未上传像素。旧工程模板不自动迁移；新建工程支持新字段。没有付费Provider、上游代码修改或截图验收。成片和工程位于忽略目录 `.workbench/deliverables-m15/`，先前演示和报告保留。

首次1.7部署后修复StoryTemplate，重新构建Release程序并替换自有包可执行文件，随后使用现有发布脚本repair-existing重新审计依赖、签名及生成ZIP；最终包以delivery-integrity.json的SHA为准。初始ZIP移至 `.workbench/diagnostics-m15/`，不作为交付包。
