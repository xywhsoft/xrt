"""Raw aggregate Server compatibility after owned Listener event changes.

Only two explicit legacy fixtures: valid literal-loopback endpoints, ordinary
accept, cancel, close and cleanup. No DNS, external peers or security tests.
This does not certify a managed Server ownership graph or language API.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    fixtures=[root/'tests/network'/name for name in ('test_net_tcp_server.c','test_net_tcp_server_future.c')]
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),root/'single/xrt.h',root/'tests/test.h',*fixtures,*compilers]
    # Detail wait is included by the fixtures, even when its single-header
    # definition suppresses a second body via its real include guard.
    files+=list((root/'include').rglob('*.h'))
    inputs={str(f):sha(f) for f in files};rows=[];products=[];failures=[];out.mkdir(parents=True)
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        OwnLiteralLoopbackOnly=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ManagedServerAcceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)));save()
    def run(name,command,instrumented=False):
        command=list(map(str,command));log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:code=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        rows.append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==0,(name,str(log))
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for label,flags in modes:
                for fixture in fixtures:
                    name=str(index)+'.'+label+'.'+fixture.stem;binary=out/(name+('.exe' if os.name=='nt' else ''))
                    cmd=[cc,'-std=c11','-Wall','-Werror',*flags,'-DXRT_IMPLEMENTATION','-DXRT_MODULE_NET_TCP_SERVER_FUTURE',
                        '-DXRT_MODULE_MEMORY_DEBUG','-I'+str(root/'include'),'-include',root/'single/xrt.h']
                    if cc.stem!='tcc':cmd+=['-Wextra']
                    if os.name!='nt':cmd+=['-D_GNU_SOURCE']
                    cmd+=[fixture,*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-lm']),'-o',binary]
                    run(name+'.build',cmd);product(binary);run(name+'.run',[binary],label=='instrumented')
        report['InputsUnchanged']=all(sha(f)==digest for f,digest in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
        assert all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise


if __name__=='__main__':main()
