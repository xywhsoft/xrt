"""Typed immutable batch ordinary own loopback, exact native edges and OOM.
No external endpoints, DNS, malformed protocol or network-security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--header',type=Path);p.add_argument('--fixture',type=Path);p.add_argument('--pinned-timing',action='store_true')
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();header=(a.header or root/'single/xrt.h').resolve()
    fixture=(a.fixture or root/'tests/network/test_net_tcp_bytes_vec.c').resolve();out=a.out.resolve();assert not out.exists()
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),header,fixture,*compilers)};out.mkdir(parents=True)
    r=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],ExternalNetworkTests=False,NetworkSecurityTests=False,OrdinaryLiteralLoopback=True)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(f):r['Products'].append(dict(Path=str(f),Sha256=sha(f)));save()
    def run(name,cmd,instrumented=False):
        cmd=list(map(str,cmd));log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:result=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        r['Rows'].append(dict(Name=name,Command=cmd,Exit=result.returncode,Log=str(log),Sha256=sha(log)));save();print(name,result.returncode,flush=True)
        assert result.returncode==0,(name,str(log));return log.read_text(errors='replace')
    try:
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for name,mode in modes:
                label=f'{index}.{name}';exe=out/(label+('.exe' if os.name=='nt' else ''))
                cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror',*mode,'-DXRT_IMPLEMENTATION','-DXRT_MODULE_NET_TCP_FUTURE','-DXRT_MODULE_MEMORY_DEBUG','-I'+str(header.parent)]
                if a.pinned_timing:cmd+=['-DTEST_PINNED_TIMING']
                if os.name!='nt':cmd+=['-D_GNU_SOURCE']
                cmd+=[fixture,*(['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']),'-o',exe]
                run(label+'.build',cmd);product(exe)
                proof=run(label+'.run',[exe],name=='instrumented');assert 'duplicate native edges' in proof and 'balanced' in proof
        r['InputsUnchanged']=all(sha(f)==s for f,s in inputs.items());r['LogsUnchanged']=all(sha(v['Log'])==v['Sha256'] for v in r['Rows'])
        r['ProductsUnchanged']=all(sha(v['Path'])==v['Sha256'] for v in r['Products']);assert all(r[k] for k in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        r['Complete']=True;save()
    except BaseException as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
