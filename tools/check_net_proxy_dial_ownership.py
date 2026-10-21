"""Owned Proxy Dial graphs, valid fixed replies and ordinary literal loopback.

Only the explicit fixture entrypoints below. No system DNS, external peers,
malformed/rejected protocol cases, fuzzing, attacks or security acceptance.
This is a native prerequisite, not public xlang or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from build import _load_modules,platform_name

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
    p.add_argument('--probe',action='store_true');args=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=args.out.resolve();assert not out.exists()
    assert not args.sanitize or os.name!='nt'
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    if args.probe:compilers=compilers[:1]
    sources,_,_,_,defines,links,*_=_load_modules('net_proxy_dial_ownership_tests,net_tcp_ownership_tests,thread_key,net_port_select,'+
        ('net_port_iocp' if os.name=='nt' else 'net_port_epoll'),platform=platform_name())
    fixtures=[('proxy.socks','tests/network/test_net_proxy_dial_ownership.c',False,()),
              ('proxy.http','tests/network/test_net_proxy_dial_ownership.c',False,('-DTEST_PROXY_DIAL_HTTP_CONNECT=1',)),
              ('proxy.single.socks','tests/single/test_single_net_proxy_dial_ownership.c',True,()),
              ('proxy.single.http','tests/single/test_single_net_proxy_dial_ownership.c',True,('-DTEST_PROXY_DIAL_HTTP_CONNECT=1',)),
              ('tcp','tests/network/test_net_tcp_ownership.c',False,()),
              ('tcp.single','tests/single/test_single_net_tcp_ownership.c',True,()),
              ('events','tests/network/test_net_stream_events_ownership.c',False,()),
              ('events.single','tests/single/test_single_net_stream_events_ownership.c',True,())]
    if args.probe:fixtures=fixtures[:2]
    files={q for area in ('include','src','tests') for q in (root/area).rglob('*') if q.is_file() and q.suffix in ('.c','.h')}
    files.update((Path(__file__).resolve(),root/'config/modules.json',root/'tools/build.py',root/'tools/amalgamate.py',
                  root/'single/xrt.h',root/'single/xrt_decl.h',root/'docs/api/proxy.md',*compilers))
    inputs={str(q):sha(q) for q in sorted(files)};rows=[];products=[];failures=[];out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,ProductsUnchanged=False,LogsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Sources=sources,DiscoveryOnly=args.probe,
        OrdinaryLiteralLoopback=True,SystemDnsQueries=False,ExternalNetworkTests=False,NetworkSecurityTests=False,
        PublicLanguageAcceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(q):products.append(dict(Path=str(q),Sha256=sha(q)))
    def run(name,cmd,instrumented=False,rejection=None):
        cmd=list(map(str,cmd));invocation=cmd
        if cmd[0] in map(str,compilers) and Path(cmd[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in cmd[1:])+'\n',encoding='utf8')
            invocation=[cmd[0],'@'+str(rsp)];product(rsp)
        env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');timeout=None
        with log.open('wb') as stream:
            try:result=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired as error:result=None;timeout=repr(error)
        rows.append(dict(Name=name,Command=cmd,Invocation=invocation,Exit=result,Timeout=timeout,
            Log=str(log),Sha256=sha(log),ExpectedRejection=rejection));save();print(name+': '+str(result),flush=True)
        assert result is not None and ((result!=0 and rejection in log.read_text(encoding='utf8',errors='replace')) if rejection else result==0),(name,str(log),timeout)
    save()
    for number,cc in enumerate(compilers):
        modes=[('O0',['-O0']),('O2',['-O2'])]
        if args.probe:modes=modes[:1]
        elif args.sanitize and cc.stem!='tcc':modes+=[('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'])]
        for mode,flags in modes:
            label=f'{number}.{mode}'
            try:
                common=[cc,'-Wall','-Wextra','-Werror',*flags,'-Iinclude',*['-D'+d for d in defines]]
                if cc.stem!='tcc':common+=['-std=c11']
                if os.name!='nt':common+=['-D_GNU_SOURCE']
                libs=['-l'+d for d in links]+(['-pthread','-lm'] if os.name!='nt' else [])
                objects=[]
                for index,source in enumerate(sources):
                    obj=out/f'{label}.{index}.o';run(f'{label}.object-{index}',[*common,'-c',source,'-o',obj]);product(obj);objects.append(obj)
                for name,source,single,extra in fixtures:
                    try:
                        binary=out/(label+'.'+name+('.exe' if os.name=='nt' else ''))
                        run(label+'.'+name+'.build',[*common,*extra,source,*([] if single else objects),*libs,'-o',binary]);product(binary)
                        run(label+'.'+name+'.run',[binary],mode=='instrumented')
                        run(label+'.'+name+'.native-run',[binary,'native'],mode=='instrumented')
                    except Exception as error:failures.append(dict(Name=label+'.'+name,Error=repr(error)));save()
                if not args.probe and number==0 and mode=='O0':
                    original=(root/'src/network/proxy_dial.c').read_text(encoding='utf8')
                    edges=[('missing-handshake-proxy-credit','\t\tif(!visit(xrtNetProxyOwnership(h->Proxy),context))return false;\n','!node.Reachable'),
                           ('missing-prepared-self-credit','\tif(pDial->PreparedEvents&&!visit(xrtNetProxyDialOwnership(pDial),context))return false;\n','self.slots==1')]
                    for name,edge,proof in edges:
                        assert original.count(edge)==1
                        text=original.replace(edge,'',1).replace('#include "../internal/xrt_proxy.h"',
                            '#include "'+(root/'src/internal/xrt_proxy.h').as_posix()+'"',1)
                        source=out/(name+'.c');source.write_text(text,encoding='utf8');product(source)
                        obj=out/(name+'.o');run(name+'.object',[*common,'-c',source,'-o',obj]);product(obj)
                        selected=[obj if q==objects[sources.index('src/network/proxy_dial.c')] else q for q in objects]
                        binary=out/(name+('.exe' if os.name=='nt' else ''))
                        run(name+'.build',[*common,'tests/network/test_net_proxy_dial_ownership.c',*selected,*libs,'-o',binary]);product(binary)
                        run(name+'.refusal',[binary],rejection=proof)
            except Exception as error:failures.append(dict(Name=label,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(q)==digest for q,digest in inputs.items())
    report['ProductsUnchanged']=all(sha(q['Path'])==q['Sha256'] for q in products)
    report['LogsUnchanged']=all(sha(q['Log'])==q['Sha256'] for q in rows)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','ProductsUnchanged','LogsUnchanged'))
    save();assert report['Complete'],failures

if __name__=='__main__':main()
