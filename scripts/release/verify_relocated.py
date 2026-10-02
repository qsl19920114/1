#!/usr/bin/env python3
"""Verify the deployed app from Chinese/spaced paths, with external Hypit."""
import argparse
import json
import os
import pathlib
import subprocess
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', default=str(REPO / '.workbench/release-macos-arm64/QtVideoWorkbench.app'))
    parser.add_argument('--hypit', default=str(REPO.parent / 'hypit'))
    args = parser.parse_args()
    source = pathlib.Path(args.app).resolve()
    hypit = pathlib.Path(args.hypit).resolve()
    if not source.is_dir() or not (hypit / 'bin/hypit.mjs').is_file():
        raise RuntimeError('Prepared .app and external Hypit Distribution required')
    evidence = REPO / 'docs/evidence/m6'
    evidence.mkdir(parents=True, exist_ok=True)
    root = pathlib.Path(tempfile.mkdtemp(prefix='qvw-m6-relocated-')).resolve() / '中文 空格目录'
    root.mkdir()
    app = root / 'Qt 视频工作台.app'
    subprocess.run(['/usr/bin/ditto', str(source), str(app)], check=True)
    (root / 'hypit').symlink_to(hypit, target_is_directory=True)
    (root / 'runtime').mkdir()
    (root / 'runtime/hypit').symlink_to(hypit, target_is_directory=True)
    (root / 'config').mkdir()
    (root / 'tools').mkdir()
    for name, target in [('custom-node','/opt/homebrew/bin/node'), ('custom-ffmpeg','/opt/homebrew/bin/ffmpeg'), ('custom-ffprobe','/opt/homebrew/bin/ffprobe')]:
        (root / 'tools' / name).symlink_to(target)
    env = os.environ.copy()
    env['PATH'] = '/usr/bin:/bin:/usr/sbin:/sbin'
    for key in ('QT_PLUGIN_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QTWEBENGINEPROCESS_PATH','DYLD_LIBRARY_PATH','DYLD_FRAMEWORK_PATH'):
        env.pop(key, None)
    executable = app / 'Contents/MacOS/qt-video-workbench'
    cases = []

    def config(name, distribution='runtime/hypit', version='0.2.10', tools=None, launcher='bin/hypit.mjs'):
        path = root / 'config' / (name + '.json')
        content = {'hypit':{'distributionPath':distribution,'version':version,'launcher':launcher}}
        if tools is not None:
            content['tools'] = tools
        path.write_text(json.dumps(content, ensure_ascii=False), encoding='utf-8')
        return path

    def verify(name, arguments, expected_success):
        report = evidence / (name + '.json')
        command = [str(executable), '--verify-startup', '--report-out', str(report), '--log', str(evidence / (name + '.jsonl'))] + [str(a) for a in arguments]
        with (evidence / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        data = json.loads(report.read_text())
        success = result.returncode == 0
        assert success == expected_success, (name, result.returncode, data)
        assert data['verdict'] == ('PASS' if expected_success else 'FAIL'), data
        assert data['cleanupStopped'] is True and data['screenshots'] is False, data
        if expected_success:
            assert all(data[key] is True for key in ('verificationSucceeded','snapshot','pageLoaded','compiledCompositionReady','imagesReady')), data
            assert pathlib.Path(data['templateDirectory']) == app / 'Contents/Resources/templates', data
            assert data['ownedStudioPid'] > 0, data
            try:
                os.kill(data['ownedStudioPid'], 0)
            except ProcessLookupError:
                pass
            else:
                raise AssertionError('Owned Studio still alive: ' + name)
        else:
            assert data['error'], data
        cases.append({'name':name, 'expectedSuccess':expected_success, 'exitCode':result.returncode, 'report':str(report.relative_to(REPO)), 'ownedStudioAbsent':True if expected_success else None})
        return data

    created = verify('relocated-default', ['--create-project',root/'默认 新建工程','--name','搬移发布验证'], True)
    explicit = config('explicit', tools={name:str(root/'tools'/('custom-'+name)) for name in ('node','ffmpeg','ffprobe')})
    reopened = verify('relocated-explicit', ['--config',explicit,'--project',created['project']], True)
    assert reopened['sourceFingerprint'] == created['sourceFingerprint']
    assert reopened['node'].endswith('/custom-node')
    shell = config('shell-default', launcher='hypit')
    shell_result = verify('relocated-shell', ['--config',shell,'--project',created['project']], True)
    assert shell_result['sourceFingerprint'] == created['sourceFingerprint']
    shell_explicit = config('shell-explicit', launcher='hypit', tools={name:str(root/'tools'/('custom-'+name)) for name in ('node','ffmpeg','ffprobe')})
    shell_pinned = verify('relocated-shell-explicit', ['--config',shell_explicit,'--project',created['project']], True)
    assert shell_pinned['sourceFingerprint'] == created['sourceFingerprint']
    assert shell_pinned['node'].endswith('/custom-node')
    missing = config('missing-hypit', distribution='runtime/not-installed')
    verify('missing-hypit', ['--config',missing,'--create-project',root/'不能创建-缺Hypit'], False)
    wrong = config('wrong-version', version='0.2.11')
    verify('wrong-version', ['--config',wrong,'--create-project',root/'不能创建-版本不符'], False)
    no_tool = config('missing-node', tools={'node':str(root/'tools/no-node')})
    verify('missing-node', ['--config',no_tool,'--create-project',root/'不能创建-缺Node'], False)
    for tool in ('ffmpeg','ffprobe'):
        absent = config('missing-'+tool, tools={tool:str(root/'tools'/('no-'+tool))})
        verify('missing-'+tool, ['--config',absent,'--create-project',root/('不能创建-缺'+tool)], False)
    summary = {'format':'qvw.relocation-verification@1','verdict':'PASS','initialPath':env['PATH'],'cwd':str(root),'app':str(app),'externalHypit':str(hypit),'externalHypitLayout':'prepared external Distribution symlink; not bundled','qtLinkOverrideEnvironmentRemoved':True,'cases':cases,'otherMachineVerified':False,'screenshots':False}
    (evidence/'relocation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False))

if __name__ == '__main__':
    main()
