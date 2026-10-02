# M3 原生编辑验收

环境：macOS 15.7.7 arm64、Qt 6.11.2、外部 Hypit 0.2.10（版本锁中的 commit）、Node 25.8.2。沿用用户要求，不截图。

## 可复现命令

在仓库根目录执行：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/tests/editor_e2e_test
```

测试使用本机临时 HTTP 服务器；真实集成还会启动版本锁指向的外部 Studio，需要本机监听权限与该依赖。临时工程位于忽略的 `.workbench/` 并在结束后清理。

## 证据范围

- `build.log` / `ctest-final.log`：最终构建与全部 CTest。
- `e2e.log` / `e2e.json`：真实标题、颜色、图片写入；编译预览含新文字/颜色，图片 HTTP 字节与原图一致；撤销/重做；422 回滚；Source 编译失败的内容保护恢复；外部写入拒绝；关闭重开一致与自有进程结束。
- `editor-test.log`：控制器模拟接口测试，包括409恢复、未知响应、数字/布尔字符串快照、合法 Source 依赖集合变化。真实 E2E 分开验证过期revision请求返回HTTP409并保留文件，以及外部修改在控制器写前GET阶段被拦截。
- `writer-test.log`、`source-test.log`、`controls-test.log`：写入边界、文件恢复保护、原生控件。
- `mapper-test.log`、`read-failure-test.log`：快照映射与错误携带的 HTTP/revision 信息。

`*-red.log` 是开发中先失败的回归证据，不是最终验收失败。`e2e-number-contract-red.log` 记录了首次真实联调发现的类型错误：Snapshot 数值为字符串字面量，POST 却必须发 JSON 数字。后续修复同时覆盖正向写入和撤销值转换。`source-whitelist-red.log` 对应缺少 `source.files` 时错误地从顶层 Source 兜底授权的问题，现已移除该兜底。

`e2e-busy-conflict-red.log` 记录 macOS 文件监听在422回滚后触发延迟编译、下一次Source请求返回409。应用正确报告失败，没有误记成功。最终测试在开始独立Source场景前只读等待 `/__studio/document` 就绪并刷新，未自动重放失败的修改。

源码恢复依赖内容检查与原子替换，不提供跨进程文件锁。网络超时或无法确认的响应不会自动回滚文件或重放写入。此门禁验证编辑闭环，应用内 MP4 导出属于 M4。
