#!/usr/bin/env python3
"""Strict native/single-header post-commit ownership, diagnostics and OOM proof.

This is not a module collector, scheduler-wide OOM or full XRT release gate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--tcc',required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=args.out.resolve()
    gcc=shutil.which('gcc')
    tcc=Path(args.tcc).resolve()
    if out.exists() or not gcc or not tcc.is_file():
        parser.error('Require a fresh evidence directory, GCC and an existing TCC')
    files={Path(__file__).resolve(),Path(sys.executable),Path(gcc),tcc,root/'tools/build.py',
           root/'tools/amalgamate.py',root/'tools/xrt_manifest.py',root/'tests/test.h',
           root/'tests/value/test_value_object_post_commit.c',root/'tests/single/test_single_value_object_post_commit.c'}
    for directory in ('src','include','single','config'):
        files.update(path for path in (root/directory).rglob('*') if path.suffix in ('.c','.h','.inc','.json'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':[],'Artifacts':[]}
    def save():
        (out/'post-commit.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for name,compiler,flags in [('gcc-O0',gcc,['--cflag=-O0']),('gcc-O2',gcc,['--cflag=-O2']),('tcc',str(tcc),[])]:
            for layout,test in [('native','test_value_object_post_commit'),('single','test_single_value_object_post_commit')]:
                label=name+'.'+layout
                command=[sys.executable,str(root/'tools/build.py'),'--compiler',compiler,
                         '--suite','memory_debug,value_container','--no-examples','--rebuild',*flags,
                         '--test' if layout=='native' else '--single-test',test]
                start=time.monotonic()
                result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
                output=result.stdout+result.stderr
                (out/(label+'.log')).write_bytes(output)
                row={'Name':label,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start}
                report['Rows'].append(row); save()
                text=output.decode(errors='replace')
                assert result.returncode==0 and '[pass]' in text,label+': '+text[-3000:]
                assert 'Value post-commit: 100 actual replacements' in text,label+' missing real ownership cases'
                assert 'Value post-commit OOM: 6 insertion/COW prefixes and balanced allocator ledger' in text,label+' missing allocator/OOM proof'
                binary=root/'out'/('gcc' if name.startswith('gcc') else 'tcc')/'native'/'memory_debug,value_container'/(test+('.exe' if sys.platform=='win32' else ''))
                copied=out/(label+binary.suffix); shutil.copy2(binary,copied)
                report['Artifacts'].append({'Path':str(copied),'Sha256':sha(copied)})
                save(); print(label+': actual back-references, receiver pin, first error and six OOM prefixes balanced',flush=True)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'],'Production/test/tool inputs changed during batch'
        report['Complete']=True
    except Exception as error:
        report['Error']=str(error); print(error,file=sys.stderr)
    finally:
        save()
    return 0 if report['Complete'] else 1


if __name__=='__main__':
    raise SystemExit(main())
