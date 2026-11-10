from pathlib import Path
import argparse,os,subprocess,sys,hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[4]
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler',default='gcc');p.add_argument('--kind',choices=['modular','single','static','shared'],default='modular');p.add_argument('--package-dir',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize');a=p.parse_args()
 sys.path.insert(0,str(ROOT/'tools'));import build
 sources,_,_,_,defines,links,_,_,includes,headers=build._load_modules('xredis_tracking,xredis_tls,tls_identity_builtin,pem,time',[ROOT/'extlibs/xredis/config/modules.json'])
 arch='native' if os.name=='nt' else 'x64';flags=['-O2']
 for v in includes:flags+=['-I',str(ROOT/v)]
 if a.sanitize:
  flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+a.sanitize]
  if a.sanitize=='thread':flags+=['-Wno-error=tsan']
 out=a.output.absolute();out.parent.mkdir(parents=True,exist_ok=True)
 source=Path(__file__).with_name('redis_tracking.c')
 command=[a.compiler,*build._compile_options(a.compiler,arch,defines,flags)]
 if a.kind=='single':
  wrapper=out.with_suffix('.c');wrapper.write_text('#include <xredis/features.h>\n#define XRT_IMPLEMENTATION\n#include "'+(ROOT/'single/xrt.h').as_posix()+'"\n#define XREDIS_IMPLEMENTATION\n#include "'+(ROOT/'single/extlibs/xredis.h').as_posix()+'"\n#include "'+source.as_posix()+'"\n',encoding='utf-8');command+=[str(wrapper)]
 else:
  command+=[str(source)]
  if a.kind=='modular':command+=[str(v) for v in build._compile_objects(a.compiler,arch,sources,defines,out.parent/(out.stem+'-obj'),False,flags,headers)]
  else:
   assert a.package_dir is not None;pkg=a.package_dir.resolve();suffix='.a' if a.kind=='static' else '.dll.a' if os.name=='nt' else '.so'
   command+=[str(pkg/('lib'+name+suffix)) for name in ['xredis','xrt']]
   if a.kind=='shared':
    command+=['-DXRT_USE_SHARED','-DXREDIS_USE_SHARED']
    if os.name!='nt':command+=['-Xlinker','-rpath','-Xlinker',str(pkg)]
 command+=['-l'+n for n in links]+['-o',str(out)]
 if a.sanitize:command+=['-fsanitize='+a.sanitize]
 subprocess.run(command,check=True,cwd=ROOT,timeout=1800)
 if a.kind=='shared' and os.name=='nt':
  for n in ['xredis','xrt']:shutil.copy2(a.package_dir/(n+'.dll'),out.parent/(n+'.dll'))
 (out.with_suffix('.build.json')).write_text(json.dumps({'command':command,'binary_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()},indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
