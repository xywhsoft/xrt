"""Public modular/full/declaration-only FinalizationV1 ABI in C11/C++11/C++17.

This validates the independent type and four outcome combinations. It does not
claim to validate a collector implementation, which is a separate consumer.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 cc=shutil.which('gcc');cxx=shutil.which('g++');assert cc and cxx
 source=root/'tests/core/test_ownership_finalization_v1.c'
 files=[Path(__file__).resolve(),Path(cc),Path(cxx),source,root/'include/xrt/core.h',root/'single/xrt.h',root/'single/xrt_decl.h'];inputs={str(v):sha(v) for v in files};report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def run(name,command):
  started=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=root,capture_output=True,timeout=60);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,seconds=round(time.monotonic()-started,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert r.returncode==0,log
 for standard,compiler in [('c11',cc),('c++11',cxx),('c++17',cxx)]:
  for form,macros in [('module',[]),('full',['-DXRT_TEST_SINGLE_FULL']),('decl',['-DXRT_TEST_SINGLE_DECL'])]:
   for opt in ['O0','O2']:
    name=standard.replace('+','p')+'-'+form+'-'+opt;exe=out/(name+('.exe' if os.name=='nt' else ''));run(name+'-build',[compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-'+opt,'-I'+str(root),'-I'+str(root/'include'),*macros,source,'-o',exe]);run(name+'-run',[exe])
 report['inputsUnchanged']=all(sha(v)==h for v,h in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
