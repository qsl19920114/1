# M2 验证证据

验收不截图；只使用本地 Provider。本目录的日志和 JSON 来自实际运行。

- `build.log` / `configure.log`：CMake 配置与构建。
- `ctest.log`：完整8套测试最终结果。
- `project-store-test.log`：ProjectStore 的17项结果，含格式、迁移、原子失败与回滚。
- `asset-service-test.log`：AssetService 的17项结果，含真实图片解码、去重、范围限制与失败保存。
- `template-test.log`：原创视觉元素的测试；`template-check.log` / `template-plan.json`：真实 check/plan，本地请求3，收费请求0。
- `e2e.log` / `e2e.json` / `session.json`：生产Controller/Service创建、导入、搬迁重开、真实Studio会话；1280×720/240帧/5个字段；图片HTTP 200且字节与外部原图完全一致；切换无效工程关闭旧会话。
- `review-report.html` / `review-summary.md`：提交前只读审查，未发现剩余P0–P2。

## 复现

从仓库根目录执行：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j4
ctest --test-dir build --output-on-failure
node --test tests/template/title-card.test.mjs
./build/tests/project_e2e_test
```

真实集成需要本机监听权限和版本锁中的Hypit；不需要桌面窗口或截图权限。临时工程在`.workbench/`内，测试完成清理。

## 失败记录与修复

`*-red.log`保留实际失败证据：空实现、新清单文件名规则、重复Visual IR顺序和旧Studio残留。它们用于说明修复的回归覆盖，不是当前通过证据。

`ctest-initial.log`记录旧版本自检测试一次偶发失败（状态InvocationFailed）；独立重复20次未复现。检查发现所有用例均用了300ms时限。加入延迟500ms的成功fixture后可稳定触发超时，见`probe-delay-red.log`。现在仅超时fixture保持300ms，其他用例2s；生产代码仍20s，见`probe-test.log`及最终ctest。

`e2e.log`末尾的Runtime错误是故意切换无效工程的回归条件。退出码143是测试主动终止Studio；不表示导出取消或失败。本阶段验证编译预览与资源交付，不声称完成MP4导出。

模板unit test验证视觉元素数据；真实会话验证实际编译及图片资源。E2E绑定图片通过fixture预先写Source，明确不作为M3原生编辑已完成的证据。
