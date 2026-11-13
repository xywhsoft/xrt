"""Run the real candidate-winner close-publication/owner-OOM production TU."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];sys.path.insert(0,str(ROOT/'tools'));import build
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',required=True);p.add_argument('--arch',choices=['native','x86','x64'],default='native');p.add_argument('--sanitize',choices=['none','address','thread'],default='none');p.add_argument('--output-dir',type=Path);p.add_argument('--source',type=Path);p.add_argument('--race-only',action='store_true');a=p.parse_args()
 sources,_,_,_,defs,links,_,_,incs,headers=build._load_modules('xrpc_tls_dial_tests',[ROOT/'extlibs/xrpc/config/modules.json']);flags=['-O2']
 for inc in incs:flags+=['-I',str(ROOT/inc)]
 if a.sanitize=='address':flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
 if a.sanitize=='thread':flags+=['-g','-fsanitize=thread','-Wno-error=tsan']
 out=a.output_dir or ROOT/'out/local-interop/tls-dial-close'/a.arch/a.sanitize
 if not out.is_absolute():out=ROOT/out
 out.mkdir(parents=True,exist_ok=True);objects=build._compile_objects(a.compiler,a.arch,sources,defs,out/'objects',False,flags,headers);objects=[o for s,o in zip(sources,objects) if s!='extlibs/xrpc/src/tls/tls.c']
 if a.source:
  source=a.source if a.source.is_absolute() else ROOT/a.source
  if not source.is_file():p.error('explicit source does not exist')
  flags+=['-DXRPC_TLS_DIAL_PROBE_SOURCE="'+source.as_posix()+'"','-I',str(ROOT/'extlibs/xrpc/src/tls')]
 exe=out/('probe.exe' if os.name=='nt' else 'probe');ldflags=['-fsanitize=address,undefined'] if a.sanitize=='address' else ['-fsanitize=thread'] if a.sanitize=='thread' else []
 build._compile_program(a.compiler,a.arch,'extlibs/xrpc/tests/interop/tls_dial_close.c',objects,defs,links,exe,flags,ldflags)
 print('[test]',exe.relative_to(ROOT) if exe.is_relative_to(ROOT) else exe,flush=True);subprocess.run([str(exe),*(['race-only'] if a.race_only else [])],cwd=ROOT,check=True);return 0
if __name__=='__main__':raise SystemExit(main())
