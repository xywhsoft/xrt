"""Normal literal-loopback file send and no-side-effect capability rejection.

No external endpoints, malformed network data, probes or security testing.
Use a fresh --out. --header permits testing a separately fixed API snapshot;
--pinned-timing selects that snapshot's microsecond relative wait convention.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--header',type=Path)
    p.add_argument('--pinned-timing',action='store_true')
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    header=(a.header or root/'single/xrt.h').resolve()
    fixture=root/'tests/network/test_net_tcp_file_capability.c'
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),header,fixture,*compilers)}
    out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        OwnLoopbackOnly=True,ExternalNetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,cmd,instrumented=False):
        cmd=list(map(str,cmd));log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:code=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        rows.append(dict(Name=name,Command=cmd,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==0,(name,str(log))
        return log.read_text(errors='replace')
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for label,mode in modes:
                name=str(index)+'.'+label;binary=out/(name+('.exe' if os.name=='nt' else ''))
                cmd=[cc,'-std=c11','-Wall','-Werror',*mode,'-DXRT_IMPLEMENTATION','-DXRT_MODULE_NET_TCP_FILE',
                    '-DXRT_MODULE_NET_TCP_SYNC','-DXRT_MODULE_MEMORY_DEBUG','-DXRT_MODULE_FILE_TEMP','-I'+str(header.parent)]
                if cc.stem!='tcc':cmd+=['-Wextra']
                if a.pinned_timing:cmd+=['-DTEST_PINNED_TIMING']
                if os.name!='nt':cmd+=['-D_GNU_SOURCE']
                cmd+=[fixture,*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',binary]
                run(name+'.build',cmd);product(binary)
                proof=run(name+'.run',[binary],label=='instrumented')
                assert proof.count('no cursor change, close-safe, NUL roundtrip and balanced')==2
        report['InputsUnchanged']=all(sha(f)==digest for f,digest in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
        assert all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise


if __name__=='__main__':main()
