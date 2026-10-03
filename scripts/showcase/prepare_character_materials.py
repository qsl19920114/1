#!/usr/bin/env python3
"""Reproduce three inspected portrait inputs from the existing official archive.

No downloads, image modifications, provider calls or upstream writes. Requires local
ffmpeg solely for full image decode validation. Paths in provenance are relative
to this repository; image paths in catalog.json are relative to the catalog.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import tarfile
import tempfile

MAX_IMAGE = 20 * 1024 * 1024
MAX_ARCHIVE = 600 * 1024 * 1024
MAX_EXPANDED = 2 * 1024 * 1024 * 1024
PREFIX = 'productions/explainer/assets/'
SELECTED = {
    PREFIX + 'presenter-a464d354ce0d.png': {
        'path': 'portrait-baseline.png', 'name': '白帽主持人',
        'sha256': 'b1aa6b53ab48b719246087b74a717253e4073ddca5a0f425e6545c9a4d06c7d1',
        'attribution': 'Hypit official complex-explainer host; upstream owner confirms the host image is AI-generated and supplied or commissioned for this project.',
        'attributionEvidence': 'examples/complex-explainer/productions/explainer/ASSET-PROVENANCE.md:6',
        'visualInspection': 'Single long-haired presenter in white sailor cap and outfit, holding microphone; face clearly visible.'},
    PREFIX + 'final-half/host.png': {
        'path': 'portrait-variant-1.png', 'name': '黑帽主持人',
        'sha256': '20cda1b21e6a7d819f0d136db4fbdb5088d0c4efc123c8ee76056dbdf87efed9',
        'attribution': 'Host image supplied in Hypit official complex-explainer archive and bound as ranking-host. The upstream provenance says the production host image is AI-generated; it does not separately certify this ranking-host file, so its exact generation history is unverified.',
        'attributionEvidence': 'examples/complex-explainer/productions/explainer/ASSET-PROVENANCE.md:6; examples/complex-explainer/productions/explainer/authors/assets.svml:63',
        'visualInspection': 'Single presenter with blunt bangs, black spiked cap and black outfit holding microphone; distinct face and wardrobe from baseline.'},
    PREFIX + 'final-half/drinking-illustration.png': {
        'path': 'portrait-variant-2.png', 'name': '像素风虚构人物',
        'sha256': '863b674f742db1d1cf8fd5a8bfce02e0f95678a0b51833f07d74a852de0cb6d0',
        'attribution': 'Hypit owner-supplied fictional drinking illustration; upstream provenance states it was generated with the built-in image tool on 2026-09-14. Reused unchanged here.',
        'attributionEvidence': 'examples/complex-explainer/productions/explainer/ASSET-PROVENANCE.md:7-15',
        'visualInspection': 'Single short-haired pixel-art character in coral shirt drinking water against pink background; clearly distinct illustrated character.'},
}
EVIDENCE_FILES = [
    'examples/complex-explainer/README.md',
    'examples/complex-explainer/productions/explainer/ASSET-PROVENANCE.md',
    'examples/complex-explainer/productions/explainer/authors/assets.svml',
    'examples/ranking-football/swap-host.svml',
    'examples/ranking-football/swap-host.svrun',
    'examples/ranking-football/hypit.runtime.json',
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def safe_output(path, distribution):
    path = Path(os.path.abspath(path))
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError(f'Refusing symlink output path: {path}')
    if path == distribution or distribution in path.parents:
        raise ValueError('Output must not modify the external Hypit distribution')
    return path


def read_selected(path, names):
    """Read bounded allowlisted bytes only; never use tar extraction paths."""
    if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size <= MAX_ARCHIVE:
        raise ValueError('Archive must be a bounded regular file')
    result = {}
    total = 0
    with tarfile.open(path, 'r|gz') as archive:
        for count, member in enumerate(archive, 1):
            total += member.size
            if count > 20000 or total > MAX_EXPANDED or member.size < 0:
                raise ValueError('Archive expansion exceeds bounds')
            name = member.name[2:] if member.name.startswith('./') else member.name
            if member.isdir() and name in ('', '.'):
                continue
            normalized = name.rstrip('/') if member.isdir() else name
            if ('\\' in normalized or '\0' in normalized or PurePosixPath(normalized).is_absolute()
                    or any(part in ('', '.', '..') for part in normalized.split('/'))):
                raise ValueError(f'Unsafe archive member: {member.name}')
            if normalized not in names:
                continue
            if (not member.isfile() or member.issym() or member.islnk() or member.sparse
                    or not 0 < member.size <= MAX_IMAGE):
                raise ValueError(f'Selected member must be a bounded regular image: {name}')
            if normalized in result:
                raise ValueError(f'Duplicate selected archive member: {name}')
            stream = archive.extractfile(member)
            if stream is None:
                raise ValueError(f'Cannot read selected member: {name}')
            with stream:
                data = stream.read(MAX_IMAGE + 1)
            if len(data) != member.size:
                raise ValueError(f'Selected member size mismatch: {name}')
            result[normalized] = data
    if result.keys() != names:
        raise ValueError(f'Missing selected images: {sorted(names - result.keys())}')
    return result


def write_bytes(path, data, distribution):
    safe_output(path, distribution)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(dir=path.parent, prefix=path.name + '.')
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def main():
    repository = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--distribution', type=Path, default=repository.parent / 'hypit')
    parser.add_argument('--archive', type=Path, default=repository / '.workbench/showcase/downloads/media.tar.gz')
    parser.add_argument('--output', type=Path, default=repository / '.workbench/character-showcase/materials')
    parser.add_argument('--evidence', type=Path, default=repository / 'docs/evidence/m14/materials/provenance.json')
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    args = parser.parse_args()
    distribution = args.distribution.resolve()
    output = safe_output(args.output, distribution)
    evidence = safe_output(args.evidence, distribution)
    if not args.ffmpeg:
        parser.error('Local ffmpeg required for image decode verification')
    lock = json.loads((repository / 'config/version-lock.json').read_text())['hypit']
    commit = subprocess.run(['git', '-C', str(distribution), 'rev-parse', 'HEAD'],
                            check=True, capture_output=True, text=True).stdout.strip()
    if commit != lock['gitCommit']:
        raise ValueError('Hypit checkout does not match config/version-lock.json')
    source_hashes = {path: sha256(distribution / path) for path in EVIDENCE_FILES}
    for path in EVIDENCE_FILES:
        pinned = subprocess.run(['git', '-C', str(distribution), 'show', f'{commit}:{path}'],
                                check=True, capture_output=True).stdout
        if hashlib.sha256(pinned).hexdigest() != source_hashes[path]:
            raise ValueError(f'Upstream evidence differs from pinned commit: {path}')
    readme = (distribution / EVIDENCE_FILES[0]).read_text()
    urls = set(re.findall(r'https://storage\.googleapis\.com/hypit-public-assets/[^\s)]+/media\.tar\.gz', readme))
    if len(urls) != 1:
        raise ValueError('Expected one archive URL in pinned README')
    source_url = urls.pop()
    if args.archive.is_symlink() or not args.archive.is_file() or not 0 < args.archive.stat().st_size <= MAX_ARCHIVE:
        raise ValueError('Archive must be a bounded regular file')
    archive_hash = sha256(args.archive)
    data = read_selected(args.archive, set(SELECTED))
    # Validate all bytes before publishing any outputs.
    samples = []
    with tempfile.TemporaryDirectory(prefix='character-material-decode-') as temporary:
        for member, metadata in SELECTED.items():
            contents = data[member]
            if hashlib.sha256(contents).hexdigest() != metadata['sha256']:
                raise ValueError(f'Image differs from visually inspected input: {member}')
            if contents[:16] != b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR':
                raise ValueError(f'Expected PNG image: {member}')
            width, height = struct.unpack('>II', contents[16:24])
            if not (0 < width <= 8192 and 0 < height <= 8192 and width * height <= 32000000):
                raise ValueError(f'Image dimensions exceed bounds: {member}')
            probe = Path(temporary) / metadata['path']
            probe.write_bytes(contents)
            decoded = subprocess.run([args.ffmpeg, '-nostdin', '-v', 'error', '-xerror', '-i', str(probe),
                                      '-map', '0:v:0', '-f', 'null', '-'],
                                     capture_output=True, text=True, check=True, timeout=60)
            samples.append({**metadata, 'sourceUrl': source_url, 'archiveMember': './' + member,
                            'sizeBytes': len(contents), 'width': width, 'height': height,
                            'mimeType': 'image/png', 'transformation': 'none; exact original bytes',
                            'validation': {'fullDecodeExitCode': decoded.returncode,
                                           'fullDecodeStderr': decoded.stderr}})
    if sha256(args.archive) != archive_hash:
        raise ValueError('Archive changed during preparation')
    for member, metadata in SELECTED.items():
        write_bytes(output / metadata['path'], data[member], distribution)
    catalog = {'schemaVersion': 1, 'samples': samples}
    provenance = {
        'schemaVersion': 1, 'status': 'verified', 'hypitVersion': lock['version'], 'upstreamCommit': commit,
        'sourceUrl': source_url, 'archiveSha256': archive_hash,
        'archivePath': os.path.relpath(args.archive, repository),
        'outputDirectory': os.path.relpath(output, repository),
        'sourceEvidenceSha256': source_hashes,
        'scope': 'Three reused official still images for local template material replacement. No new images, voices or identity-regenerated performances were generated by this workbench.',
        'visualInspection': {'method': 'view_image of original PNGs before inclusion',
                             'date': '2026-10-03',
                             'rejectedCandidates': 'montage/01.png through montage/04.png are multi-image collages with mixed sources, not clean single portraits.'},
        'providerBoundary': {
            'example': 'examples/ranking-football/swap-host.svrun:4-5',
            'imageGeneration': 'examples/ranking-football/swap-host.svml:8,39-40 declares @hypit/gpt-image and gpt:Image',
            'performanceGeneration': 'examples/ranking-football/swap-host.svml:9,133-144 declares @hypit/seedance and seedance:ReferenceVideo',
            'externalEndpoint': 'examples/ranking-football/hypit.runtime.json:11-20 declares @hypit/provider-hypihub at https://hypit.ai with platform credential hypihub.oauth',
            'executed': False,
            'limitation': 'Official swap-host regeneration requires configured external generation services. This showcase only substitutes existing image bindings in a locally rendered template; it does not demonstrate new speaking performance, voice replacement or video face replacement.'},
        'materials': [{**sample, 'repositoryPath': os.path.relpath(output / sample['path'], repository)} for sample in samples],
    }
    write_bytes(output / 'catalog.json', json_bytes(catalog), distribution)
    write_bytes(output / 'provenance.json', json_bytes(provenance), distribution)
    write_bytes(evidence, json_bytes(provenance), distribution)
    for sample in samples:
        print(f"Verified {output / sample['path']}: {sample['width']}x{sample['height']}, {sample['sha256']}")
    print(f'Provenance: {evidence}')


if __name__ == '__main__':
    main()
