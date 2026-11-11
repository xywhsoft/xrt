"""Owned Redis bootstrap-role matrix using the shared PID/run-id/TLS lifecycle."""
from pathlib import Path
import argparse,json,os,platform,shutil,subprocess,tempfile
from run_redis_tls import certificates
from run_redis_tracking import run,sha

def main():
 p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);p.add_argument('--windows-python',type=Path);p.add_argument('--quick',action='store_true');a=p.parse_args();a.client=a.client.absolute();a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True)
 result=dict(schema=1,complete=False,client_sha256=sha(a.client),server_sha256=sha(a.server),server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),platform=platform.platform(),runner_sha256=sha(__file__),cases=[])
 with tempfile.TemporaryDirectory(prefix='role-client-material-',dir=a.report.parent) as temporary:
  material=Path(temporary);certificates(Path(shutil.which('openssl')),material)
  for version in ([0] if a.quick else [0,12,13]):
   for mode in ([0] if a.quick else [0,1,2]):
    for auth in ([0] if a.quick else [0,1,2]):
     result['cases'].append(run(a,material,version,mode,auth));a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(version,mode,auth,'role init passed',flush=True)
 result['complete']=True;result['runtime_material_removed']=not material.exists();a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
