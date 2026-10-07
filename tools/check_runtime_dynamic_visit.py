#!/usr/bin/env python3
"""Selected XRT DynamicFields protected visitor; not whole-runtime acceptance."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc')
    if not cc or out.exists() or (args.sanitize and os.name=='nt'):
        parser.error('Require gcc, fresh evidence and Linux for sanitizers')
    fixture='extlibs/xruntime/tests/single/test_single_runtime_dynamic_field_visit.c'
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable),root/fixture,
        root/'single/xrt.h',root/'single/extlibs/xruntime.h',root/'single/extlibs/xruntime_decl.h',
        root/'include/xrt/features.h',root/'extlibs/xruntime/include/xruntime/features.h',
        root/'extlibs/xruntime/include/xrt/runtime_field.h',root/'extlibs/xruntime/src/runtime/runtime_dynamic_field.c'}
    inputs=[{'Path':str(path),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'SelectedRuntimeInstrumented':args.sanitize,'WholeRuntimeInstrumented':False,'Scope':__doc__}
    out.mkdir(parents=True); env=dict(os.environ)
    flags=['-std=c11','-Wall','-Wextra','-Werror']; links=[] if os.name=='nt' else ['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        print(name+': '+str(result.returncode),flush=True)
        if result.returncode: raise RuntimeError(data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        for level in (0,2):
            binary=out/('visit.O'+str(level)+('.exe' if os.name=='nt' else ''))
            run(f'O{level}.build',[cc,*flags,f'-O{level}',fixture,*links,'-o',str(binary)])
            proof=run(f'O{level}.run',[str(binary)])
            assert 'protected keys/edges' in proof
            match=re.search(r'(\d+) complete fault positions balanced',proof); assert match and int(match[1])>0
            report[f'O{level}FaultPositions']=int(match[1])
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
