# 换电脑恢复 FrameLab

## 只查看报告和演示

打开[在线展示](https://qsl19920114.github.io/1/showcase/)，或者从[GitHub附件](https://github.com/qsl19920114/1/releases/tag/framelab-v1.7.0-backup)下载`FrameLab-1.7.0-showcase-backup.zip`，解压后双击`showcase/index.html`。全部视频、截图和HTML都使用相对链接；Word在`showcase/m13/experiment-report.docx`。离线展示无需Qt或Hypit。

## 恢复源码与可编辑工程

```bash
git clone --branch qt-video-workbench https://github.com/qsl19920114/1.git FrameLab
```

源码在FrameLab目录；展示在`docs/showcase/`；三个工程位于`docs/showcase/projects/`。把要编辑的工程整个目录复制到自己的工作目录，Qt选择“打开工程”并选其`workbench.qvw.json`。每个工程的源码、原创包和图片/视频素材都在目录内，路径为相对路径。旧机运行缓存不影响重开工程。

## 在新Mac运行Qt工作台

从同一[附件页面](https://github.com/qsl19920114/1/releases/tag/framelab-v1.7.0-backup)下载`FrameLab-1.7.0-macOS-arm64.zip`并解压。运行包适用于Apple芯片Mac、macOS15及以上，包含Qt/WebEngine。原机仅验证macOS15.7.7 arm64，本地ad hoc签名；Intel Mac或Windows使用源码另行构建和验证。

继续创作需要外部Hypit0.2.10（commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`）、Node>=22.15、pnpm10.33.0、FFmpeg/ffprobe与本地渲染浏览器。按[固定版本的Hypit安装说明](https://github.com/hypit-ai/hypit/tree/1af179d3f58284c2d6d3c1f63052172a4fe1b5a6)准备依赖；目录可放在.app同级的`hypit/`。自定义位置在应用“选择配置…”中指定，参考[使用指南](../USER_GUIDE.md)。

使用AI时在新电脑重新登录Codex；路径变化后按使用指南配置`tools.codex`。旧机的启动.command包含本机绝对路径，在新电脑直接打开.app并选择已备份工程即可。编译源码所用命令在[仓库README](../../README.md)。

## 下载后核对完整性

展示备份ZIP里的`showcase/manifest.json`记录每个文件的SHA256、大小及视频参数。运行包的SHA记录在发布附件`SHA256SUMS.txt`。已备份原始文档与视频，无需重新生成来恢复展示。
