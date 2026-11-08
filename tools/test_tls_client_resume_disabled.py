"""Valid client credential/config rejection when the resume overlay is absent.

Modular plus native/core/runtime products, GCC O0/O2 and genuine TCC.
No handshake, network traffic, malformed crypto or security test campaign.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--xlang',type=Path);p.add_argument('--modular',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    case=root/'tests/tls/test_tls_client_resume_disabled.c'
    inputs={Path(__file__).resolve(),case,gcc,tcc,root/'config/modules.json',root/'tools/xrt_manifest.py'}
    inputs.update(q for folder in ('src','include') for q in (root/folder).rglob('*') if q.is_file() and q.suffix in ('.h','.c'))
    inputs.update(root/'single'/s for s in ('xrt.h','xrt_decl.h'))
    if a.xlang:inputs.update(a.xlang.resolve()/s for s in ('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h'))
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(inputs)},Rows=[],Failures=[],Products=[],ModularSources=[],
        NetworkTraffic=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if Path(args[0]) in (gcc,tcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+q.replace('\\','/')+'"' for q in args[1:]),encoding='utf8');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=600).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(encoding='utf8',errors='replace')
    modules=['TLS_CLIENT_VERIFY','TLS_RESUME','TLS_KEY_EXCHANGE_X25519','TLS_RECORD_AES','TLS_SCHEDULE_SHA256','MEMORY_DEBUG']
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    variants=[('native',root,'single/xrt.h','single/xrt_decl.h','XRT_IMPLEMENTATION','XRT_CLIENT_RESUME_SINGLE')]
    if a.xlang:
        xl=a.xlang.resolve();variants.extend([('core',xl,'lib/xrt/xrt.h','release/include/xrt_decl.h','XRT_IMPLEMENTATION','XRT_CLIENT_RESUME_XLANG_CORE'),
            ('runtime',xl,'lib/xruntime/xruntime.h','release/include/xruntime_decl.h','XRUNTIME_IMPLEMENTATION','XRT_CLIENT_RESUME_XLANG_RUNTIME')])
    try:
        if a.modular:
            sys.path.insert(0,str(root/'tools'));from xrt_manifest import load_manifest,dependency_closure
            selected=dependency_closure([m.lower() for m in modules],load_manifest(root/'config/modules.json')['modules'])
            assert not any(m['name']=='tls_client_resume' for m in selected)
            sources=list(dict.fromkeys(s for m in selected for s in m['sources']));r['ModularSources']=sources
            flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'include'),*['-D'+m['feature'] for m in selected if m.get('feature')],*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
            objects=[]
            for index,path in enumerate(sources):
                obj=out/('modular.'+str(index)+'.o');run('modular.'+str(index),[gcc,*flags,'-c',root/path,'-o',obj]);objects.append(obj)
            binary=out/('modular'+('.exe' if os.name=='nt' else ''));run('modular.link',[gcc,*flags,case,*objects,*links,'-o',binary])
            assert 'default verified config unchanged, no traffic, balanced: ok' in run('modular.run',[binary])
        for product,folder,header,declarations,macro,define in variants:
            source=out/(product+'.c');source.write_text('#define '+macro+'\n#include "'+header+'"\n',encoding='utf8')
            shim=out/(product+'.include');shim.mkdir();(shim/'xrt.h').write_text('#include "'+(folder/declarations).as_posix()+'"\n',encoding='utf8')
            flags=['-std=c11','-I'+str(shim),'-I'+str(folder),'-I'+str(root/'include'),*['-DXRT_MODULE_'+m for m in modules],*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
            for opt,cc in [('O0',gcc),('O2',gcc),('tcc',tcc)]:
                name=product+'.'+opt;obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
                options=['-'+opt,'-Wall','-Wextra','-Werror'] if cc==gcc else []
                run(name+'.implementation',[cc,*flags,*options,'-c',source,'-o',obj]);run(name+'.link',[cc,*flags,*options,'-D'+define,case,obj,*links,'-o',binary])
                assert 'default verified config unchanged, no traffic, balanced: ok' in run(name+'.run',[binary])
        r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items());r['Stages']=len(r['Rows']);r['Complete']=r['InputsUnchanged']
    except Exception as e:r['Failures'].append(repr(e));save();raise
    finally:
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save()
    assert r['Complete']
if __name__=='__main__':main()
