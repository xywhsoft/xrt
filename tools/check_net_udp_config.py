"""Offline UDP configuration boundaries; no sockets, DNS, traffic or probes."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--header',type=Path,action='append',required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();headers=[h.resolve() for h in a.header]
    cc=[Path(shutil.which(c) or c).resolve() for c in a.compiler]
    assert not out.exists() and (not a.sanitize or os.name!='nt');out.mkdir(parents=True)
    fixture=root/'tests/network/test_net_udp_config.c';sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),fixture,*headers,*cc}
    for folder in ('include','src','config'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.json'))
    inputs={str(path):sha(path) for path in sorted(files)}
    report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],NetworkTraffic=False,
        ExternalNetworkTests=False,NetworkSecurityTests=False,OsErrorQueueProbes=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False):
        env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');command=list(map(str,command))
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        report['Rows'].append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,(name,str(log));return log.read_text(errors='replace')
    save()
    try:
        for hi,header in enumerate(headers):
            for ci,compiler in enumerate(cc):
                modes=[('O0',['-O0']),('O2',['-O2'])]
                if a.sanitize and compiler.stem!='tcc':modes.append(('instrumented',
                    ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
                for mode,options in modes:
                    name=f'{hi}.{ci}.{mode}';binary=out/(name+('.exe' if os.name=='nt' else '.run'))
                    flags=[compiler,'-std=c11','-Wall','-Wextra','-Werror',*options,'-DXRT_MODULE_NET_UDP',
                        '-I'+str(root/'include')]
                    if os.name!='nt':flags+=['-D_GNU_SOURCE']
                    includes=['-DXRT_IMPLEMENTATION','-include',root/'single/xrt.h']
                    if header.name=='xruntime.h':includes+=['-DXRUNTIME_IMPLEMENTATION','-include',header]
                    libraries=['-lws2_32','-liphlpapi','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
                    run(name+'.build',[*flags,*includes,fixture,*libraries,'-o',binary]);product(binary)
                    text=run(name+'.run',[binary],mode=='instrumented');assert '33 rejected boundaries' in text
                # The actual modular implementation must compile without a
                # single-header pre-include; this is not a link proof for all XRT.
                obj=out/(f'{hi}.{ci}.modular.o')
                run(f'{hi}.{ci}.modular',[*flags,'-O0','-c',root/'src/network/udp_open.c','-o',obj]);product(obj)
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
        report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
        report['Complete']=all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'));save();assert report['Complete']
    except BaseException as error:report['Failures'].append(repr(error));save();raise

if __name__=='__main__':main()
