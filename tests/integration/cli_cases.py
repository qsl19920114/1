#!/usr/bin/env python3
"""Offline CLI regression: QApplication's --session collision and exit semantics."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
exe = repo / 'build/app/qt-video-workbench'
session = repo / 'docs/evidence/m1/session_with_writable_fields.json'
env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
results = []
with tempfile.TemporaryDirectory(prefix='qvw 参数-') as folder:
    root = Path(folder)
    base = ['--selftest', '--log=' + str(root / 'log.jsonl')]
    cases = [
        ('session-spaces', ['--out', str(root / 'space.png'), '--session', str(session)], 0),
        ('session-equals', ['--out=' + str(root / 'equals.png'), '--session=' + str(session)], 0),
        ('missing-out', ['--session=' + str(session)], 2),
        ('unknown-option', ['--out=x.png', '--unexpected'], 1),
        ('missing-file', ['--out=' + str(root / 'bad.png'), '--session=' + str(root / 'missing.json')], 3),
        ('invalid-output', ['--out=/dev/null/cannot-save.png', '--session=' + str(session)], 3),
        ('invalid-config', ['--out=' + str(root / 'bad-config.png'), '--config=' + str(root / 'missing-config.json'), '--run=a', '--workspace=b', '--runtime=c'], 4),
    ]
    for name, arguments, expected in cases:
        run = subprocess.run([str(exe)] + base + arguments, capture_output=True, text=True, env=env, timeout=15)
        output = run.stdout + run.stderr
        result = {'case': name, 'expected': expected, 'actual': run.returncode, 'passed': run.returncode == expected}
        if expected == 0:
            result['loadedExpectedSession'] = 'OFFLINE revision= 1 writableFields= 2' in output
            result['passed'] &= result['loadedExpectedSession']
        results.append(result)
        if not result['passed']: print(output)
path = repo / 'docs/evidence/m1-live/cli-cases.json'
path.write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
print(path.read_text())
raise SystemExit(0 if all(case['passed'] for case in results) else 1)
