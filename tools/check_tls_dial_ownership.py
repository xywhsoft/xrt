"""Explicitly allowlisted, ordinary TLS ownership-graph acceptance.

This does not execute the module's broad test list. Only a local ownership
fixture and three existing ordinary TLS/transport loopback fixtures run. No
external addresses, malformed crypto/records, probing, fuzz or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from build import _load_modules,platform_name

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true');p.add_argument('--probe',action='store_true',help='first compiler O0, new modular fixture only; not acceptance')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler]
    sources,_,_,_,defines,links,*_=_load_modules('tls_dial_ownership_tests',platform=platform_name())
    fixtures=['tests/tls/test_tls_dial_ownership.c','tests/tls/test_tls_listener_ownership.c','tests/tls/test_tls_stream_future_ownership.c','tests/tls/test_tls_stream_ownership.c','tests/tls/test_tls_stream.c','tests/tls/test_tls_stream_client_attach.c','tests/tls/test_tls_transport_handoff.c','tests/tls/test_tls_stream_dial.c']
    if a.probe:fixtures=fixtures[:1];compilers=compilers[:1]
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    paths={q for folder in ['include','src','tests/fixtures'] for q in (root/folder).rglob('*') if q.is_file() and q.suffix in ['.c','.h']}
    paths.update([root/q for q in [*fixtures,'tests/single/test_single_tls_dial_ownership.c','tests/test.h','single/xrt.h','single/xrt_decl.h','config/modules.json','tools/build.py','tools/amalgamate.py']]);paths.update([Path(__file__),*compilers])
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(paths)},Rows=[],Products=[],Failures=[],Sources=sources,Defines=defines,
        DiscoveryOnly=a.probe,OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingLibraryAcceptance=False,FullTlsAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,sanitize=False):
        args=list(map(str,args));command=args
        if args[0] in list(map(str,compilers)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+x.replace('\\','/').replace('"','\\"')+'"' for x in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic();env=dict(os.environ)
        if sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(errors='replace')
    for index,cc in enumerate(compilers):
        modes=[('tcc',[])] if cc.stem=='tcc' else [('O0',['-O0']),('O2',['-O2'])]
        if a.probe:modes=modes[:1]
        if a.sanitize and not a.probe and cc.stem!='tcc':
            assert os.name!='nt';modes.append(('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
        for name,opt in modes:
            label=str(index)+'.'+name
            try:
                common=[cc,'-Wall','-Wextra','-Werror','-I'+str(root/'include'),*opt,*(['-std=c11'] if cc.stem!='tcc' else []),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),*['-D'+d for d in defines]]
                libs=['-l'+n for n in links]+([] if os.name=='nt' else ['-pthread','-lm'])
                objects=[]
                for i,source in enumerate(sources):
                    obj=out/(label+'.'+str(i)+'.o');run(label+'.object-'+str(i),[*common,'-c',root/source,'-o',obj]);objects.append(obj)
                for source in fixtures:
                    binary=out/(label+'.'+Path(source).stem+('.exe' if os.name=='nt' else ''))
                    run(label+'.'+Path(source).stem+'.build',[*common,root/source,*objects,*libs,'-o',binary])
                    result=run(label+'.'+Path(source).stem+'.run',[binary],name=='asan')
                    if source==fixtures[0]:
                        assert 'TLS Dial ownership:' in result
                        assert 'TLS Dial ownership:' in run(label+'.'+Path(source).stem+'.native-run',[binary,'native'],name=='asan')
                if not a.probe:
                    binary=out/(label+'.single'+('.exe' if os.name=='nt' else ''))
                    run(label+'.single.build',[*common,root/'tests/single/test_single_tls_dial_ownership.c',*libs,'-o',binary])
                    assert 'TLS Dial ownership:' in run(label+'.single.run',[binary],name=='asan')
                    assert 'TLS Dial ownership:' in run(label+'.single.native-run',[binary,'native'],name=='asan')
            except Exception as e:r['Failures'].append(dict(Name=label,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Stages']=len(r['Rows'])
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
