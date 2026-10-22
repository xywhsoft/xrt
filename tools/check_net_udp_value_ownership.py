"""Native UDP value owners, normal self-owned loopback and offline OOM.
No DNS/public endpoints, ICMP/error-queue probes, malformed protocols or
network-security tests. Not managed UDP socket/Future or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--header',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();header=a.header.resolve();out=a.out.resolve()
    assert not out.exists() and (os.name!='nt' or not a.sanitize)
    fixture=root/'tests/network/test_net_udp_value_ownership.c'
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),header,fixture,*compilers)}
    out.mkdir(parents=True)
    r=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Scope=__doc__,
        ExternalNetworkTests=False,NetworkSecurityTests=False,OrdinaryLiteralLoopback=True,
        ManagedUdpSocketAcceptance=False,ShippingLibraryAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(f):r['Products'].append(dict(Path=str(f),Sha256=sha(f)));save()
    def run(name,cmd,instrumented=False,reject=False,marker=None):
        cmd=list(map(str,cmd));env=dict(os.environ);log=out/(name+'.log')
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:done=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        r['Rows'].append(dict(Name=name,Command=cmd,Exit=done.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log)))
        save();print(name,done.returncode,flush=True)
        assert (done.returncode!=0 if reject else done.returncode==0),(name,str(log))
        text=log.read_text(errors='replace')
        if reject:assert marker and marker in text,(name,'expected ownership assertion must actually reject')
        return text
    def command(cc,mode,selected,binary):
        cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror',*mode,'-DXRT_IMPLEMENTATION',
             '-DXRT_MODULE_NET_UDP_FUTURE','-DXRT_MODULE_THREAD','-DXRT_MODULE_MEMORY_DEBUG','-I'+str(selected.parent)]
        if os.name!='nt':cmd+=['-D_GNU_SOURCE']
        return cmd+[fixture,*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',binary]
    save()
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for name,mode in modes:
                label=f'{index}.{name}';exe=out/(label+('.exe' if os.name=='nt' else ''))
                run(label+'.build',command(cc,mode,header,exe));product(exe)
                text=run(label+'.run',[exe],name=='instrumented')
                assert 'concurrent-takes=64 winners=8' in text and 'balanced' in text
        needle='if (pPacket && !pVisit(xrtNetUdpPacketOwnership(pPacket), pContext)) return false;'
        text=header.read_text(encoding='utf8');assert text.count(needle)==1
        mutated=out/'missing-batch-edge'/'xrt.h';mutated.parent.mkdir();mutated.write_text(text.replace(needle,'(void)pPacket; (void)pContext;'),encoding='utf8');product(mutated)
        gcc=next(c for c in compilers if c.stem!='tcc');exe=out/('missing-batch-edge'+('.exe' if os.name=='nt' else '.run'))
        run('missing-batch-edge.build',command(gcc,['-O0'],mutated,exe));product(exe)
        run('missing-batch-edge.refused',[exe],reject=True,marker='xrtOwnershipSnapshotNodeCount(proof)==nodes')
        start=text.index('XRT_API xnetudppacket* xrtNetUdpBatchTake(',text.index('/* source: src/network/udp_future.c */'))
        end=text.index('static bool __xrtNetUdpBatchCount(',start);body=text[start:end]
        assert body.count('if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;')==1
        assert body.count('if (!xrtOwnershipScopeEnd(&Mutation)) abort();')==2
        body=body.replace('xrtownershipscope Mutation = {0};','').replace(
            'if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;','').replace('if (!xrtOwnershipScopeEnd(&Mutation)) abort();','')
        mutated=out/'unguarded-batch-take'/'xrt.h';mutated.parent.mkdir()
        mutated.write_text(text[:start]+body+text[end:],encoding='utf8');product(mutated)
        exe=out/('unguarded-batch-take'+('.exe' if os.name=='nt' else '.run'))
        run('unguarded-batch-take.build',command(gcc,['-O0'],mutated,exe));product(exe)
        run('unguarded-batch-take.refused',[exe],reject=True,marker='xrtOwnershipSnapshotNodeCount(proof)==5')
        r['InputsUnchanged']=all(sha(f)==v for f,v in inputs.items())
        r['LogsUnchanged']=all(sha(v['Log'])==v['Sha256'] for v in r['Rows'])
        r['ProductsUnchanged']=all(sha(v['Path'])==v['Sha256'] for v in r['Products'])
        assert all(r[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        r['Complete']=True;save()
    except BaseException as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
