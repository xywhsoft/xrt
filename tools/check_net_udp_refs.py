"""Typed UDP ownership, metadata and normal literal-loopback traffic only.

No DNS, public endpoint, OS error queue, ICMP, malformed traffic, fuzzing or
security acceptance. Does not stand for managed xlang or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--header',type=Path,required=True);p.add_argument('--baseline-header',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();header=a.header.resolve();old=a.baseline_header.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    cc=[Path(shutil.which(c) or c).resolve() for c in a.compiler];fixture=root/'tests/network/test_net_udp_refs.c'
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in (Path(__file__).resolve(),fixture,header,old,*cc)}
    out.mkdir(parents=True)
    r=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Scope=__doc__,
        ExternalNetworkTests=False,NetworkSecurityTests=False,OsErrorQueueProbes=False,
        ManagedLanguageAcceptance=False,ShippingLibraryAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(path):r['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def command(compiler,flags,selected,binary,source=fixture,layout=False):
        cmd=[str(compiler),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-DXRT_IMPLEMENTATION',
             '-DXRT_MODULE_NET_UDP','-DXRT_MODULE_MEMORY_DEBUG','-I'+str(selected.parent)]
        if layout:cmd+=['-include',str(selected)]
        if os.name!='nt':cmd+=['-D_GNU_SOURCE']
        return cmd+[str(source),*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',str(binary)]
    def run(name,cmd,instrumented=False,rejection=False,marker=None):
        cmd=list(map(str,cmd));log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:done=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        r['Rows'].append(dict(Name=name,Command=cmd,Exit=done.returncode,ExpectedRejection=rejection,Log=str(log),Sha256=sha(log)));save()
        print(name,done.returncode,flush=True)
        assert (done.returncode!=0 if rejection else done.returncode==0),(name,str(log))
        text=log.read_text(errors='replace')
        if marker:
            markers=(marker,) if isinstance(marker,str) else marker
            assert any(item in text for item in markers),(name,markers,str(log))
        return text
    save()
    try:
        for index,compiler in enumerate(cc):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and compiler.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for name,flags in modes:
                label=f'{index}.{name}';binary=out/(label+('.exe' if os.name=='nt' else '.run'))
                run(label+'.build',command(compiler,flags,header,binary));product(binary)
                for backend in ('select','native'):
                    text=run(label+'.'+backend,[binary,*(['native'] if backend=='native' else [])],name=='instrumented')
                    assert all(marker in text for marker in ('graphs=8','wire=9','prefixes=17','OOM=15','guards=24','close-races=24','balanced'))
        gcc=next(compiler for compiler in cc if compiler.stem!='tcc')
        layout=out/'layout.c';layout.write_text('#include <stdio.h>\nint main(void){printf("%zu %zu\\n",sizeof(__xrt_net_udp_send),offsetof(__xrt_net_udp_send,Tail));return 0;}\n',encoding='utf8');product(layout)
        layouts=[]
        for name,selected in (('old',old),('new',header)):
            binary=out/('layout-'+name+('.exe' if os.name=='nt' else '.run'))
            run('layout.'+name+'.build',command(gcc,['-O0'],selected,binary,layout,True));product(binary)
            layouts.append(run('layout.'+name+'.run',[binary]))
        assert layouts[0]==layouts[1],layouts
        r['OrdinarySendRecordLayoutUnchanged']=True;r['SendRecordLayout']=layouts[1].strip()
        text=header.read_text(encoding='utf8')
        graph_markers=('xrtOwnershipSnapshotNode(proof,i,&n)&&!n.Reachable','n.Reachable==(n.Reference.Data==c.a)')
        controls=[('missing-bytes-edge',
            'if (pVisit && !pVisit(xrtNetBytesOwnership(p->ReleaseContext), pContext)) return false;',
            '(void)pContext;',graph_markers),
            ('missing-packet-edge',
            'if (pVisit && !pVisit(xrtNetUdpPacketOwnership(pPacket), pContext)) return false;',
            '(void)pContext;',graph_markers),
            ('missing-vector-edge',
            'if (pVisit && !pVisit(xrtNetBytesOwnership(ppOwners[j]), pContext)) return false;',
            '(void)pContext;',graph_markers),
            ('lost-submit-unwind',
            '(bReleaseExternal || __xrtNetUdpSendRetained(pSend))',
            'bReleaseExternal','refs==1u+')]
        for name,needle,replacement,marker in controls:
            assert text.count(needle)==1,(name,text.count(needle))
            selected=out/name/'xrt.h';selected.parent.mkdir();selected.write_text(text.replace(needle,replacement),encoding='utf8');product(selected)
            binary=out/(name+('.exe' if os.name=='nt' else '.run'))
            run(name+'.build',command(gcc,['-O0'],selected,binary));product(binary)
            run(name+'.refused',[binary],rejection=True,marker=marker)
        r['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        r['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in r['Rows'])
        r['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in r['Products'])
        assert all(r[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        r['Complete']=True;save()
    except BaseException as error:r['Failures'].append(repr(error));save();raise

if __name__=='__main__':main()
