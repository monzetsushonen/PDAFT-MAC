#!/usr/bin/env python3
"""Build an experimental local launcher beside an existing P2 experiment."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TESTED_ADAPTER = '7ba7919ad19f4a5932ea5b417333da2fb52fa53c522fd8d3f1b49ca9853edbde'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--experiment-root', type=Path, required=True, help='Existing private P2 layout; see docs/COMPATIBILITY.md.')
    args = parser.parse_args()
    experiment = args.experiment_root.expanduser().resolve()
    if experiment == ROOT or ROOT in experiment.parents:
        parser.error('The experiment and build output must be outside the public checkout, on your external SSD.')
    for name in ['game/diva.exe', 'game/plugins/PDAFTAutoPlay.dva', 'game/plugins/TLAC.dva', 'game/plugins/components.ini', 'game-prefix', 'logs', 'tmp', 'cache/shaders', 'isolation.sb']:
        if not (experiment/name).exists():
            parser.error('Missing private experiment path: ' + name)
    wine = experiment.parent/'20261107-p1/runtime/MetalSharp-Wine.app/Contents/MacOS'
    if not all((wine/name).is_file() for name in ['wine', 'wineserver']):
        parser.error('The experimental launcher expects the sibling 20261107-p1 runtime layout.')
    adapter_hash = hashlib.sha256((experiment/'game/plugins/PDAFTAutoPlay.dva').read_bytes()).hexdigest()
    build = experiment/'launcher-build'
    build.mkdir(exist_ok=True)
    source = (ROOT/'native/Compatibility/P2Launcher.m').read_text()
    if source.count(TESTED_ADAPTER) != 1:
        raise RuntimeError('Launcher adapter pin template changed; inspect before building.')
    generated = build/'P2Launcher.m'
    generated.write_text(source.replace(TESTED_ADAPTER, adapter_hash))
    app = experiment/'PDAFT Compatibility Diagnostics.app'
    for name in ['Contents/MacOS', 'Contents/Resources']:
        (app/name).mkdir(parents=True, exist_ok=True)
    executable = app/'Contents/MacOS/PDAFTP2Launcher'
    def run(command):
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        return result.stdout
    build_log = run(['xcrun', 'clang', '-arch', 'arm64', '-fobjc-arc', '-Wno-deprecated-declarations', '-O2', '-mmacosx-version-min=13.0', generated, '-framework', 'AppKit', '-o', executable])
    (build/'build.log').write_text(build_log)
    (app/'Contents/Info.plist').write_bytes(plistlib.dumps({
        'CFBundleIdentifier': 'io.github.monzetsushonen.pdaft.compatibility.diagnostics',
        'CFBundleName': 'PDAFT Compatibility Diagnostics', 'CFBundleExecutable': executable.name,
        'CFBundlePackageType': 'APPL', 'CFBundleShortVersionString': '0.3.0',
        'CFBundleVersion': '3', 'LSMinimumSystemVersion': '13.0', 'NSHighResolutionCapable': True,
    }))
    (app/'Contents/Resources/README.txt').write_text('Local diagnostics only. Uses an existing external experiment; contains no game data or third-party runtime. Keep the game foreground until results. Video MV, HDR and MetalFX are disabled. Only Tell Your World EASY has full-song acceptance; other charts and HOLD need testing. Launcher button UI acceptance is pending.\n')
    run(['codesign', '--force', '--sign', '-', app])
    run(['codesign', '--verify', '--deep', '--strict', app])
    assert run(['lipo', '-archs', executable]).strip() == 'arm64'
    plan = json.loads(run([executable, '--verify']))
    assert plan['verificationError'] is None
    report = {'app': str(app), 'adapterSHA256': adapter_hash, 'architecture': 'arm64',
              'adHocSignatureVerified': True, 'preflight': 'PASS', 'gameResourcesBundled': False,
              'runtimeBundled': False, 'launcherButtonsAccepted': False}
    (build/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
