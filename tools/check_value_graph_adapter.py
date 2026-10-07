#!/usr/bin/env python3
"""Existing value graph regressions plus the optional resident graph adapter."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--legacy-include',type=Path,
        help='Frozen previous single header: prove the transient-source regression fails there')
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc')
    if not cc or out.exists() or (args.sanitize and os.name=='nt'):
        parser.error('Require gcc, fresh evidence and Linux for sanitizers')
    fixtures=('test_value_graph','test_value_graph_contract','test_value_graph_oom','test_value_graph_adapter',
        'test_value_graph_projection')
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),root/'tests/test.h',root/'single/xrt.h',
        root/'single/xrt_decl.h',root/'include/xrt/value.h',root/'src/value/value_graph.c'}
    files.update(root/('tests/value/'+name+'.c') for name in fixtures)
    if args.legacy_include: files.add(args.legacy_include.resolve()/'xrt.h')
    inputs=[{'Path':str(path),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Scope':__doc__,'SelectedRuntimeInstrumented':args.sanitize,'WholeRuntimeInstrumented':False,
        'LegacyRegressionProved':False,'ProjectionFaultPositions':{}}
    out.mkdir(parents=True)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXRT_IMPLEMENTATION','-DXRT_MODULE_VALUE_GRAPH',
        '-DXRT_MODULE_VALUE_COLLECTION','-Isingle']
    links=[] if os.name=='nt' else ['-pthread','-lm']
    env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,expected=0):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'ExpectedExit':expected,
            'Complete':result.returncode==expected})
        print(f'{name}: {result.returncode}',flush=True)
        if result.returncode!=expected: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        if args.legacy_include:
            legacy=out/('legacy.exe' if os.name=='nt' else 'legacy')
            legacy_flags=[f for f in flags if f!='-Isingle']+['-I'+str(args.legacy_include.resolve())]
            run('legacy.build',[cc,*legacy_flags,'-O0','tests/value/test_value_graph_adapter.c',*links,'-o',str(legacy)])
            proof=run('legacy.expected_failure',[str(legacy)],expected=1)
            assert 'transient source address reused an earlier memo identity' in proof
            report['LegacyRegressionProved']=True
        for level in (0,2):
            for name in fixtures:
                lane=name+f'.O{level}'; binary=out/(lane+('.exe' if os.name=='nt' else ''))
                debug=['-DXRT_MODULE_MEMORY_DEBUG'] if name=='test_value_graph_projection' else []
                run(lane+'.build',[cc,*flags,*debug,f'-O{level}','tests/value/'+name+'.c',*links,'-o',str(binary)])
                proof=run(lane+'.run',[str(binary)])
                if name=='test_value_graph_projection':
                    match=re.search(r'(\d+) complete allocation failure positions',proof)
                    assert match and int(match[1])>0
                    report['ProjectionFaultPositions'][str(level)]=int(match[1])
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
