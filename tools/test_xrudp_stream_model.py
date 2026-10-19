"""Seeded public-API stream/control model with bounded faulted packet queues.

Virtual-clock unit exploration; never substitutes for native or 72h acceptance.
Uses an explicit committed SDK core, preserving unrelated local XRT core edits.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time
from test_xrudp_native_soak import compile_new

ROOT=Path(__file__).resolve().parents[1]

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def write_same(path,body):
    if not path.exists() or path.read_bytes()!=body: path.write_bytes(body)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True); parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true'); parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--development',action='store_true')
    parser.add_argument('--seeds',type=int,default=32); parser.add_argument('--steps',type=int,default=20000)
    parser.add_argument('--base-seed',type=lambda value:int(value,0),default=0x87165943)
    args=parser.parse_args(); assert 1<=args.seeds<=128 and 1000<=args.steps<=200000
    sdk=args.sdk.resolve(); revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    for name in ('xrt.h','xrt_decl.h'):
        assert (sdk/'lib'/name).read_bytes()==subprocess.check_output(['git','show',revision+':single/'+name],cwd=ROOT)
    platform='windows' if os.name=='nt' else 'linux'
    target=ROOT/'out/xrudp-stream-model'; target.mkdir(parents=True,exist_ok=True)
    core_source=target/f'core-{platform}.c'
    wrapper=target/f'model-{platform}.c'
    write_same(core_source,b'#define XRT_IMPLEMENTATION\n#include <xrt.h>\n')
    write_same(wrapper,b'#include <xrt.h>\n#include "../../extlibs/xrudp/tests/test_stream_model.c"\n')
    version=subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0]
    flags=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1',
        '-I'+str(sdk/'lib'),'-I'+str(ROOT/'extlibs/xrudp/include'),'-I'+str(sdk/'lib/xrtshim')]
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    if os.name=='nt': flags+=['--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096']
    core_hashes={str(p):sha(p) for p in (core_source,sdk/'lib/xrt.h',sdk/'lib/xrt_decl.h')}
    core_command=flags+['-c',str(core_source),'-o','<core-output>']
    core_key=hashlib.sha256(json.dumps([core_command,version,core_hashes],sort_keys=True).encode()).hexdigest()
    core=target/'builds'/core_key/'core.o'; core_receipt=core.with_name('core.o.build.json')
    if core.exists():
        meta=json.loads(core_receipt.read_text()); assert meta['input_key']==core_key and meta['object_sha256']==sha(core)
    else:
        core_command[-1]=str(core); print('compile core '+platform,flush=True)
        compile_new(core,core_command)
        assert all(sha(Path(p))==digest for p,digest in core_hashes.items())
        core_receipt.write_text(json.dumps({'input_key':core_key,'compiler':version,'command':core_command,
            'source_hashes':core_hashes,'object_sha256':sha(core)},indent=2)+'\n')
    sources=sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))
    paths=[*sources,wrapper,core,Path(__file__).resolve(),ROOT/'tools/test_xrudp_native_soak.py',
        ROOT/'extlibs/xrudp/tests/test_stream_model.c',ROOT/'extlibs/xrudp/tests/test_session.c']
    for directory in (ROOT/'extlibs/xrudp/include',ROOT/'extlibs/xrudp/src/internal',sdk/'lib/xrtshim'):
        paths+=sorted(directory.rglob('*.h'))
    dirty=[]
    for path in paths:
        if path.is_relative_to(ROOT/'extlibs/xrudp') or path.is_relative_to(ROOT/'tools'):
            raw=subprocess.run(['git','show',revision+':'+path.relative_to(ROOT).as_posix()],cwd=ROOT,capture_output=True)
            if raw.returncode or raw.stdout.replace(b'\r\n',b'\n')!=path.read_bytes().replace(b'\r\n',b'\n'):
                dirty.append(path.relative_to(ROOT).as_posix())
    assert args.development or not dirty,'commit test/runtime inputs before fixed-source acceptance'
    hashes={str(path):sha(path) for path in paths}; hashes.update(core_hashes)
    command=flags+['-DXRUDP_MODULE_ALL=1',str(wrapper),str(core),*map(str,sources),'-o','<model-output>']
    command+=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-lm','-ldl']
    key=hashlib.sha256(json.dumps([command,version,hashes,revision,args.development],sort_keys=True).encode()).hexdigest()
    binary=target/'builds'/key/('model.exe' if os.name=='nt' else 'model')
    command[command.index('-o')+1]=str(binary)
    receipt=binary.with_name(binary.name+'.build.json')
    if args.reuse:
        build=json.loads(receipt.read_text()); assert build['input_key']==key and build['binary_sha256']==sha(binary)
        assert build['source_commit']==revision and build['development_only']==args.development
    else:
        print('compile stream model '+platform,flush=True); compile_new(binary,command)
        assert all(sha(Path(path))==digest for path,digest in hashes.items()),'input changed during build'
        build={'source_commit':revision,'development_only':args.development,'dirty_inputs':dirty,'input_key':key,
            'command':command,'compiler':version,'source_hashes':hashes,'binary_sha256':sha(binary)}
        receipt.write_text(json.dumps(build,indent=2)+'\n')
    cases=[]; started=time.monotonic()
    for index in range(args.seeds):
        seed=(args.base_seed+index*0x9e3779b9)&0xffffffff
        if not seed: seed=1
        proc=subprocess.run([str(binary),str(seed),str(args.steps)],capture_output=True,text=True,timeout=90,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
        output=proc.stdout+proc.stderr
        if proc.returncode or 'balanced=1' not in proc.stdout:
            print(json.dumps({'failed_seed':seed,'steps':args.steps,'exit':proc.returncode}),flush=True)
            print(output[:8192]+('...\n'+output[-4096:] if len(output)>8192 else ''),flush=True)
            return 1
        counters=dict((name,int(value)) for name,value in re.findall(r'([a-z-]+)=(\d+)',proc.stdout))
        assert counters['seed']==seed and counters['steps']==args.steps and counters['balanced']==1
        cases.append(counters); print(proc.stdout.strip(),flush=True)
    assert all(sha(Path(path))==digest for path,digest in hashes.items()) and sha(binary)==build['binary_sha256']
    record={'build':build,'cases':cases,'platform':platform,'sanitized':args.sanitize,'virtual_time':True,
            'committed_acceptance':not args.development and not dirty,'acceptance_72h':False,
            'real_seconds':time.monotonic()-started}
    path=target/(('development-' if args.development else 'acceptance-')+platform+('-asan' if args.sanitize else '')+'.json')
    path.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps({'passed_seeds':len(cases),'exploration_steps':args.steps*args.seeds,'committed_acceptance':record['committed_acceptance']}),flush=True)
    return 0

if __name__=='__main__': raise SystemExit(main())
