"""Run ordinary owned Dial lifecycle against an explicit pinned implementation.

Uses the native fixture with an explicit ms-to-microsecond test adapter; does
not rewrite the implementation, its fields, or its duration/deadline ABI.
Only fixed-key explicit loopback. No external network, DNS or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from build import _load_modules,platform_name

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--implementation',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    implementation=a.implementation.resolve();sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    _,_,_,_,defines,links,*_=_load_modules('tls_dial_ownership_tests',platform=platform_name())
    source=out/'pinned.c';source.write_text('#define XRUNTIME_IMPLEMENTATION\n#define XRT_IMPLEMENTATION\n#include "'+implementation.as_posix()+'"\n'
        '#define TLS_DIAL_OWNERSHIP_PINNED\n#define TLS_DIAL_TEST_DURATION(n) ((uint64)(n)*1000u)\n'
        '#define TLS_DIAL_TEST_DEADLINE xdeadline\n'
        '#define __xrtWaitAfter(n) xrtDeadlineAfter((uint64)(n)*1000u)\n'
        '#define __xrtWaitExpired(n) xrtDeadlineExpired(n)\n'
        '#define xrtFutureWaitFor(f,n) (xrtFutureWaitFor((f),(uint64)(n)*1000u))\n'
        '#include "'+(root/'tests/tls/test_tls_dial_ownership.c').as_posix()+'"\n')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler]
    paths=[Path(__file__),implementation,source,root/'tests/tls/test_tls_dial_ownership.c',root/'tests/test.h',root/'config/modules.json',root/'tools/build.py',*compilers]
    paths += [q for folder in ['include','tests/fixtures'] for q in (root/folder).rglob('*.h')]
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in paths},Rows=[],Failures=[],Products=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,
        OriginalTimeAbiPreserved=True,TestDurationUnit='milliseconds-to-pinned-microseconds',ShippingLibraryAcceptance=False,FullTlsAcceptance=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args,sanitize=False):
        args=list(map(str,args));command=args
        if args[0] in list(map(str,compilers)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic();env=dict(os.environ)
        if sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        row=dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start)
        r['Rows'].append(row);save();print(name,code,flush=True);assert not code,str(log)
        if not name.endswith('.build'):
            match=re.search(r'TLS Dial ownership: (\d+) graphs; (\d+) proofs; (\d+) refusals; (\d+) persistent failures; (\d+)/(\d+) budgets; balanced',log.read_text())
            assert match,str(log);row['ObservedCounters']=list(map(int,match.groups()));assert row['ObservedCounters'][0]==row['ObservedCounters'][1] and row['ObservedCounters'][3]==5 and row['ObservedCounters'][5]>0
    for index,cc in enumerate(compilers):
        modes=[('tcc',[])] if cc.stem=='tcc' else [('O0',['-O0']),('O2',['-O2'])]
        if a.sanitize and cc.stem!='tcc':
            assert os.name!='nt';modes.append(('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
        for mode,opt in modes:
            name=str(index)+'.'+mode;binary=out/(name+('.exe' if os.name=='nt' else ''))
            flags=['-Wall','-Wextra','-Werror','-I'+str(root/'include'),*opt,*(['-std=c11'] if cc.stem!='tcc' else []),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),*['-D'+d for d in defines]]
            libs=['-l'+s for s in links]+([] if os.name=='nt' else ['-pthread','-lm'])
            try:
                run(name+'.build',[cc,*flags,source,*libs,'-o',binary]);run(name+'.select',[binary],mode=='asan');run(name+'.native',[binary,'native'],mode=='asan')
            except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Stages']=len(r['Rows'])
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
