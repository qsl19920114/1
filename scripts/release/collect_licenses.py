#!/usr/bin/env python3
"""Collect pinned attribution inputs; no runtime binaries or upstream vendoring."""
import concurrent.futures
import hashlib
import fnmatch
import json
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.parse

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = REPO / '.workbench' / 'license-inputs'
SOURCES = json.loads((REPO / 'docs/m6-license-sources.json').read_text())
CACHED_SOURCES = {r['file']: r.get('source') for r in json.loads((OUT / 'sources.json').read_text()).get('records', [])} if (OUT / 'sources.json').is_file() else {}

def fetch(pair):
    relative, url = pair
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or parsed.hostname not in {'doc.qt.io', 'raw.githubusercontent.com', 'www.gnu.org'}:
        raise ValueError('Unexpected attribution source')
    target = OUT / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.is_file():
        temporary = target.with_suffix(target.suffix + '.part')
        subprocess.run(['/usr/bin/curl', '--fail', '--location', '--silent', '--show-error',
                        '--max-time', '35', '--retry', '1', '--max-filesize', '8388608',
                        '--output', str(temporary), url], check=True)
        if not temporary.is_file() or temporary.stat().st_size < 100:
            raise ValueError('Empty attribution: ' + relative)
        temporary.replace(target)
    data = target.read_bytes()
    if relative.startswith('Qt/attributions/') and b'6.11.2' not in data:
        raise ValueError('Qt attribution does not identify pinned version: ' + relative)
    return {'file': relative, 'source': CACHED_SOURCES.get(relative) or url, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

def copy_local(path, relative):
    target = OUT / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, target)
    data = target.read_bytes()
    return {'file': relative, 'localSource': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    urls = [('Qt/attributions/index.html', SOURCES['qt']['indexUrl'])]
    urls += [('Qt/attributions/' + pathlib.PurePosixPath(urllib.parse.urlparse(u).path).name, u)
             for u in SOURCES['qt']['attributionUrls']]
    urls += [('FreeType/' + f['path'], f['url']) for f in SOURCES['freetype']['files']]
    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        for record in pool.map(fetch, urls):
            records.append(record)
    # The GNU site may be unavailable; Qt's official pinned SPDX text is an
    # alternate source of the same GFDL 1.3 full text, recorded explicitly.
    fdl_urls = [SOURCES['documentationLicense']['fullTextUrl'],
                'https://raw.githubusercontent.com/qt/qtbase/v6.11.2/LICENSES/GFDL-1.3-no-invariants-only.txt']
    for url in fdl_urls:
        try:
            record = fetch(('GNU/GFDL-1.3.txt', url))
            if b'GNU Free Documentation License' not in (OUT / record['file']).read_bytes():
                raise ValueError('GFDL full text not present')
            records.append(record)
            break
        except (ValueError, subprocess.CalledProcessError):
            (OUT / 'GNU/GFDL-1.3.txt').unlink(missing_ok=True)
    else:
        raise RuntimeError('Could not collect full GFDL text')
    for source, destination in [
            (REPO.parent / 'hypit/LICENSE', 'Hypit/0.2.10/LICENSE'),
            (pathlib.Path('/opt/homebrew/opt/ffmpeg/COPYING.LGPLv3'), 'GNU/LGPL-3.0.txt'),
            (pathlib.Path('/opt/homebrew/opt/ffmpeg/COPYING.GPLv3'), 'GNU/GPL-3.0.txt'),
            (pathlib.Path('/opt/homebrew/opt/qtwebengine/LICENSE.Chromium'), 'Qt/LICENSE.Chromium')]:
        records.append(copy_local(source, destination))
    # Candidate dependencies were derived from the research Mach-O inventory.
    # Final package audit identifies its actual binary closure separately.
    text = (REPO / 'docs/M6_LICENSE_INPUTS.md').read_text()
    formulas = set(re.findall(r'/opt/homebrew/opt/([A-Za-z0-9_+-]+)', text))
    formulas.update(re.findall(r'^\| ([A-Za-z0-9@_+-]+) [0-9]', text, re.MULTILINE))
    formulas.update({'qtbase', 'qtwebengine', 'qtdeclarative', 'qtwebchannel', 'qtpositioning', 'qtsvg', 'qtshadertools', 'qtvirtualkeyboard', 'qtserialport'})
    for formula in sorted(formulas):
        root = pathlib.Path('/opt/homebrew/opt') / formula
        if not root.is_dir():
            continue
        version = root.resolve().name
        candidates = set()
        for pattern in ('LICENSE*', 'LICENCE*', 'COPYING*', 'NOTICE*', 'AUTHORS*', 'Copyright*', 'COPYRIGHT*', 'LGPL*', 'license.html', 'README.ijg', 'sbom.spdx.json'):
            candidates.update(root.glob(pattern))
        candidates.update(root.glob('share/qt/sbom/*.spdx'))
        for path in root.glob('share/doc/**/*'):
            if any(fnmatch.fnmatch(path.name.lower(), pattern) for pattern in ('license*','licence*','copying*','notice*','copyright*','readme.ijg')):
                candidates.add(path)
        for path in sorted(candidates):
            if path.is_file() and path.stat().st_size <= 8 * 1024 * 1024:
                records.append(copy_local(path, 'Homebrew/' + formula + '/' + version + '/' + str(path.relative_to(root))))
    (OUT / 'sources.json').write_text(json.dumps({'format': 'qvw.license-materials@1',
        'qtVersion': '6.11.2', 'qtAttributionPages': len(SOURCES['qt']['attributionUrls']),
        'records': records, 'scope': 'Attribution/notice inputs and SBOM; not an authorization or complete source-distribution audit'}, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(OUT), 'files': len(records), 'qtAttributionPages': 126}))

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
