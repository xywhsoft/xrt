#!/usr/bin/env python3
"""Existing value graph regressions plus the optional resident graph adapter."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
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
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc')
    if not cc or out.exists() or (args.sanitize and os.name=='nt'):
        parser.error('Require gcc, fresh evidence and Linux for sanitizers')
    fixtures=('test_value_graph','test_value_graph_contract','test_value_graph_oom','test_value_graph_adapter')
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),root/'tests/test.h',root/'single/xrt.h',
        root/'single/xrt_decl.h',root/'include/xrt/value.h',root/'src/value/value_graph.c'}
    files.update(root/('tests/value/'+name+'.c') for name in fixtures)
    inputs=[{'Path':str(path),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Scope':__doc__,'SelectedRuntimeInstrumented':args.sanitize,'WholeRuntimeInstrumented':False}
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
    def run(name,command):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        print(f'{name}: {result.returncode}',flush=True)
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
    try:
        for level in (0,2):
            for name in fixtures:
                lane=name+f'.O{level}'; binary=out/(lane+('.exe' if os.name=='nt' else ''))
                run(lane+'.build',[cc,*flags,f'-O{level}','tests/value/'+name+'.c',*links,'-o',str(binary)])
                run(lane+'.run',[str(binary)])
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
