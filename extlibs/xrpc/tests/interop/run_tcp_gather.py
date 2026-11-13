"""Build/run the actual RPC TCP production-TU gather/refusal/native-offer probe."""
import argparse,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
import build

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',required=True);p.add_argument('--arch',choices=['native','x86','x64'],default='native');p.add_argument('--sanitize',choices=['none','address','thread'],default='none');p.add_argument('--source',type=Path,help='explicit alternative production TCP source, for matched old/current evidence');p.add_argument('--baseline',action='store_true',help='assert the original separate-header/body native-offer count');a=p.parse_args()
    sources,_,_,_,defines,links,_,_,incs,headers=build._load_modules('xrpc_tcp_tests',[ROOT/'extlibs/xrpc/config/modules.json']);flags=['-O2']
    if a.sanitize=='address':flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    if a.sanitize=='thread':flags+=['-g','-fsanitize=thread','-Wno-error=tsan']
    for inc in incs:flags+=['-I',str(ROOT/inc)]
    dest=ROOT/'out/interop/tcp-gather'/a.arch/a.sanitize;dest.mkdir(parents=True,exist_ok=True)
    objects=build._compile_objects(a.compiler,a.arch,sources,defines,dest/'objects',False,flags,headers);objects=[obj for src,obj in zip(sources,objects) if src not in ('src/network/tcp.c','extlibs/xrpc/src/tcp/tcp.c')]
    options=build._compile_options(a.compiler,a.arch,defines,flags);core=dest/'native-instrumented.o'
    subprocess.run([a.compiler,*options,'-DxrtNetSocketSendVec=tcpGatherSocketSendVec','-DxrtNetPortSendVec=tcpGatherPortSendVec','-c',str(ROOT/'src/network/tcp.c'),'-o',str(core)],cwd=ROOT,check=True)
    extra=[]
    if a.source:
        source=a.source if a.source.is_absolute() else ROOT/a.source
        if not source.is_file():p.error('explicit source does not exist')
        extra+=['-DXRPC_TCP_PROBE_SOURCE="'+source.as_posix()+'"','-I',str(ROOT/'extlibs/xrpc/src/tcp')]
    if a.baseline:extra+=['-DXRPC_TCP_GATHER_BASELINE']
    exe=dest/('probe.exe' if sys.platform=='win32' else 'probe');cmd=[a.compiler,*options,*extra,str(ROOT/'extlibs/xrpc/tests/interop/tcp_gather.c'),str(core),*map(str,objects),*['-l'+link for link in links],'-o',str(exe)]
    if sys.platform!='win32':cmd+=['-pthread','-lm']
    subprocess.run(cmd,cwd=ROOT,check=True);print('[probe]',exe,flush=True);subprocess.run([str(exe)],cwd=ROOT,check=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
