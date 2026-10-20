"""Offline TCP configuration predicates, modular/single GCC/TCC and sanitizers.
No transport creation, Socket, DNS, packets or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from build import _load_modules,platform_name

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    sources,_,_,_,defines,links,*_=_load_modules('net_tcp,memory_debug',platform=platform_name())
    fixtures=('tests/network/test_net_tcp_config.c','tests/single/test_single_net_tcp_config.c')
    files={p for area in ('include','src') for p in (root/area).rglob('*') if p.is_file() and p.suffix in ('.c','.h')}
    files.update(root/name for name in (*fixtures,'single/xrt.h','single/xrt_decl.h','tests/test.h','config/modules.json'))
    files.update((Path(__file__).resolve(),*compilers));inputs={str(p):sha(p) for p in files}
    rows,products,failures=[],[],[];out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,Sources=sources,NetworkTraffic=False,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ShippingLibraryAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,instrumented=False):
        command=list(map(str,command));invocation=command
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+x.replace('\\','/')+'"' for x in command[1:])+'\n',encoding='utf-8')
            invocation=[command[0],'@'+str(rsp)];products.append(dict(Path=str(rsp),Sha256=sha(rsp)))
        env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log')
        with log.open('wb') as stream:result=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=result.returncode,Log=str(log),Sha256=sha(log)))
        save();print(name+': '+str(result.returncode),flush=True);assert result.returncode==0,(name,str(log))
    save()
    for number,cc in enumerate(compilers):
        modes=[('O0',['-O0']),('O2',['-O2'])]
        if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
        for mode,flags in modes:
            for package,fixture in zip(('modular','single'),fixtures):
                label=f'{number}.{mode}.{package}';binary=out/(label+('.exe' if os.name=='nt' else ''))
                try:
                    common=[cc,'-Wall','-Wextra','-Werror',*flags,'-Iinclude']
                    if cc.stem!='tcc':common+=['-std=c11']
                    if os.name!='nt':common+=['-D_GNU_SOURCE']
                    common+=['-D'+name for name in defines]
                    libraries=['-l'+name for name in links]
                    if os.name!='nt':libraries+=['-pthread','-lm']
                    run(label+'.build',[*common,fixture,*(sources if package=='modular' else []),*libraries,'-o',binary])
                    products.append(dict(Path=str(binary),Sha256=sha(binary)));save()
                    run(label+'.run',[binary],mode=='instrumented')
                except Exception as error:failures.append(dict(Name=label,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(p)==digest for p,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],failures
if __name__=='__main__':main()
