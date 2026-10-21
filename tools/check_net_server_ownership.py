"""Actual aggregate Server, binding, Future producer and cancel graphs.

Offline ABI/ownership/OOM plus ordinary self-created literal loopback only.
Explicit allowlist; no DNS, external endpoints, malformed wire traffic,
probes, attacks or network-security tests. Not language/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from build import _load_modules,platform_name


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--probe',action='store_true',help='new fixture first compiler O0 only; discovery, not acceptance')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    if a.probe:compilers=compilers[:1]
    sources,_,_,_,defines,links,*_=_load_modules('net_tcp_server_ownership_tests,thread_key',platform=platform_name())
    fixtures=('tests/network/test_net_server_ownership.c','tests/network/test_net_tcp_server.c',
        'tests/network/test_net_tcp_server_future.c','tests/network/test_net_tcp_server_reentrant.c',
        'tests/network/test_net_tcp_server_future_oom.c','tests/network/test_net_tcp_server_sync.c')
    singles=('tests/single/test_single_net_server_ownership.c','tests/single/test_single_net_tcp_server.c')
    if a.probe:fixtures=fixtures[:1];singles=singles[:1]
    files={Path(__file__).resolve(),*compilers}
    for area in ('include','src','tests'):
        files.update(f for f in (root/area).rglob('*') if f.is_file() and f.suffix in ('.c','.h'))
    files.update(root/f for f in ('single/xrt.h','single/xrt_decl.h','config/modules.json','tools/build.py','tools/amalgamate.py'))
    inputs={str(f):sha(f) for f in sorted(files)};rows=[];products=[];failures=[];out.mkdir(parents=True)
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,Sources=sources,
        DiscoveryOnly=a.probe,OwnLiteralLoopbackOnly=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ManagedLanguageApiAcceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,cmd,instrumented=False,rejection=None):
        cmd=list(map(str,cmd));invocation=cmd;env=dict(os.environ)
        if cmd[0] in map(str,compilers) and Path(cmd[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+c.replace('\\','/').replace('"','\\"')+'"' for c in cmd[1:])+'\n',encoding='utf8')
            product(rsp);invocation=[cmd[0],'@'+str(rsp)]
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        rows.append(dict(Name=name,Command=cmd,Invocation=invocation,Exit=code,Log=str(log),Sha256=sha(log),ExpectedRejection=rejection));save()
        print(name,code,flush=True);text=log.read_text(errors='replace')
        assert (code!=0 and rejection in text) if rejection else code==0,(name,str(log))
        return text
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.probe:modes=modes[:1]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for label,flags in modes:
                prefix=str(index)+'.'+label
                common=[cc,'-std=c11','-Wall','-Werror',*flags,'-I'+str(root/'include')]
                if cc.stem!='tcc':common+=['-Wextra']
                if os.name!='nt':common+=['-D_GNU_SOURCE']
                common+=['-D'+d for d in defines]
                libs=['-l'+name for name in links]
                if os.name!='nt':libs+=['-pthread','-lm']
                objects=[]
                for number,source in enumerate(sources):
                    obj=out/(prefix+'.'+str(number)+'.o');run(prefix+'.object-'+str(number),[*common,'-c',source,'-o',obj]);product(obj);objects.append(obj)
                for fixture in (*fixtures,*singles):
                    name=prefix+'.'+Path(fixture).stem;binary=out/(name+('.exe' if os.name=='nt' else ''))
                    run(name+'.build',[*common,fixture,*(objects if fixture not in singles else []),*libs,'-o',binary]);product(binary)
                    text=run(name+'.run',[binary],label=='instrumented')
                    if 'server_ownership' in name:
                        assert 'no fabricated roots, balanced' in text and 'drop-races=9' in text
                        run(name+'.native-run',[binary,'native'],label=='instrumented')
                if not a.probe:
                    # Independent core-only link/run: do not root Server Future
                    # or synchronous adapters by using the broader object set.
                    minimal=[cc,'-std=c11','-Wall','-Werror',*flags,'-I'+str(root/'include'),'-DXRT_MODULE_NET_TCP_SERVER']
                    if cc.stem!='tcc':minimal+=['-Wextra']
                    if os.name!='nt':minimal+=['-D_GNU_SOURCE']
                    binary=out/(prefix+'.minimal-server'+('.exe' if os.name=='nt' else ''))
                    run(prefix+'.minimal.build',[*minimal,'tests/single/test_single_net_tcp_server.c',*libs,'-o',binary]);product(binary)
                    run(prefix+'.minimal.run',[binary],label=='instrumented')
                    nm=shutil.which('nm') or str(cc.with_name('nm.exe' if os.name=='nt' else 'nm'))
                    symbols=run(prefix+'.minimal.symbols',[nm,'-g',binary])
                    assert 'xrtNetServerAcceptAsync' not in symbols and 'xrtNetServerFutureProducerPolicyV1Get' not in symbols
                if not a.probe and index==0 and label=='O0':
                    original=(root/'src/network/tcp_server_ownership.c').read_text(encoding='utf8')
                    needle='return visit && __xrtNetServerBindingCount(c, &count) && visit(xrtNetServerOwnership(c->Server), context);'
                    assert original.count(needle)==1
                    mutation=original.replace(needle,'(void)context;return visit && __xrtNetServerBindingCount(c, &count);')
                    mutation=mutation.replace('#include "../internal/xrt_tcp_server.h"','#include "'+str(root/'src/internal/xrt_tcp_server.h').replace('\\','/')+'"',1)
                    source=out/'missing-binding-parent.c';source.write_text(mutation,encoding='utf8');product(source)
                    obj=out/'missing-binding-parent.o';run('mutation.object',[*common,'-c',source,'-o',obj]);product(obj)
                    selected=[obj if sources[n]=='src/network/tcp_server_ownership.c' else old for n,old in enumerate(objects)]
                    binary=out/('missing-binding-parent'+('.exe' if os.name=='nt' else ''))
                    run('mutation.build',[*common,fixtures[0],*selected,*libs,'-o',binary]);product(binary)
                    run('mutation.refusal',[binary],rejection='!node.Reachable')
        report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items())
        report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
        report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
        assert all(report[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise


if __name__=='__main__':main()
