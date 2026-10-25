"""TLS selector identity ownership with real fixed-key in-memory handshakes.

No sockets, external endpoints, malformed traffic or crypto security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True);p.add_argument('--xlang',type=Path)
    p.add_argument('--modular',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    case=root/'tests/tls/test_tls_server_choice_identity.c'
    inputs={Path(__file__).resolve(),case,gcc,tcc,root/'config/modules.json',root/'tests/test.h'}
    inputs.update(q for folder in ['src','include','tests/fixtures'] for q in (root/folder).rglob('*') if q.is_file() and q.suffix in ['.h','.c'])
    inputs.update(root/'single'/s for s in ['xrt.h','xrt_decl.h'])
    if a.modular:inputs.add(root/'tools/xrt_manifest.py')
    if a.xlang:inputs.update(a.xlang.resolve()/s for s in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h'])
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(inputs)},Rows=[],Failures=[],Products=[],
        NetworkTraffic=False,NetworkSecurityTests=False,MalformedCryptoTests=False,RealFixedCertificateHandshakes=True,
        LanguageCallbackAccepted=False,ShippingAccepted=False,NativeExecutions=0,ModularSources=[])
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if Path(args[0]) in [gcc,tcc]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n',encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=900).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    def check(name,binary):
        assert 'TLS selector identity: borrowed and temporary results; replacement/rejection/checked errors; 64 OOM prefixes balanced' in run(name,[binary])
        r['NativeExecutions']+=1;save()
    modules=['TLS_CLIENT_VERIFY','TLS_SERVER','TLS_KEY_EXCHANGE_X25519','TLS_RECORD_AES','TLS_SCHEDULE_SHA256','TLS_IDENTITY_RSA','PEM','MEMORY_DEBUG']
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    variants=[('native',root,'single/xrt.h','single/xrt_decl.h','XRT_IMPLEMENTATION','-DXRT_CHOICE_SINGLE')]
    if a.xlang:
        xl=a.xlang.resolve();variants += [('core',xl,'lib/xrt/xrt.h','release/include/xrt_decl.h','XRT_IMPLEMENTATION','-DXRT_CHOICE_XLANG_CORE'),
            ('runtime',xl,'lib/xruntime/xruntime.h','release/include/xruntime_decl.h','XRUNTIME_IMPLEMENTATION','-DXRT_CHOICE_XLANG_RUNTIME')]
    try:
        if a.modular:
            sys.path.insert(0,str(root/'tools'));from xrt_manifest import load_manifest,dependency_closure
            selected=dependency_closure([m.lower() for m in modules],load_manifest(root/'config/modules.json')['modules'])
            sources=list(dict.fromkeys(s for m in selected for s in m['sources']));r['ModularSources']=sources
            flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'include'),*['-D'+m['feature'] for m in selected if m.get('feature')]]
            if os.name!='nt':flags+=['-D_GNU_SOURCE']
            objects=[]
            for index,path in enumerate(sources):
                obj=out/('modular.'+str(index)+'.o');run('modular.'+str(index),[gcc,*flags,'-c',root/path,'-o',obj]);objects.append(obj)
            binary=out/('modular'+('.exe' if os.name=='nt' else ''))
            run('modular.link',[gcc,*flags,case,*objects,*libraries,'-o',binary]);check('modular.run',binary)
        for product,folder,header,declarations,macro,decl in variants:
            source=out/(product+'.c');source.write_text('#define '+macro+'\n#include "'+header+'"\n',encoding='utf8')
            shim=out/(product+'.include');shim.mkdir();(shim/'xrt.h').write_text('#include "'+(folder/declarations).as_posix()+'"\n',encoding='utf8')
            flags=['-std=c11','-I'+str(shim),'-I'+str(folder),'-I'+str(root/'include'),*['-DXRT_MODULE_'+m for m in modules]]
            if os.name!='nt':flags+=['-D_GNU_SOURCE']
            for label,cc,options in [('O0',gcc,['-O0']),('O2',gcc,['-O2']),('tcc',tcc,[])]:
                name=product+'.'+label;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                strict=['-Wall','-Wextra','-Werror'] if cc==gcc else []
                run(name+'.implementation',[cc,*flags,*options,*strict,'-c',source,'-o',obj])
                run(name+'.link',[cc,*flags,*options,*strict,decl,case,obj,*libraries,'-o',binary]);check(name+'.run',binary)
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Stages']=len(r['Rows']);r['Complete']=r['InputsUnchanged']
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
