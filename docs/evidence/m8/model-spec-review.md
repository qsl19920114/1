# T037 ModelClient 规格复审

复审时间：2026-10-02 21:41 +0800。结论：**PASS，首次规格审查的 2 项缺口均已修复**。本次仅复核这两项，不扩展审查范围。首次完整 FAIL 报告已原样保留为 [model-spec-initial.md](model-spec-initial.md)。

## 修复核对

| 首次缺口 | 当前实现 | 针对性模拟验证 | 结论 |
| --- | --- | --- | --- |
| 未显式关闭记忆 | `app/agent/ModelClient.cpp:206` 为每次请求加入 `features.memories=false`、`memories.use_memories=false`、`memories.generate_memories=false` | `tests/unit/ModelClientTest.cpp:139` 对三个参数逐项断言；`runsAsynchronouslyWithIsolatedArgumentsAndStdin` 通过 | 已关闭 |
| `{}` 被当作成功 | `app/agent/ModelClient.cpp:276` 在 `object.isEmpty()` 时调用 `fail` 并返回，无法触发 `completed` | `tests/unit/ModelClientTest.cpp:75` 增加空对象假 CLI 模式；`:156` 注册负向数据；`:165`、`:166`、`:176`、`:178` 验证一次失败、无完成、空结果错误和等待后的单次信号；对应测试通过 | 已关闭 |

管理员配置仍可能提供工具。本次参数复核和模拟验证不构成绝对隔离证明；首次报告中的工具事件拒绝边界继续适用。

## 验证证据

读取 `model-fix-red.log`，两个修复目标分别因缺少 `features.memories=false` 和空对象未失败而变红，日志统计 **2 passed / 2 failed**；读取修复后的 `model-green.log`，全套模拟日志为 **19 passed / 0 failed**。本次没有回退实现重演 RED。

本次独立执行两项针对性测试：

```text
.workbench/model-client-test/build/model_client_test runsAsynchronouslyWithIsolatedArgumentsAndStdin 'rejectsInvalidCliResults:empty-object'

PASS : runsAsynchronouslyWithIsolatedArgumentsAndStdin()
PASS : rejectsInvalidCliResults(empty-object)
Totals: 4 passed, 0 failed, 0 skipped, 0 blacklisted, 802ms
exit code: 0
```

统计中的另外两项为 QtTest 的初始化和清理。运行前确认上述测试二进制修改时间晚于本次 `ModelClient.cpp` 和 `ModelClientTest.cpp`；未误用共享 `build/tests/model_client_test` 的旧二进制，未并发重建共享构建目录。

上述验证均通过假 CLI 完成，不访问真实模型。本次只修改审查报告及首次报告副本，未修改实现或测试源代码。

## 真实请求证据的边界

`real-model-probe.json` 已记录当前 Codex 登录的一次真实结构化请求返回 `qvw.agent-plan@1` / `kind=clarify`，问题要求先选择三张图片。该记录说明此前二进制可以获得真实结构化澄清结果。

根据本次任务提供的二进制信息，这条真实请求发生在记忆禁用修复之前，因此不能作为最终修复版客户端的真实请求验收证据。最终客户端仍需单独复测；本报告的 **PASS** 是两项规格缺口的复审结论，不表示完整视频创作闭环或最终客户端真实请求已验收。

复审快照 SHA-256：

```text
edc21d2c062ac6866e6a774e5a64d71625d42c451525d9051ed938a3235c5341  app/agent/ModelClient.cpp
8218e784a00d20275fd4e190336311067cf2222c112ca92db625d5a3f6c93763  tests/unit/ModelClientTest.cpp
9535384253a80f8249537537ec7bbb77e6ba4f12ebf8faee8315cbc7218f4ab7  docs/evidence/m8/model-spec-initial.md
```

## 最终客户端真实请求验收更新

更新时间：2026-10-02 22:55 +0800。**最终修复版 ModelClient 的真实请求已验收，T037 规格结论保持 PASS。** 前文“最终客户端仍需单独复测”记录的是 21:41 复审时的状态，现由以下证据更新；旧 `real-model-probe.json` 的历史边界与原始文件仍保留。

[agent-e2e.json](agent-e2e.json) 记录 `provider=codex-current-chatgpt-login`、`realModelRequests=2`、`verdict=PASS`。最终客户端完成两次真实 Codex 请求：先生成三张本地图片的创建方案，再生成只修改结尾标题的编辑方案。对应 [agent-e2e-run.log](agent-e2e-run.log) 记录两次“等待 Codex 制定方案 → 方案待审阅 → 应用并回读确认”，端到端测试结果为 **3 passed / 0 failed，86766 ms**。

此端到端验证通过真实 Hypit 完成创建、编辑、撤销/重做、重开及媒体导出；证据记录 15 个创建步骤、真实 Build ID、720×1280、30 fps、450 帧和 `ExportController ffprobe and full decode` 验收。图片像素没有上传。方案批准由测试驱动确认；媒体规格和全片解码通过，视觉内容的创作质量仍需人工观看，不能由结构化请求成功替代。

本次规格复审之后，`ModelClient.cpp` 仅补充取消 Starting 状态进程时的回收修复；[model-quality-review.md](model-quality-review.md) 已对该修复给出 PASS，其源码 SHA 与当前实际 SHA 一致。当前 [model-green.log](model-green.log) 为 **22 passed / 0 failed，4082 ms**，属于模拟测试；最终真实请求证据独立来自上述 AgentE2E，二者没有混记。

本次仅核对已有证据、读取文件 SHA 并追加报告，没有重跑模型或修改源码。当前实际 SHA-256：

```text
04a588436bced70010b76b2f272392a432ed4e347fb16b2d8b8acbc7e784cb23  app/agent/ModelClient.h
3a9623028907ac43a92e247b6f79197d54cfa6908e1fd543527ae64b3f201749  app/agent/ModelClient.cpp
0576ae39edbe9e56abf2bd05679b85821463856b894ccd12da1cba48ca14c408  tests/unit/ModelClientTest.cpp
1d565a77bca387a044c5b2682d8028e2eb4460bbc5305220d687e872ffe23474  docs/evidence/m8/agent-e2e.json
1aecf8f8c52c81f6ddb6f33ca11b57bef4b842484fe5128c5aaf6c81b3562ca6  docs/evidence/m8/agent-e2e-run.log
9535384253a80f8249537537ec7bbb77e6ba4f12ebf8faee8315cbc7218f4ab7  docs/evidence/m8/model-spec-initial.md
276a17272d91fc0a0c5da58f8625eea52f141d2bb4902688072d6c2b103eabc6  docs/evidence/m8/real-model-probe.json
```
