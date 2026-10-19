"""Native Future adapter gates against an immutable SDK XRT core.

Default builds require committed xrudp inputs. --development permits explicit
precommit development tests and marks the receipt ineligible for acceptance.
No account, model, external service, private XRT header or network dependency.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--development',action='store_true')
    parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--layout',choices=('modular','single'),default='modular')
    parser.add_argument('--suite',choices=('future','faults','omitted'),default='future')
    parser.add_argument('--repeat',type=int,default=1)
    parser.add_argument('--families',choices=('both','4','6'),default='both')
    parser.add_argument('--backends',choices=('both','select','native'),default='both')
    args=parser.parse_args(); sdk=args.sdk.resolve(); assert 1<=args.repeat<=50
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    for name in ('xrt.h','xrt_decl.h'):
        assert (sdk/'lib'/name).read_bytes()==subprocess.check_output(['git','show',revision+':single/'+name],cwd=ROOT),'SDK core must equal the committed XRT core'
    target=ROOT/'out/xrudp-endpoint-future'; target.mkdir(parents=True,exist_ok=True)
    platform='windows' if os.name=='nt' else 'linux'
    tag=args.suite+'-'+args.layout+'-'+platform+('-asan' if args.sanitize else '')
    wrapper=target/(tag+'.c')
    source='test_endpoint_future'+{'future':'','faults':'_fault','omitted':'_disabled'}[args.suite]+'.c'
    body='#include <xrt.h>\n'
    if args.layout=='single': body+='#define XRUDP_IMPLEMENTATION\n#include "../../single/extlibs/xrudp.h"\n'
    wrapper.write_text(body+'#include "../../extlibs/xrudp/tests/'+source+'"\n',encoding='utf-8',newline='\n')
    binary=target/(tag+('.exe' if os.name=='nt' else ''))
    command=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1',
             '-I'+str(sdk/'lib'),'-I'+str(ROOT/'extlibs/xrudp/include'),'-I'+str(sdk/'lib/xrtshim')]
    if args.sanitize: command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    sources=sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))
    inputs=sources+[wrapper,Path(__file__).resolve(),ROOT/'extlibs/xrudp/config/modules.json',sdk/'lib/xrt.h',sdk/'lib/xrt_decl.h',ROOT/'extlibs/xrudp/tests/test_endpoint_fault.c']
    inputs+=sorted((ROOT/'extlibs/xrudp/tests').glob('test_endpoint_future*.c'))
    inputs+=sorted((ROOT/'extlibs/xrudp/tests/single').glob('test_single_endpoint_future*.c'))
    if args.layout=='single': inputs+=[ROOT/'single/extlibs/xrudp.h',ROOT/'single/extlibs/xrudp_decl.h']
    for directory in (ROOT/'extlibs/xrudp/include',ROOT/'extlibs/xrudp/src/internal',sdk/'lib/xrtshim'):
        inputs+=sorted(directory.rglob('*.h'))
    dirty=[]
    for path in inputs:
        if path.is_relative_to(ROOT/'extlibs/xrudp') or path.is_relative_to(ROOT/'single/extlibs') or path==Path(__file__).resolve():
            committed=subprocess.run(['git','show',revision+':'+path.relative_to(ROOT).as_posix()],cwd=ROOT,capture_output=True)
            if committed.returncode or path.read_bytes().replace(b'\r\n',b'\n')!=committed.stdout.replace(b'\r\n',b'\n'): dirty.append(path.relative_to(ROOT).as_posix())
    assert args.development or not dirty,'commit runtime changes before an acceptance build'
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    version=subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0]
    core_source=target/('core-'+platform+'.c')
    core_source.write_text('#define XRT_IMPLEMENTATION\n#include <xrt.h>\n',encoding='utf-8',newline='\n')
    core=target/('core-'+platform+('-asan' if args.sanitize else '')+'.o')
    core_command=command+['-c',str(core_source),'-o',str(core)]
    core_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [core_source,sdk/'lib/xrt.h',sdk/'lib/xrt_decl.h']}
    core_key=hashlib.sha256(json.dumps([core_command,version,core_hashes],sort_keys=True).encode()).hexdigest()
    core_record=core.with_name(core.name+'.json')
    core_build=json.loads(core_record.read_text(encoding='utf-8')) if core_record.exists() else {}
    if not core.exists() or core_build.get('input_key')!=core_key or core_build.get('object_sha256')!=hashlib.sha256(core.read_bytes()).hexdigest():
        print(json.dumps({'stage':'compile-core','platform':platform,'sanitized':args.sanitize}),flush=True)
        subprocess.run(core_command,cwd=ROOT,check=True)
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in core_hashes.items()),'core input changed during build'
        core_build={'input_key':core_key,'command':core_command,'source_hashes':core_hashes,'compiler':version,'object_sha256':hashlib.sha256(core.read_bytes()).hexdigest()}
        core_record.write_text(json.dumps(core_build,indent=2)+'\n',encoding='utf-8')
    inputs+=[core_source,core]; hashes.update(core_hashes); hashes[str(core)]=hashlib.sha256(core.read_bytes()).hexdigest()
    command+=['-DXRUDP_MODULE_RUDP_ENDPOINT=1' if args.suite=='omitted' else '-DXRUDP_MODULE_ALL=1',str(wrapper),str(core)]
    if args.layout=='modular': command+=list(map(str,sources))
    command+=['-o',str(binary)]
    command+=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-lm','-ldl']
    key=hashlib.sha256(json.dumps([command,version,hashes],sort_keys=True).encode()).hexdigest()
    build_receipt=binary.with_name(binary.name+'.build.json')
    if args.reuse:
        build=json.loads(build_receipt.read_text(encoding='utf-8'))
        assert build['input_key']==key and build['binary_sha256']==hashlib.sha256(binary.read_bytes()).hexdigest() and build['source_commit']==revision and build['development_only']==args.development,'native inputs/provenance changed; rebuild without --reuse'
    else:
        print(json.dumps({'stage':'compile-adapter','tag':tag}),flush=True)
        result=subprocess.run(command,cwd=ROOT)
        if result.returncode: return result.returncode
        assert all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in inputs),'compiler input changed during build'
        build={'source_commit':revision,'development_only':args.development,'uncommitted_inputs':dirty,
               'compiler':version,'command':command,'source_hashes':hashes,'input_key':key,
               'core_build':core_build,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
        build_receipt.write_text(json.dumps(build,indent=2)+'\n',encoding='utf-8')
    families=(4,6) if args.families=='both' else (int(args.families),)
    backends=(False,True) if args.backends=='both' else (args.backends=='native',)
    cases=[]
    matrix=[(None,False)] if args.suite=='faults' else [(family,backend) for backend in backends for family in families]
    marker={'future':'Engine-safe results passed','faults':'balanced object lifetimes passed','omitted':'native endpoint lifecycle passed'}[args.suite]
    for iteration in range(args.repeat):
        for family,backend in matrix:
            arguments=[] if args.suite=='faults' else [str(family),str(int(backend))]
            result=subprocess.run([str(binary),*arguments],capture_output=True,text=True,timeout=45,
                env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
            if result.returncode or marker not in result.stdout:
                print(json.dumps({'native_exit':result.returncode,'family':family,'backend':int(backend)}),flush=True)
                print(result.stdout+result.stderr,flush=True); return 1
            case={'suite':args.suite,'layout':args.layout,'iteration':iteration,'family':family,'backend':('iocp' if os.name=='nt' else 'epoll') if backend else 'select',
                  'development_only':args.development,'native_exit':result.returncode,'sanitized':args.sanitize,'joined':True}
            cases.append(case); print(json.dumps(case),flush=True)
    receipt=target/(('development-' if args.development else 'acceptance-')+tag+'.json')
    receipt.write_text(json.dumps({'build':build,'cases':cases,'committed_acceptance':not args.development and not dirty},indent=2)+'\n',encoding='utf-8')
    return 0


if __name__=='__main__': raise SystemExit(main())
