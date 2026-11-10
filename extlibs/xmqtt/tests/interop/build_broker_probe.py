"""Build the public-API broker probe using the repository's module closure."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[4]
    sys.path.insert(0, str(root / 'tools'))
    from build import _load_modules

    parser = argparse.ArgumentParser()
    parser.add_argument('--kind', choices=['modular', 'single', 'static', 'shared'], default='modular')
    parser.add_argument('--package-dir', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', help='GCC sanitizer list, for example address,undefined')
    parser.add_argument('--probe', choices=['ordinary', 'session', 'tls-session'], default='ordinary')
    args = parser.parse_args()
    if args.kind in ['static', 'shared'] and not args.package_dir:
        parser.error('--package-dir is required for a library consumer')
    suite = ('xmqtt_client_session_tls,xmqtt_client_tls_resume,tls_identity_rsa'
             if args.probe == 'tls-session' else
             'xmqtt_client_session,time' if args.probe == 'session' else 'xmqtt_client_tls_resume,tls_identity_rsa')
    modules = _load_modules(suite, [root / 'extlibs/xmqtt/config/modules.json'],
                            platform='windows' if os.name == 'nt' else 'linux')
    command = [args.compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
               '-I' + str(root / 'include'), '-I' + str(root / 'extlibs/xmqtt/include')]
    command += ['-D' + item for item in modules[4]]
    if os.name != 'nt':
        command += ['-pthread']
    if args.sanitize:
        if args.kind in ['static', 'shared']:
            parser.error('use modular/single so implementation objects are instrumented too')
        command += ['-g', '-fno-omit-frame-pointer', '-fsanitize=' + args.sanitize]
        if args.sanitize == 'thread':
            command += ['-Wno-error=tsan']
    here = Path(__file__).resolve().parent
    source = ('session_tls_broker_probe.c' if args.probe == 'tls-session' else
              'session_broker_probe.c' if args.probe == 'session' else 'broker_probe.c')
    if args.kind == 'single':
        command += [str(here / ('single_' + source))]
    else:
        command += [str(here / source)]
        if args.kind == 'modular':
            command += [str(root / source) for source in modules[0]]
        else:
            package = args.package_dir.resolve()
            suffix = '.a' if args.kind == 'static' else '.dll.a' if os.name == 'nt' else '.so'
            command += [str(package / ('lib' + product + suffix)) for product in ['xmqtt', 'xrt']]
            if args.kind == 'shared':
                command += ['-DXRT_USE_SHARED', '-DXMQTT_USE_SHARED']
                if os.name != 'nt':
                    command += ['-Xlinker', '-rpath', '-Xlinker', str(package)]
    command += ['-l' + item for item in modules[5]]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    command += ['-o', str(args.output.resolve())]
    subprocess.run(command, cwd=root, check=True, timeout=1800)
    if args.kind == 'shared' and os.name == 'nt':
        for product in ['xmqtt', 'xrt']:
            dll = package / (product + '.dll')
            if dll.resolve() != (args.output.parent / dll.name).resolve():
                shutil.copy2(dll, args.output.parent / dll.name)


if __name__ == '__main__':
    main()
