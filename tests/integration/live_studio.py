#!/usr/bin/env python3
"""Real M1 gate. Only local providers; separate workspace with Chinese/spaces.
Run from any cwd: python3 tests/integration/live_studio.py
Requires native window access and local listening sockets (not a restricted sandbox).
"""
from pathlib import Path
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
lock = json.loads((repo / 'config/version-lock.json').read_text())
hypit = (repo / lock['hypit']['distributionPath']).resolve()
evidence = repo / 'docs/evidence/m1-live'
evidence.mkdir(parents=True, exist_ok=True)
(repo / '.workbench').mkdir(exist_ok=True)
workspace = Path(tempfile.mkdtemp(prefix='真实 工程-', dir=repo / '.workbench'))
source = hypit / 'examples/semantic-composition'
for name in ('chat.svml', 'chat.svrun', 'chat.svs', 'hypit.runtime.json'):
    shutil.copy2(source / name, workspace / name)
profile = json.loads((workspace / 'hypit.runtime.json').read_text())
assert {x['use'] for x in profile['endpoints'].values()} <= {
    '@hypit/provider-media-local', '@hypit/provider-hyperframes-local'}
package = workspace / 'packages/chat-scene'
shutil.copytree(source / 'packages/chat-scene', package,
                ignore=shutil.ignore_patterns('node_modules', '.hypit'))
for name in ('activation-studio.js', 'studio.js'):
    shutil.copy2(repo / 'tests/fixtures/writable-probe' / name, package / 'src' / name)
manifest = json.loads((package / 'package.json').read_text())
manifest['hypit']['activation'] = './src/activation-studio.js'
(package / 'package.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2))
(package / 'node_modules/@hypit').mkdir(parents=True, exist_ok=True)
(package / 'node_modules/@hypit/hypit').symlink_to(hypit, target_is_directory=True)

# Occupy both localhost address families in a separate, harness-owned process.
# Studio must find another port; closing Qt must leave this process untouched.
helper_code = '''import socket,time
s6=socket.socket(socket.AF_INET6); s6.setsockopt(socket.IPPROTO_IPV6,socket.IPV6_V6ONLY,1)
s6.bind(("::1",0)); port=s6.getsockname()[1]; s6.listen()
s4=socket.socket(); s4.bind(("127.0.0.1",port)); s4.listen()
print(port,flush=True)
time.sleep(180)
'''
helper = subprocess.Popen([sys.executable, '-u', '-c', helper_code], stdout=subprocess.PIPE, text=True)
owned_pid = None
report = {'workspace': str(workspace), 'hypit': str(hypit), 'localProvidersOnly': True}
try:
    requested_port = int(helper.stdout.readline().strip())
    command = [str(repo / 'build/app/qt-video-workbench'), '--selftest',
               '--config=' + str(repo / 'config/version-lock.json'),
               '--out=' + str(evidence / 'live-studio.png'),
               '--log=' + str(evidence / 'workbench.jsonl'),
               '--session-out=' + str(evidence / 'session.json'),
               '--workspace=' + str(workspace), '--run=' + str(workspace / 'chat.svrun'),
               '--runtime=' + str(workspace / 'hypit.runtime.json'), '--port=' + str(requested_port)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    output = result.stdout + result.stderr
    (evidence / 'live-run.log').write_text(output)
    report.update(exitCode=result.returncode, requestedPort=requested_port)
    match = re.search(r'Studio 已就绪：(http://[^\s]+?)（PID (\d+)）', output)
    if match:
        report['actualUrl'] = match[1]; owned_pid = int(match[2]); report['studioPid'] = owned_pid
    assert result.returncode == 0, output[-6000:]
    assert match, 'missing actual URL / PID evidence'
    actual_port = int(re.search(r':(\d+)/', match[1])[1])
    assert actual_port != requested_port, 'port fallback was not exercised'
    report['portFallbackVerified'] = True
    snapshot = json.loads((evidence / 'session.json').read_text())
    report['revision'] = snapshot['revision']
    report['tracks'] = len(snapshot['tracks'])
    report['writableFields'] = sum('edit' in field for track in snapshot['tracks'] for clip in track['clips'] for field in clip['inspector'])
    assert report['tracks'] > 0 and report['writableFields'] == 2
    assert 'PREVIEW compiled composition present=true' in output
    report['compiledPreviewPresent'] = True
    assert (evidence / 'live-studio.png').stat().st_size > 10000
    group_alive = True
    for _ in range(20):
        try: os.killpg(owned_pid, 0)
        except ProcessLookupError: group_alive = False; break
        time.sleep(0.1)
    report['ownedProcessGroupStopped'] = not group_alive
    report['unrelatedProcessPreserved'] = helper.poll() is None
    assert report['ownedProcessGroupStopped'] and report['unrelatedProcessPreserved']
    report['verdict'] = 'PASS'
except BaseException as error:
    report['verdict'] = 'FAIL'; report['error'] = str(error)
    raise
finally:
    (evidence / 'e2e.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if owned_pid and report.get('verdict') != 'PASS':
        try: os.killpg(owned_pid, signal.SIGTERM)
        except ProcessLookupError: pass
    helper.terminate()
    helper.wait(timeout=5)
