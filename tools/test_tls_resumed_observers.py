"""Base-role status linkage/ownership: no transport, handshake or crypto campaign."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--xlang',type=Path);p.add_argument('--modular',action='store_true');p.add_argument('--case',type=Path);a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    test=a.case.resolve() if a.case else root/'tests/tls/test_tls_resumed_observers.c'
    inputs={Path(__file__).resolve(),test,gcc,tcc,root/'config/modules.json'}
    inputs.update(q for folder in ['include','src'] for q in (root/folder).rglob('*') if q.is_file())
    inputs.update(root/'single'/s for s in ['xrt.h','xrt_decl.h'])
    if a.modular:inputs.add(root/'tools/xrt_manifest.py')
    if a.xlang:
        for name in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:inputs.add(a.xlang.resolve()/name)
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(inputs)},Rows=[],Failures=[],Products=[],NetworkTraffic=False,SecurityTests=False,RealRoleFactories=True,HandshakeExecuted=False,ModularRequested=a.modular,ModularSources={})
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if Path(args[0]) in [gcc,tcc]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n',encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    base=['TLS_CLIENT_VERIFY','TLS_SERVER','TLS_KEY_EXCHANGE_X25519','TLS_RECORD_AES','TLS_SCHEDULE_SHA256','MEMORY_DEBUG']
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    variants=[('native',root,'single/xrt.h','XRT_IMPLEMENTATION','-DXRT_OBSERVER_SINGLE')]
    if a.xlang:
        xl=a.xlang.resolve();variants += [('core',xl,'lib/xrt/xrt.h','XRT_IMPLEMENTATION','-DXRT_OBSERVER_XLANG_CORE'),('runtime',xl,'lib/xruntime/xruntime.h','XRUNTIME_IMPLEMENTATION','-DXRT_OBSERVER_XLANG_RUNTIME')]
    try:
        for enabled in [False,True]:
            modules=base+(['TLS_CLIENT_RESUME','TLS_SERVER_RESUME'] if enabled else [])
            if a.modular:
                sys.path.insert(0,str(root/'tools'));from xrt_manifest import load_manifest,dependency_closure
                selected=dependency_closure([s.lower() for s in modules],load_manifest(root/'config/modules.json')['modules'])
                sources=list(dict.fromkeys(s for m in selected for s in m['sources']))
                stem='modular'+('.resume' if enabled else '.base');r['ModularSources'][stem]=sources
                # Modular translation units include role headers before the
                # public umbrella: use the canonical build's full feature
                # closure, not single-header request selectors.
                defines=['-D'+m['feature'] for m in selected if m.get('feature')]
                flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'include'),*defines]
                if os.name!='nt':flags+=['-D_GNU_SOURCE']
                run(stem+'.case',[gcc,*flags,'-fsyntax-only',test])
                objects=[]
                for index,source in enumerate(sources):
                    obj=out/(stem+'.'+str(index)+'.o');run(stem+'.'+str(index),[gcc,*flags,'-c',root/source,'-o',obj]);objects.append(obj)
                binary=out/(stem+('.exe' if os.name=='nt' else ''))
                run(stem+'.link',[gcc,*flags,test,*objects,*libraries,'-o',binary])
                assert 'TLS resumed observers: real role factories; 256 fresh observations;' in run(stem+'.run',[binary])
            for product,folder,header,implementationMacro,decl in variants:
                stem=product+('.resume' if enabled else '.base');source=out/(stem+'.c')
                source.write_text('#define '+implementationMacro+'\n#include "'+header+'"\n',encoding='utf8')
                flags=['-std=c11','-I'+str(folder),'-I'+str(root/'include'),*['-DXRT_MODULE_'+s for s in modules]]
                if os.name!='nt':flags+=['-D_GNU_SOURCE']
                for label,cc,options in [('O0',gcc,['-O0']),('O2',gcc,['-O2']),('tcc',tcc,[])]:
                    name=stem+'.'+label;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                    strict=['-Wall','-Wextra','-Werror'] if cc==gcc else []
                    run(name+'.implementation',[cc,*flags,*options,*strict,'-c',source,'-o',obj])
                    run(name+'.link',[cc,*flags,*options,*strict,decl,test,obj,*libraries,'-o',binary])
                    assert 'TLS resumed observers: real role factories; 256 fresh observations;' in run(name+'.run',[binary])
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Stages']=len(r['Rows']);r['Complete']=r['InputsUnchanged']
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.iterdir() if q.is_file() and q.name!='results.json'];save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
