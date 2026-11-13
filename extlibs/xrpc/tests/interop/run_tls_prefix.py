"""Build/run the TLS production-TU prefix/retirement probe; no production hooks."""
import argparse,subprocess,sys,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
import build

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',required=True);p.add_argument('--arch',choices=['native','x86','x64'],default='native');p.add_argument('--sanitize',choices=['none','address','thread'],default='none');p.add_argument('--source',type=Path);p.add_argument('--short-only',action='store_true');p.add_argument('--output-dir',type=Path);a=p.parse_args()
 sources,_,_,_,defs,links,_,_,incs,headers=build._load_modules('xrpc_tls_tests',[ROOT/'extlibs/xrpc/config/modules.json']);flags=['-O2']
 for inc in incs:flags+=['-I',str(ROOT/inc)]
 if a.sanitize=='address':flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
 if a.sanitize=='thread':flags+=['-g','-fsanitize=thread','-Wno-error=tsan']
 dest=a.output_dir or ROOT/'out/local-interop/tls-prefix'/a.arch/a.sanitize
 if not dest.is_absolute():dest=ROOT/dest
 dest.mkdir(parents=True,exist_ok=True)
 objects=build._compile_objects(a.compiler,a.arch,sources,defs,dest/'objects',False,flags,headers);objects=[obj for src,obj in zip(sources,objects) if src!='extlibs/xrpc/src/tls/tls.c']
 if a.source:
  source=a.source if a.source.is_absolute() else ROOT/a.source
  if not source.is_file():p.error('explicit production source does not exist')
  flags+=['-DXRPC_TLS_PROBE_SOURCE="'+source.as_posix()+'"','-I',str(ROOT/'extlibs/xrpc/src/tls')]
 exe=dest/('probe.exe' if os.name=='nt' else 'probe');build._compile_program(a.compiler,a.arch,'extlibs/xrpc/tests/interop/tls_prefix.c',objects,defs,links,exe,flags,['-fsanitize=address,undefined'] if a.sanitize=='address' else ['-fsanitize=thread'] if a.sanitize=='thread' else [])
 print('[test]',exe.relative_to(ROOT) if exe.is_relative_to(ROOT) else exe,flush=True);subprocess.run([str(exe),*(['short-only'] if a.short_only else [])],cwd=ROOT,check=True);return 0
if __name__=='__main__':raise SystemExit(main())
