#!/usr/bin/env python3
"""Build the offline input adapter without bundling or installing dependencies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'native' / 'Compatibility'


def external_output(value):
    output = Path(value).expanduser().resolve()
    if output == ROOT or ROOT in output.parents:
        raise argparse.ArgumentTypeError('Choose a build directory outside the source checkout, on your external SSD.')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=external_output)
    parser.add_argument('--checks-only', action='store_true', help='Run 55 native input checks; no Wine/MSVC required.')
    parser.add_argument('--wine', type=Path, help='Path to the ARM64 Wine executable.')
    parser.add_argument('--compiler-dir', type=Path, help='User-provided ARM64 MSVC lib.exe/link.exe and their DLLs.')
    parser.add_argument('--compiler-prefix', type=Path, help='Existing dedicated build prefix; do not use a game prefix.')
    args = parser.parse_args()
    if not args.checks_only:
        for name in ['wine', 'compiler_dir', 'compiler_prefix']:
            if getattr(args, name) is None:
                parser.error('--' + name.replace('_', '-') + ' is required unless --checks-only is used')
        for path in [args.wine, args.compiler_prefix, args.compiler_dir/'lib.exe', args.compiler_dir/'link.exe']:
            if not path.exists():
                parser.error('Missing dependency: ' + str(path))
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    logs = output/'logs'
    logs.mkdir(exist_ok=True)

    def run(command, label, env=None):
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True, env=env, timeout=90)
        (logs/(label+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError(label + ': ' + result.stdout + result.stderr)
        return result.stdout.strip()

    checks = []
    for name in ['input-check', 'scheduler-check']:
        run(['xcrun', 'clang', '-arch', 'arm64', '-O2', SOURCE/(name+'.c'), '-o', output/name], name+'-build')
        checks.append(run([output/name], name))
    report = {'checks': checks, 'checksOnly': args.checks_only, 'installed': False}
    if not args.checks_only:
        obj = output/'autoplay.obj'
        run(['xcrun', 'clang', '-target', 'x86_64-pc-windows-msvc', '-std=c11', '-O2', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fms-extensions', '-c', SOURCE/'PDAFTAutoPlay.c', '-o', obj], 'adapter-compile')
        env = os.environ.copy()
        for key in ['DYLD_INSERT_LIBRARIES', 'DYLD_LIBRARY_PATH', 'DYLD_FRAMEWORK_PATH']:
            env.pop(key, None)
        (output/'tmp').mkdir(exist_ok=True)
        env.update(WINEPREFIX=str(args.compiler_prefix.resolve()), TMPDIR=str(output/'tmp')+'/', WINEDEBUG='-all')
        win = lambda path: 'Z:'+str(path.resolve()).replace('/', '\\')
        library = output/'kernel32.lib'
        run([args.wine, args.compiler_dir/'lib.exe', '/nologo', '/machine:x64', '/def:'+win(SOURCE/'kernel32.def'), '/out:'+win(library)], 'import-library', env)
        binary = output/'PDAFTAutoPlay.dva'
        run([args.wine, args.compiler_dir/'link.exe', '/nologo', '/machine:x64', '/dll', '/entry:DllMain', '/nodefaultlib', '/dynamicbase', '/nxcompat', '/out:'+win(binary), win(obj), win(library)], 'adapter-link', env)
        report.update(candidate=str(binary), sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                      note='Stop the game before installing. PE timestamps can change hashes. Rebuild the test launcher after installing a candidate.')
    (output/'build-result.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
