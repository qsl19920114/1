#!/usr/bin/env python3
"""Build a Qt bundle, audit deployed links, sign locally and archive it."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import stat
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
MAGIC = {b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}

def run(arguments):
    subprocess.run([str(a) for a in arguments], check=True)

def capture(arguments):
    return subprocess.check_output([str(a) for a in arguments], text=True, stderr=subprocess.STDOUT)

def binaries(app):
    result = []
    for path in app.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open('rb') as file:
            if file.read(4) in MAGIC:
                result.append(path)
    return sorted(result)

def inspect(path):
    loads = capture(['/usr/bin/otool', '-l', path])
    rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', loads)
    dependencies = [line.strip().split(' (compatibility version', 1)[0]
                    for line in capture(['/usr/bin/otool', '-L', path]).splitlines()[1:]]
    return dependencies, rpaths, loads

def complete_dependencies(app, qt_prefix):
    """Repair Homebrew umbrella-prefix omissions, then close actual links."""
    frameworks = app / 'Contents/Frameworks'
    changes = []
    for iteration in range(24):
        copied = False
        for binary in binaries(app):
            dependencies, rpaths, loads = inspect(binary)
            commands = []
            own_id = re.search(r'cmd LC_ID_DYLIB\s+cmdsize \d+\s+name (.*?) \(offset', loads)
            if own_id:
                relative = binary.relative_to(frameworks)
                deployed_id = '@rpath/' + str(relative)
                if own_id.group(1) != deployed_id:
                    commands += ['-id', deployed_id]
            for dependency in dependencies:
                if dependency.startswith(('/System/Library/', '/usr/lib/')):
                    continue
                if own_id and dependency == own_id.group(1):
                    continue
                source = None
                if dependency.startswith('@rpath/'):
                    suffix = dependency[len('@rpath/'):]
                    target = frameworks / suffix
                    if not target.exists():
                        source = pathlib.Path(qt_prefix) / 'lib' / suffix
                elif dependency.startswith('@executable_path/../Frameworks/'):
                    # These libraries are shared by the main app and nested
                    # WebEngine helper, whose executable directories differ.
                    suffix = dependency[len('@executable_path/../Frameworks/'):]
                    target = frameworks / suffix
                    source = pathlib.Path(qt_prefix) / 'lib' / suffix
                    commands += ['-change', dependency, '@rpath/' + suffix]
                elif dependency.startswith('/'):
                    if not dependency.startswith(('/opt/homebrew/', '/usr/local/opt/', '/usr/local/Cellar/')):
                        raise RuntimeError('Unexpected non-system dependency: ' + dependency)
                    source = pathlib.Path(dependency)
                    match = re.search(r'([^/]+\.framework/.*)$', dependency)
                    suffix = match.group(1) if match else source.name
                    target = frameworks / suffix
                    commands += ['-change', dependency, '@rpath/' + suffix]
                else:
                    continue
                if not target.exists():
                    if source is None or not source.is_file():
                        raise RuntimeError('Missing deployment source: ' + dependency)
                    match = re.search(r'^(.*?\.framework)/', str(source))
                    if match:
                        framework_source = pathlib.Path(match.group(1))
                        run(['/usr/bin/ditto', framework_source, frameworks / framework_source.name])
                    else:
                        shutil.copy2(source, target)
                    changes.append({'source':str(source),'deployed':str(target.relative_to(app))})
                    copied = True
            loader_relative = os.path.relpath(frameworks, binary.parent)
            loader_rpath = '@loader_path' + ('' if loader_relative == '.' else '/' + loader_relative)
            if loader_rpath not in rpaths:
                commands += ['-add_rpath', loader_rpath]
            if commands:
                binary.chmod(binary.stat().st_mode | stat.S_IWUSR)
                run(['/usr/bin/install_name_tool'] + commands + [binary])
        if not copied:
            return changes
    raise RuntimeError('Deployment closure exceeded 24 passes')

def audit(app, fix=False):
    entries = []
    all_binaries = binaries(app)
    for binary in all_binaries:
        dependencies, rpaths, loads = inspect(binary)
        for path in rpaths:
            if path.startswith('/') and not path.startswith(('/System/', '/usr/lib')):
                if not fix:
                    raise RuntimeError('External RPATH: ' + str(binary) + ': ' + path)
                binary.chmod(binary.stat().st_mode | stat.S_IWUSR)
                run(['/usr/bin/install_name_tool', '-delete_rpath', path, binary])
        if fix:
            dependencies, rpaths, loads = inspect(binary)
        for dependency in dependencies:
            if dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if not dependency.startswith(('@rpath/', '@loader_path/', '@executable_path/')):
                raise RuntimeError('External deployed dependency: ' + str(binary) + ': ' + dependency)
            if dependency.startswith('@rpath/'):
                suffix = dependency[len('@rpath/'):]
                if not (app / 'Contents/Frameworks' / suffix).exists() and not any(str(p).endswith('/' + suffix) for p in all_binaries):
                    raise RuntimeError('Missing deployed dependency: ' + dependency)
            elif dependency.startswith('@loader_path/'):
                if not (binary.parent / dependency[len('@loader_path/'):]).exists():
                    raise RuntimeError('Missing loader-relative dependency: ' + dependency)
            elif dependency.startswith('@executable_path/'):
                if binary.is_relative_to(app / 'Contents/Frameworks'):
                    raise RuntimeError('Shared library depends on executable context: ' + dependency)
                executable_directory = app / 'Contents/MacOS'
                for parent in binary.parents:
                    if parent.name.endswith('.app'):
                        executable_directory = parent / 'Contents/MacOS'
                        break
                if not (executable_directory / dependency[len('@executable_path/'):]).exists():
                    raise RuntimeError('Missing executable-relative dependency: ' + dependency)
        entries.append({'file': str(binary.relative_to(app)), 'dependencies': dependencies, 'rpaths': rpaths,
                        'buildVersion': re.findall(r'minos (\S+)', loads), 'architectures': capture(['/usr/bin/lipo', '-archs', binary]).strip()})
    return entries

def sign(app):
    for binary in binaries(app):
        if binary.name != 'QtWebEngineProcess':
            run(['/usr/bin/codesign', '--force', '--sign', '-', binary])
    helpers = sorted(app.rglob('QtWebEngineProcess.app'))
    if not helpers:
        raise RuntimeError('QtWebEngine helper missing')
    for helper in helpers:
        entitlements = helper / 'Contents/Resources/QtWebEngineProcess.entitlements'
        if not entitlements.is_file():
            raise RuntimeError('WebEngine entitlements missing')
        run(['/usr/bin/codesign', '--force', '--sign', '-', '--entitlements', entitlements, helper])
    for framework in sorted((app / 'Contents/Frameworks').glob('*.framework')):
        run(['/usr/bin/codesign', '--force', '--sign', '-', framework])
    run(['/usr/bin/codesign', '--force', '--sign', '-', app])
    run(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', app])
    helper_entitlements = capture(['/usr/bin/codesign', '-d', '--entitlements', '-', helpers[0]])
    for required in ('allow-jit', 'allow-unsigned-executable-memory', 'disable-library-validation'):
        if required not in helper_entitlements:
            raise RuntimeError('Missing helper entitlement: ' + required)
    return helper_entitlements

def main():
    if sys.platform != 'darwin':
        raise RuntimeError('This release target is macOS only')
    match = re.search(r'project\(QtVideoWorkbench VERSION ([0-9]+\.[0-9]+\.[0-9]+)', (REPO / 'CMakeLists.txt').read_text())
    if not match:
        raise RuntimeError('Application version missing')
    app_version = match.group(1)
    parser = argparse.ArgumentParser()
    parser.add_argument('--qt-prefix', default='/opt/homebrew/opt/qt')
    parser.add_argument('--output', default=str(REPO / ('.workbench/release-macos-arm64-' + app_version)))
    parser.add_argument('--repair-existing', action='store_true', help='Repair links/signatures of this script\'s existing deployment of the current version without rebuilding application code')
    args = parser.parse_args()
    output = pathlib.Path(args.output).absolute()
    if not output.parent.resolve().is_relative_to(REPO) or output.is_symlink() or (output.exists() and not args.repair_existing):
        raise RuntimeError('Release output must be a new directory inside repository')
    if args.repair_existing:
        marker = output / 'release.json'
        if not marker.is_file() or json.loads(marker.read_text()).get('format') != 'qvw.release-artifact@1' or json.loads(marker.read_text()).get('version') != 'Qt Video Workbench ' + app_version:
            raise RuntimeError('Repair requires this script\'s release marker for the current version')
    license_inputs = REPO / '.workbench/license-inputs'
    if not (license_inputs / 'sources.json').is_file():
        raise RuntimeError('Run collect_licenses.py first')
    build = REPO / 'build-release'
    app = output / 'QtVideoWorkbench.app'
    if not args.repair_existing:
        run(['cmake', '-S', REPO, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
             '-DQVW_BUILD_PROBES=OFF', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0', '-DCMAKE_PREFIX_PATH=' + args.qt_prefix])
        run(['cmake', '--build', build, '--parallel', '4'])
        output.mkdir(parents=True)
        run(['/usr/bin/ditto', build / 'app/qt-video-workbench.app', app])
    resources = app / 'Contents/Resources'
    (resources / 'config').mkdir(exist_ok=True)
    shutil.copy2(REPO / 'config/bundle.example.json', resources / 'config/version-lock.json')
    shutil.copytree(license_inputs, resources / 'licenses', dirs_exist_ok=args.repair_existing)
    for name in ('USER_GUIDE.md', 'THIRD_PARTY.md'):
        source = REPO / 'docs' / name
        if not source.is_file():
            raise RuntimeError('Release guide missing: ' + name)
        shutil.copy2(source, resources / name)
        shutil.copy2(source, output / name)
    if not args.repair_existing:
        run([pathlib.Path(args.qt_prefix) / 'bin/macdeployqt', app, '-verbose=2', '-always-overwrite', '-codesign=-'])
    additional_dependencies = complete_dependencies(app, args.qt_prefix)
    if args.repair_existing and (output / 'deployment-repairs.json').is_file():
        additional_dependencies = json.loads((output / 'deployment-repairs.json').read_text()) + additional_dependencies
    (output / 'deployment-repairs.json').write_text(json.dumps(additional_dependencies,indent=2)+'\n')
    entries = audit(app, fix=True)
    entitlements = sign(app)
    entries = audit(app)
    if not (app / 'Contents/PlugIns/platforms/libqcocoa.dylib').is_file():
        raise RuntimeError('Cocoa platform plugin missing')
    framework = app / 'Contents/Frameworks/QtWebEngineCore.framework'
    for name in ('icudtl.dat', 'qtwebengine_resources.pak', 'qtwebengine_resources_100p.pak',
                 'qtwebengine_resources_200p.pak', 'v8_context_snapshot.arm64.bin'):
        if not any(framework.rglob(name)):
            raise RuntimeError('WebEngine resource missing: ' + name)
    (output / 'link-audit.json').write_text(json.dumps({'format':'qvw.bundle-link-audit@1','verdict':'PASS','binaries':entries},indent=2)+'\n')
    (output / 'helper-entitlements.txt').write_text(entitlements)
    version = capture([app / 'Contents/MacOS/qt-video-workbench', '--version']).strip()
    archive = output / ('FrameLab-' + app_version + '-macOS-arm64.zip')
    if archive.exists():
        archive.replace(archive.with_suffix('.previous.zip'))
    run(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, archive])
    digest = hashlib.sha256()
    with archive.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(chunk)
    checksum = digest.hexdigest()
    (output / 'SHA256SUMS.txt').write_text(checksum + '  ' + archive.name + '\n')
    summary = {'format':'qvw.release-artifact@1','version':version,'app':str(app),'archive':str(archive),
               'sha256':checksum,'signing':'ad-hoc; not notarized','platform':'macOS arm64; minimum 15.0',
               'externalDependencies':['Hypit 0.2.10','Node >=22.15','FFmpeg/ffprobe','Runtime-selected rendering browser'],
               'machOBinaries':len(entries)}
    (output / 'release.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary))

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
