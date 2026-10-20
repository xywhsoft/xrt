"""Provider-cache publication acceptance: unbound local sockets, no traffic.

Tests modular sources and single headers with GCC/TCC at O0/O2. An optional
--root may select a clean HEAD-plus-fix snapshot; --header adds pinned consumer
headers without importing their unrelated source changes. No security tests.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--header',type=Path,action='append',default=[])
    parser.add_argument('--compiler',action='append',required=True)
    args=parser.parse_args()
    root=args.root.resolve();out=args.out.resolve()
    assert not out.exists()
    sys.path.insert(0,str(root/'tools'))
    from xrt_manifest import load_manifest,dependency_closure
    modules=dependency_closure(['net_socket','thread','atomic','time'],load_manifest()['modules'])
    sources=list(dict.fromkeys(root/name for m in modules for name in m['sources']))
    fixture=root/'tests/network/test_net_socket_message_cache.c'
    headers=[root/'single/xrt.h',*[path.resolve() for path in args.header]]
    compilers=[Path(shutil.which(name) or name).resolve() for name in args.compiler]
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),root/'config/modules.json',
            root/'tools/xrt_manifest.py',fixture,*sources,*headers,*compilers)}
    for folder in ('src/internal','include'):
        inputs.update((str(f),sha(f)) for f in (root/folder).rglob('*.h'))
    out.mkdir(parents=True)
    rows=[];products=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,
        Rows=rows,Products=products,NetworkTraffic=False,NetworkSecurityTests=False,
        ShippingLibraryAcceptance=False,Scope=__doc__)

    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')

    def run(name,command):
        command=list(map(str,command));log=out/(name+'.log');started=time.monotonic()
        with log.open('wb') as stream:
            result=subprocess.run(command,cwd=out,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Exit=result.returncode,Command=command,Log=str(log),
            Sha256=sha(log),Seconds=time.monotonic()-started))
        save();print(name+': '+str(result.returncode),flush=True)
        assert result.returncode==0,str(log)
        return log.read_text(encoding='utf8',errors='replace')

    libraries=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for index,cc in enumerate(compilers):
            flags=[cc,'-std=c11','-Wall','-Werror','-I'+str(root/'include'),'-I'+str(root),
                *['-DXRT_MODULE_'+name for name in ('NET_SOCKET','THREAD','ATOMIC','TIME')],
                *['-D'+m['feature'] for m in modules if m.get('feature')]]
            if cc.stem.lower()!='tcc':flags+=['-Wextra']
            if os.name=='nt':flags+=['-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00']
            else:flags+=['-D_GNU_SOURCE']
            for opt in ('O0','O2'):
                for mode in range(len(headers)+1):
                    name=f'{index}.{opt}.{mode}';binary=out/(name+('.exe' if os.name=='nt' else ''))
                    units=list(sources) if mode==0 else [
                        '-DTEST_SOCKET_CACHE_HEADER="'+str(headers[mode-1]).replace('\\','/')+'"']
                    # xruntime embeds XRT under its own implementation switch.
                    if mode and headers[mode-1].name=='xruntime.h':units+=['-DXRUNTIME_IMPLEMENTATION']
                    run(name+'.build',[*flags,'-'+opt,fixture,*units,*libraries,'-o',binary])
                    products.append(dict(Path=str(binary),Sha256=sha(binary)));save()
                    proof=run(name+'.run',[binary])
                    assert 'eight simultaneous first users, canonical publication; no traffic' in proof
        report['InputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged']
        assert all(sha(row['Path'])==row['Sha256'] for row in products)
        report['Complete']=True
    except BaseException as error:
        report['Error']=repr(error)
        raise
    finally:
        save()


if __name__=='__main__':
    main()
