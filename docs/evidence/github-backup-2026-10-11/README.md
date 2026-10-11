# GitHub 换机备份（2026-10-11）

源码基准aefc71d；报告/展示/工程提交fb50900（qt-video-workbench分支）。`docs/showcase/manifest.json`记录73个交付文件中除manifest自身外72个文件的SHA、字节数与媒体验证。13个展示视频均与原始演示记录SHA一致且重新完整解码；Word ZIP完整性、三个工程登记素材哈希与相对网页链接通过。运行缓存未纳入可编辑工程。

GitHub Release附件含82.04MiB完整展示备份、117.74MiB macOS arm64运行包和SHA256SUMS。release-upload.json中三个serverDigest均与本地SHA256匹配。GitHub Pages从qt-video-workbench /docs发布。

新增工作仅为资料整理、远端存储与静态展示发布；Qt应用代码及既有测试结果未改动。换机后查看离线报告/视频不需要依赖；继续创作需要恢复外部Hypit/Node/FFmpeg和Codex登录，见docs/showcase/RESTORE.md。

在线展示、三个HTML展示页和Word下载验证可访问；13个视频均返回video/mp4与HTTP206，支持按位置读取/播放。逐项记录见online-playback.json。
