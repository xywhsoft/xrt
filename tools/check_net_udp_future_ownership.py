"""Exact native UDP Future graphs; offline contracts and own literal loopback.

Explicit allowlist only: never run the old general UDP Future fixture, which
also contains OS error-queue traffic. No public endpoints, DNS, ICMP, scans,
malformed messages, fuzzing or security acceptance. Not managed xlang or
shipping-library acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', action='append', required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    header, out = args.header.resolve(), args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    compilers = [Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    fixtures = [root/'tests/network'/name for name in (
        'test_net_udp_future_ownership.c', 'test_net_udp_future_threads.c', 'test_net_udp_future_oom.c')]
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files = {Path(__file__).resolve(), header, root/'tests/test.h', *fixtures, *compilers}
    files.update(p for p in (root/'include').rglob('*.h'))
    inputs = {str(p): sha(p) for p in sorted(files)}
    out.mkdir(parents=True)
    report = dict(Complete=False, Inputs=inputs, Rows=[], Products=[], Failures=[], Scope=__doc__,
                  OrdinaryLiteralLoopback=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  OsErrorQueueProbes=False, ManagedLanguageAcceptance=False, ShippingLibraryAcceptance=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')

    def product(path):
        report['Products'].append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def command(cc, flags, source, selected, binary):
        result = [str(cc), '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, '-DXRT_IMPLEMENTATION',
                  '-DXRT_MODULE_NET_UDP_FUTURE', '-DXRT_MODULE_MEMORY_DEBUG', '-I'+str(selected.parent),
                  '-I'+str(root/'include')]
        if source != fixtures[0]:
            result += ['-include', str(selected)]
        if os.name != 'nt':
            result += ['-D_GNU_SOURCE']
        return result + [str(source), *(['-lws2_32', '-liphlpapi', '-ladvapi32'] if os.name == 'nt'
                                       else ['-pthread', '-ldl', '-lm']), '-o', str(binary)]

    def run(name, cmd, instrumented=False, rejection=False, marker=None):
        cmd = list(map(str, cmd))
        log = out/(name+'.log')
        env = dict(os.environ)
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:
            done = subprocess.run(cmd, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        report['Rows'].append(dict(Name=name, Command=cmd, Exit=done.returncode, ExpectedRejection=rejection,
                                   Log=str(log), Sha256=sha(log)))
        save()
        print(name, done.returncode, flush=True)
        assert (done.returncode != 0 if rejection else done.returncode == 0), (name, str(log))
        text = log.read_text(errors='replace')
        if marker:
            assert marker in text, (name, marker, str(log))
        return text

    save()
    try:
        for number, cc in enumerate(compilers):
            modes = [('O0', ['-O0']), ('O2', ['-O2'])]
            if args.sanitize and cc.stem != 'tcc':
                modes.append(('instrumented', ['-O1', '-g', '-fsanitize=address,undefined',
                                               '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
            for mode, flags in modes:
                for index, fixture in enumerate(fixtures):
                    label = f'{number}.{mode}.{index}'
                    binary = out/(label+('.exe' if os.name == 'nt' else '.run'))
                    run(label+'.build', command(cc, flags, fixture, header, binary))
                    product(binary)
                    for backend in (['select', 'native'] if index == 0 else ['select']):
                        text = run(label+'.'+backend, [binary, *(['native'] if backend == 'native' else [])],
                                   mode == 'instrumented')
                        if index == 0:
                            assert all(word in text for word in ('graphs=24', 'roots=24', 'drop-races=24', 'OOM=36',
                                                                'deliveries=5', 'cancel-races=48', 'closed-shells=4',
                                                                'late-commands=1', 'balanced')), label
        text = header.read_text(encoding='utf8')
        gcc = next(cc for cc in compilers if cc.stem != 'tcc')
        controls = [
            ('missing-source-wait',
             'if (pVisit && !pVisit(__xrtNetUdpWaitOwnership(pWait), pContext)) return false;',
             '(void)pContext; (void)pVisit;', 'n.StrongCount==3&&n.InternalCount==3', []),
            ('missing-empty-batch',
             'return !pWait->Batch || pVisit(xrtNetUdpBatchOwnership(pWait->Batch), pContext);',
             'return true;', 'emptyBatches', []),
            ('missing-result-batch',
             'return pValue && !pData && pVisit && pVisit(xrtNetUdpBatchOwnership(pValue), pContext);',
             'return pValue && !pData && pVisit && ((void)pContext, true);', 'resultBatch&&resultPacket', []),
            ('early-command-drop',
             '__xrtNetUdpControlImpl(w, data); __xrtNetUdpActiveLeave(u);',
             '__xrtNetUdpControlImpl(w, data); xrtNetUdpDestroy(u); __xrtNetUdpActiveLeave(u);',
             'late command tail regression', ['late-only']),
        ]
        for name, needle, replacement, marker, options in controls:
            assert text.count(needle) == 1, name
            selected = out/name/'xrt.h'
            selected.parent.mkdir()
            selected.write_text(text.replace(needle, replacement), encoding='utf8')
            product(selected)
            binary = out/(name+('.exe' if os.name == 'nt' else '.run'))
            run(name+'.build', command(gcc, ['-O0'], fixtures[0], selected, binary))
            product(binary)
            run(name+'.refused', [binary, *options], rejection=True, marker=marker)
        report['InputsUnchanged'] = all(sha(p) == value for p, value in inputs.items())
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in report['Rows'])
        report['ProductsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in report['Products'])
        assert all(report[key] for key in ('InputsUnchanged', 'LogsUnchanged', 'ProductsUnchanged'))
        report['Complete'] = True
        save()
    except BaseException as error:
        report['Failures'].append(repr(error))
        save()
        raise


if __name__ == '__main__':
    main()
