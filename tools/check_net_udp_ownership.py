"""Native base UDP graphs, checked retirement and stable closed tails.
Only offline ownership/OOM and self-owned literal loopback normal traffic.
No external endpoints, DNS, ICMP/error-queue probes or security tests.
Pending Future graph registration remains explicitly unsupported by base V1.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--header',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();header=a.header.resolve()
    assert not out.exists() and (os.name!='nt' or not a.sanitize)
    fixture=root/'tests/network/test_net_udp_ownership.c';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    inputs={str(p):sha(p) for p in (Path(__file__).resolve(),header,fixture,*compilers)}
    out.mkdir(parents=True)
    r=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Scope=__doc__,OrdinaryLiteralLoopback=True,
        ExternalNetworkTests=False,NetworkSecurityTests=False,ManagedLanguageAcceptance=False,PendingUdpFutureGraphComplete=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(p):r['Products'].append(dict(Path=str(p),Sha256=sha(p)));save()
    def command(cc,mode,selected,binary,base=False):
        args=[str(cc),'-std=c11','-Wall','-Wextra','-Werror',*mode,'-DXRT_IMPLEMENTATION',
            '-DXRT_MODULE_NET_UDP'+('' if base else '_FUTURE'),'-DXRT_MODULE_MEMORY_DEBUG','-I'+str(selected.parent)]
        if os.name!='nt':args+=['-D_GNU_SOURCE']
        return args+[str(fixture),*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',str(binary)]
    def run(name,cmd,instrumented=False,marker=None):
        cmd=list(map(str,cmd));log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:done=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        r['Rows'].append(dict(Name=name,Command=cmd,Exit=done.returncode,ExpectedRejection=marker is not None,Log=str(log),Sha256=sha(log)))
        save();print(name,done.returncode,flush=True)
        assert (done.returncode!=0 if marker else done.returncode==0),(name,str(log))
        text=log.read_text(errors='replace')
        if marker:assert marker in text,(name,'expected graph assertion')
        return text
    save()
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for name,mode in modes:
                label=f'{index}.{name}';binary=out/(label+('.exe' if os.name=='nt' else '.run'))
                run(label+'.build',command(cc,mode,header,binary));product(binary)
                text=run(label+'.run',[binary],name=='instrumented')
                assert all(word in text for word in ('retired=2','closed-call-borrows=2','observed-sends=6','OOM=16','balanced'))
        gcc=next(c for c in compilers if c.stem!='tcc')
        for opt in ('O0','O2'):
            label='base.'+opt;binary=out/(label+('.exe' if os.name=='nt' else '.run'))
            run(label+'.build',command(gcc,['-'+opt],header,binary,base=True));product(binary)
            run(label+'.run',[binary])
        binary=out/('0.O0'+('.exe' if os.name=='nt' else '.run'))
        run('missing-runtime.refused',[binary,'missing-runtime'],marker='xrtOwnershipSnapshotCreate(roots,count,slots,count,admit,g,&proof)')
        text=header.read_text(encoding='utf8');needle='if (pVisit && !pVisit(xrtNetUdpPacketOwnership(p), pContext)) return false;'
        assert text.count(needle)==1
        mutant=out/'missing-queue-edge'/'xrt.h';mutant.parent.mkdir();mutant.write_text(text.replace(needle,'(void)pContext;'),encoding='utf8');product(mutant)
        binary=out/('missing-queue-edge'+('.exe' if os.name=='nt' else '.run'))
        run('missing-queue-edge.build',command(gcc,['-O0'],mutant,binary));product(binary)
        run('missing-queue-edge.refused',[binary],marker='!g->queued||packet')
        needle='if (pVisit && !pVisit(xrtNetUdpErrorPacketOwnership(p), pContext)) return false;'
        assert text.count(needle)==1
        mutant=out/'missing-error-edge'/'xrt.h';mutant.parent.mkdir();mutant.write_text(text.replace(needle,'(void)pContext;'),encoding='utf8');product(mutant)
        binary=out/('missing-error-edge'+('.exe' if os.name=='nt' else '.run'))
        run('missing-error-edge.build',command(gcc,['-O0'],mutant,binary));product(binary)
        run('missing-error-edge.refused',[binary],marker='__xrtNetUdpQueues(u,errorEdge,&view)&&view.edges==1')
        r['InputsUnchanged']=all(sha(p)==v for p,v in inputs.items())
        r['LogsUnchanged']=all(sha(p['Log'])==p['Sha256'] for p in r['Rows'])
        r['ProductsUnchanged']=all(sha(p['Path'])==p['Sha256'] for p in r['Products'])
        assert all(r[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        r['Complete']=True;save()
    except BaseException as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
