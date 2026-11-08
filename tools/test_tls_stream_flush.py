"""Explicit control-record flush: normal literal-loopback TLS, no public DNS.

Modular and native/core/runtime single products, GCC O0/O2 and genuine TCC.
Only ordinary valid handshakes, ticket rotation, KeyUpdate, bounded queue flow
and API/lifecycle checks; no malformed crypto or security-test campaign.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--xlang',type=Path)
    parser.add_argument('--modular',action='store_true')
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    names=['resume','invalid','baseline','backpressure']
    leaves=['test_tls_stream_resume.c','test_tls_stream_invalid.c','test_tls_stream.c','test_tls_stream_backpressure.c']
    cases=[root/'tests/tls'/leaf for leaf in leaves]
    inputs={Path(__file__).resolve(),gcc,tcc,root/'config/modules.json',root/'tools/xrt_manifest.py',root/'tests/test.h',*cases}
    inputs.update(q for folder in ('src','include','tests/fixtures') for q in (root/folder).rglob('*') if q.is_file() and q.suffix in ('.h','.c'))
    inputs.update(root/'single'/s for s in ('xrt.h','xrt_decl.h'))
    if args.xlang:inputs.update(args.xlang.resolve()/s for s in ('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h'))
    result=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(inputs)},Rows=[],Failures=[],ModularSources=[],NativeExecutions=0,
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,
        ExplicitControlFlushAccepted=False,LanguageControlApiAccepted=False,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));actual=command
        if Path(command[0]) in (gcc,tcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+q.replace('\\','/')+'"' for q in command[1:]),encoding='utf8')
            actual=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:
            code=subprocess.run(actual,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=600).returncode
        result['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start))
        save();print(name,code,flush=True);assert code==0,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    def execute(name,binary,index):
        text=run(name,[binary])
        if index==0:assert 'explicit ticket/KeyUpdate flush and allocation-free empty flush' in text
        elif index!=1:assert '[PASS]' in text
        result['NativeExecutions']+=1;save()
    modules=['TLS_STREAM','TLS_CLIENT_RESUME','TLS_SERVER_RESUME','TLS_CLIENT_VERIFY','TLS_IDENTITY_RSA',
             'TLS_KEY_EXCHANGE_X25519','TLS_RECORD_AES','TLS_SCHEDULE_SHA256','NET_PORT_SELECT','PEM','MEMORY_DEBUG']
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    variants=[('native',root,'single/xrt.h','single/xrt_decl.h','XRT_IMPLEMENTATION')]
    if args.xlang:
        xl=args.xlang.resolve();variants.extend([('core',xl,'lib/xrt/xrt.h','release/include/xrt_decl.h','XRT_IMPLEMENTATION'),
            ('runtime',xl,'lib/xruntime/xruntime.h','release/include/xruntime_decl.h','XRUNTIME_IMPLEMENTATION')])
    try:
        if args.modular:
            sys.path.insert(0,str(root/'tools'));from xrt_manifest import load_manifest,dependency_closure
            selected=dependency_closure([m.lower() for m in modules],load_manifest(root/'config/modules.json')['modules'])
            sources=list(dict.fromkeys(s for m in selected for s in m['sources']));result['ModularSources']=sources
            flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'include'),
                   *['-D'+m['feature'] for m in selected if m.get('feature')],*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
            objects=[]
            for i,source in enumerate(sources):
                obj=out/('modular.'+str(i)+'.o');run('modular.'+str(i),[gcc,*flags,'-c',root/source,'-o',obj]);objects.append(obj)
            for i,case in enumerate(cases):
                name='modular.'+names[i];binary=out/(name+('.exe' if os.name=='nt' else ''))
                run(name+'.link',[gcc,*flags,case,*objects,*links,'-o',binary]);execute(name+'.run',binary,i)
        for product,folder,header,declarations,macro in variants:
            source=out/(product+'.c');source.write_text('#define '+macro+'\n#include "'+header+'"\n',encoding='utf8')
            shim=out/(product+'.include');shim.mkdir();(shim/'xrt.h').write_text('#include "'+(folder/declarations).as_posix()+'"\n',encoding='utf8')
            flags=['-std=c11','-I'+str(shim),'-I'+str(folder),'-I'+str(root/'include'),
                   *['-DXRT_MODULE_'+m for m in modules],*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
            for label,cc in [('O0',gcc),('O2',gcc),('tcc',tcc)]:
                name=product+'.'+label;obj=out/(name+'.o');strict=['-'+label,'-Wall','-Wextra','-Werror'] if cc==gcc else []
                run(name+'.implementation',[cc,*flags,*strict,'-c',source,'-o',obj])
                for i,case in enumerate(cases):
                    test=name+'.'+names[i];binary=out/(test+('.exe' if os.name=='nt' else ''))
                    run(test+'.link',[cc,*flags,*strict,case,obj,*links,'-o',binary]);execute(test+'.run',binary,i)
        result['InputsUnchanged']=all(sha(p)==h for p,h in result['Inputs'].items())
        result['Stages']=len(result['Rows']);result['ExplicitControlFlushAccepted']=result['InputsUnchanged']
        result['Complete']=result['ExplicitControlFlushAccepted']
    except Exception as error:result['Failures'].append(repr(error));save();raise
    finally:
        result['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.rglob('*') if p.is_file() and p.name!='results.json'];save()
    assert result['Complete']
if __name__=='__main__':main()
