# writable-probe fixture

用途：给 `probes/qt-webengine/http_probe --write` 提供一个**含真实可写属性**的 Run，以验证 `POST /__studio/mutation` 的成功路径。

## 为什么需要它

上游 `examples/semantic-composition` 的 chat 示例**不带 Studio Companion**，所以它的 snapshot 里 `tracks[].clips[].inspector` 是空数组，没有任何字段能接受 `parameter.adjust`。M0 因此无法验证成功写入，G0 只能判 PARTIAL。

本 fixture 补的正是缺失的那一层**声明**：Surface 解码器、manifest、渲染器全都不动，仍然来自上游 `dist/`。

## 可写的判定条件

从 Hypit 0.2.10 源码逐行核实（不是推测）：

1. `packages/studio/src/parameters.ts:417`
   ```ts
   const writable = declaration.writable === true && !isReference;
   ```
   即必须**同时**满足：Companion 声明了 `writable: true`，且该属性在 SVML 中是**字面值**而非 `{reference}`。引用绑定的属性被强制只读。

2. `packages/studio/src/parameters.ts:495`
   ```ts
   ...(binding.writable ? { edit: { language, source, ... } } : {}),
   ```
   `edit` 键**仅在**上述 writable 为真时出现，否则整个键不存在（不是 `edit: null`）。这就是 Qt 侧判定字段可写的唯一依据。

3. `packages/studio/src/parameters.ts:482`
   无 public schema 且未显式给 `control` 时，`inspectorFieldsForBindings` **抛错**而不是静默丢弃字段。所以本 fixture 为两个 binding 都显式写了 `control`。

## 选 title 与 entrance-frames 的理由

上游 `chat-scene/src/activation.ts` 已经把这两个属性声明为字面属性并通过 `element.attributes` 读取，`chat.svml` 里 `title="Launch crew"` 也是字面值。**没有为了制造可写属性而编造字段**，符合 AGENTS.md 第 7 条。

两者覆盖了不同情形：

| 字段 | control | SVML 中 | 写入时的行为 |
|---|---|---|---|
| `title` | text | 已存在 | 替换已有属性值 |
| `entrance-frames` | number | **被省略**（走 fallback `"10"`） | 向源文件**插入**新属性 |

`entrance-frames` 的 `edit.source.range` 实测为 `{start:650,end:650}` 空区间，正是"属性不存在、待插入"的表示。这一点对 Qt 的写入实现有影响：不能假设 range 非空。

## 文件

| 文件 | 作用 |
|---|---|
| `studio.js` | Studio Companion facet，声明两个可写 binding 与对应 inspector 字段 |
| `activation-studio.js` | 包装上游 `dist/activation.js`，往 `hostFacets` 追加本 facet |

两个文件都是纯 JavaScript，**无需 tsc 构建**。这是照 `examples/complex-explainer/packages/web-scenes` 的模式来的——该包的 `hypit.activation` 直接指向 `src/*.js`，没有 build 步骤。

## 复现步骤

```bash
# 1. 复制上游示例到 scratch 目录（不污染上游仓库）
rm -rf /tmp/t007 && mkdir -p /tmp/t007
cp -R ../hypit/examples/semantic-composition/* /tmp/t007/
cd /tmp/t007 && rm -rf .hypit packages/responsive-explainer \
  packages/performance-styles packages/sound-styles

# 2. 放入本 fixture 的两个文件
cp <repo>/tests/fixtures/writable-probe/*.js packages/chat-scene/src/

# 3. 把 activation 指向新入口
node -e "const fs=require('fs');const p=JSON.parse(fs.readFileSync('packages/chat-scene/package.json','utf8'));p.hypit.activation='./src/activation-studio.js';fs.writeFileSync('packages/chat-scene/package.json',JSON.stringify(p,null,2)+'\n')"

# 4. 链接 Hypit
mkdir -p packages/chat-scene/node_modules/@hypit
ln -sfn <abs-path-to>/hypit packages/chat-scene/node_modules/@hypit/hypit

# 5. 启动 Studio
<hypit> studio --run chat.svrun --workspace . --runtime hypit.runtime.json --port 5610

# 6. 跑带写入的探针（注意用 stdout 实际打印的端口）
<repo>/probes/qt-webengine/build/http_probe http://localhost:<port> --write
```

预期 `ALL PASS`、退出码 0，其中包含：

```
INFO writable field discovered: yes control=text parameterId=...:inspector:title
PASS POST mutation, real writable field -> 200 accepted  body= {"revision":8}
PASS revision advanced 7 -> 8
PASS value persisted in recompiled snapshot: {"after":"Qt 写入验证","before":"校园社团介绍"}
PASS POST mutation, replaying the consumed revision -> 409 conflict
```

## 注意

- **改 activation 或包代码后必须重启 Studio 进程。** 上游 `packages/studio/README.md` 与 `docs/guide/studio-companion-architecture.md` 都明确说明它不热加载包模块。
- 端口被占用时 Studio 会**自动改用下一个端口**并在 stdout 打印（实测 5610 被占时让到 5611）。Qt 侧必须解析 stdout 的 URL 行，不能假设传入的 `--port` 生效。
- 本 fixture 只用于测试。M2 的自建 title-card 模板是独立交付物，不复用这里的 chat-scene。
