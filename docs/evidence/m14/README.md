# M14 / FrameLab 1.6 证据索引

## 实现与定向回归

- `character-handoff-red.log`：缺少人物入口时的真实失败；最终 `targeted-green.log` 含产品交互、面板及播放器3组通过。
- `scope-red.log`：首次人物交接显示完整内部路径；修复后任务范围显示组件名、完整ID保留提示。
- `player-red.log`、`player-green.log`：原播放器缺控件/新实现的实际媒体测试。
- `player-loop-diagnostic-repeat.log`：实际循环时缓冲导致停止按钮失效；最终实现锁定已解码就绪状态，错误/关闭时撤销。
- `player-close-red.log`：全屏直接关闭不能销毁；后续修复分离关闭和Escape。
- `player-escape-crash.log`、`player-escape-red.log`：修复关闭后暴露直接按键事件绕过shortcut；旧测试使用已释放指针导致崩溃。测试改为QPointer断言，产品增加keyPressEvent处理Escape，最终 `player-final.log` 7项通过。
- `build-initial.log`：演示脚本JSON/QVariant编译问题，已修复；`build.log` / `fix-build.log`：后续成功构建。

## 真实人物演示

- `character-showcase-initial.log`：演示初始写入传入QJsonValue而非简单QVariant，控制器正确拒绝；驱动改为toVariant。
- `character-showcase-conflict.log`：基准准备连续编辑触发Studio源码冲突保护；驱动加入有界快照/预览稳定检查。上述两次均在模型请求前失败，没有计入真实演示通过。
- `character-showcase.log` / `character-showcase.json`：最终真实Qt人物演示。只有PASS记录可计入交付；含2次Codex/批准、三份成片、播放器、录像哈希和实际时间。
- `materials/`：三张官方图片的来源、精确归档成员、SHA、原始尺寸、安全提取测试、实际解码和重复执行一致性。
- `showcase-delivery.json`：展示页与四个视频的最终完整性。

## 审查、回归和交付

- `spec-review.md`：独立规格实现审查；实际演示和发布完成状态以对应JSON为准。
- `quality-review-summary.md`、`quality-review.html`：独立分组/交叉审查、发现修复情况。
- `ctest-final.log`：最终完整CTest。
- `package-build.log`、`packaged-native.json`、`packaged-startup.json`、`delivery-integrity.json`：1.6构建、搬移中文空格路径真实启动和签名/ZIP/许可材料一致性。

视频与原工程位于忽略目录 `.workbench/deliverables-m14/`，旧版本保留。人物素材替换不等于原视频人物重生成；静态人物原图属于上游公开归档，Qt工作台、受限Agent流程与模板为本项目实现。没有调用外部图像/视频生成Provider，没有上传人物像素，也没有以截图作为验收。
