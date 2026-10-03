#!/usr/bin/env python3
"""Package only verified real Qt character demonstration media into a local gallery."""
import argparse
import hashlib
import json
from html import escape
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def require(value, message):
    if not value:
        raise ValueError(message)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', type=Path, default=REPO/'docs/evidence/m14/character-showcase.json')
    parser.add_argument('--materials', type=Path, default=REPO/'.workbench/character-showcase/materials/catalog.json')
    parser.add_argument('--output', type=Path, default=REPO/'.workbench/deliverables-m14')
    args = parser.parse_args()
    report = json.loads(args.demo.read_text())
    require(report.get('verdict') == 'PASS', '真实演示没有通过')
    require(report.get('realModelRequests') == 2 and report.get('explicitDriverApprovals') == 2, '模型/批准证据不完整')
    require(all(report.get(x) is True for x in ['unchangedBeforeApproval', 'nonImagePropertiesPreserved', 'recordingFullDecode', 'ownedStudioStopped']), '缺少执行或清理证据')
    require(report.get('pixelsUploaded') is False, '演示边界不匹配')
    artifacts = report['artifacts']
    require(len(artifacts) == 3 and all(a.get('fullDecode') is True for a in artifacts), '需要三份通过解码的真实成片')
    require(len({a['sha256'] for a in artifacts}) == 3, '三份成片应有不同内容')
    provenance = json.loads(args.materials.read_text())
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.character-delivery-', dir=output) as temporary:
        stage = Path(temporary); (stage/'videos').mkdir(); (stage/'evidence').mkdir(); (stage/'images').mkdir()
        media = []
        sources = [(report['recording'], report['recordingSha256'], 'walkthrough.mp4', 'Qt 内嵌 AI：完整操作演示')]
        sources += [(a['path'], a['sha256'], f'character-{i+1}.mp4', title) for i,(a,title) in enumerate(zip(artifacts,['原始人物','替换人物','插画人物']))]
        ffprobe = shutil.which('ffprobe'); ffmpeg = shutil.which('ffmpeg'); require(ffprobe and ffmpeg, '缺少 ffprobe / ffmpeg')
        for source, expected, filename, title in sources:
            source = Path(source); require(source.is_file() and sha(source) == expected, '媒体与真实执行证据SHA不一致')
            target = stage/'videos'/filename; shutil.copy2(source, target); require(sha(target) == expected, '复制媒体校验失败')
            p = subprocess.run([ffprobe, '-v','error','-show_streams','-show_format','-of','json',str(target)], capture_output=True, text=True, timeout=30, check=True)
            probe = json.loads(p.stdout); stream = next(s for s in probe['streams'] if s['codec_type']=='video')
            duration = float(probe['format']['duration']); require(duration > 0, '时长无效')
            poster = 'images/'+Path(filename).stem+'.jpg'
            subprocess.run([ffmpeg,'-y','-v','error','-ss',str(min(18 if filename=='walkthrough.mp4' else 3,duration/2)),'-i',str(target),'-frames:v','1',str(stage/poster)],check=True,timeout=30)
            require((stage/poster).is_file(), '封面帧提取失败')
            media.append({'poster':poster,'title':title,'path':'videos/'+filename,'sha256':expected,'bytes':target.stat().st_size,'durationSeconds':duration,'width':stream['width'],'height':stream['height'],'fullDecodeEvidence':'SHA256-bound producing demo evidence','ffprobe':probe})
        require(all(abs(m['durationSeconds']-8)<0.1 for m in media[1:]), '成片时长不符')
        require(abs(media[0]['durationSeconds']-report['recordingElapsedMs']/1000)<2, '录像没有保留实际时长')
        shutil.copy2(args.demo, stage/'evidence/demo.json'); shutil.copy2(args.materials, stage/'evidence/materials.json')
        cards = ''.join(f'<article><h3>{escape(m["title"])}</h3><video controls preload="metadata" poster="{m["poster"]}" src="{m["path"]}"></video><p>{m["durationSeconds"]:.2f} 秒 · {m["width"]} × {m["height"]}</p></article>' for m in media[1:])
        steps = ''.join(f'<li>{s["atMs"]/1000:.1f}s — {escape(s["title"])}</li>' for s in report['stages'])
        html = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>FrameLab · 人物素材创作演示</title><style>body{margin:0;background:#0b1220;color:#e9f0f8;font:16px/1.7 system-ui,sans-serif}main{max-width:1280px;margin:40px auto;padding:0 24px}h1{font-size:36px}h2{margin-top:36px}p,li{color:#b9c8da}a{color:#4bd9c4}.tag{color:#4bd9c4;letter-spacing:2px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:18px}article,.hero{padding:18px;border:1px solid #2b3d55;background:#131e2f;border-radius:16px}video{width:100%;max-height:76vh;border-radius:10px;background:#070b12}.note{border-left:3px solid #4bd9c4;padding:10px 18px;background:#142738}code{overflow-wrap:anywhere}</style><main><div class="tag">FRAMELAB / QT × AI × HYPIT</div><h1>同一舞台，不同人物</h1><p>在 Qt 中选择人物素材，由内嵌 Agent 生成受限方案，审阅确认后写回 Hypit，更新预览并导出。</p><div class="note">本次实现的是人物图片素材替换：保留文案、版式与入场动画。没有对原视频人物进行换脸或动作重生成；模型接收登记信息，没有接收人物像素。三张人物素材复用 Hypit 官方归档，来源见下方清单。</div><h2>完整操作录像</h2><div class="hero"><video controls preload="metadata" poster="images/walkthrough.jpg" src="videos/walkthrough.mp4"></video><p>程序驱动真实 Qt 控件与生产控制器，2 次实际 Codex 请求、2 次显式批准。录像来自本应用窗口，保留等待时间。</p></div><h2>三份真实导出成片</h2><div class="grid">'''+cards+'''</div><h2>播放器新增能力</h2><p>Qt 原生播放/暂停、停止归零、前后跳转、进度定位、倍速、音量与静音、循环、全屏及 Esc 退出全屏。状态来自实际媒体；不可播放时显示错误。</p><h2>演示时间线</h2><ol>'''+steps+'''</ol><h2>工程与证据</h2><p>可编辑工程：<code>'''+escape(report['project'])+'''</code></p><p><a href="evidence/demo.json">真实执行记录</a> · <a href="evidence/materials.json">人物素材来源</a> · <a href="manifest.json">媒体哈希与参数</a></p><p>保持整个文件夹一起移动即可离线播放。Hypit 原始 swap-host 使用图像/视频生成 Provider；本次未调用这些外部 Provider。原生应用已验证平台与依赖见仓库使用指南。</p></main></html>'''
        (stage/'index.html').write_text(html)
        manifest = {'verdict':'PASS','media':media,'materials':provenance,'project':report['project'],'realModelRequests':2,'source':str(args.demo),'recordingRetainsRealTiming':True}
        (stage/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
        # Every gallery URL is a controlled relative path checked before publish.
        for relative in ['videos/walkthrough.mp4','evidence/demo.json','evidence/materials.json','manifest.json']+[m['path'] for m in media[1:]]+[m['poster'] for m in media]:
            require((stage/relative).is_file(), '缺少展示页资源：'+relative)
        for child in stage.iterdir():
            target = output/child.name; require(not target.is_symlink(), '拒绝符号链接交付目标')
            if child.is_dir():
                target.mkdir(exist_ok=True)
                for source in child.iterdir(): source.replace(target/source.name)
            else: child.replace(target)
    print(json.dumps({'verdict':'PASS','output':str(output),'videos':len(media),'recordingSeconds':media[0]['durationSeconds']},ensure_ascii=False))

if __name__ == '__main__':
    main()
