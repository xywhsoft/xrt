"""Disposable automatic subscriber session matrix using shared PID/run-id/TLS fixture lifecycle."""
from pathlib import Path
import argparse,json,os,platform,shutil,subprocess,tempfile
from run_redis_tls import certificates
from run_redis_tracking import run,sha
def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);p.add_argument('--quick',action='store_true');p.add_argument('--windows-python',type=Path);a=p.parse_args()
    a.client=a.client.resolve(strict=True);a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True);a.pubsub_channels=True
    assert a.windows_python or a.client.suffix.lower()!='.exe' or os.name=='nt'
    result=dict(schema=1,complete=False,client_sha256=sha(a.client),server_sha256=sha(a.server),server_version=subprocess.check_output([str(a.server),'--version'],encoding='utf-8').strip(),platform=platform.platform(),protocols_per_case=[2,3],runner_sha256=sha(__file__),helper_sha256={name:sha(Path(__file__).with_name(name)) for name in ('run_redis_tracking.py','run_redis_tls.py','health_tracking.py','redis_init.c')},cases=[])
    with tempfile.TemporaryDirectory(prefix='session-material-',dir=a.report.parent) as temporary:
        material=Path(temporary);certificates(Path(shutil.which('openssl')),material);result['public_certificate_sha256']={n:sha(material/n) for n in ('ca.pem','server.pem')}
        for version in ([0] if a.quick else [0,12,13]):
            for mode in ([0] if a.quick else [0,1,2]):
                for auth in ([0] if a.quick else [0,1,2]):
                    result['cases'].append(run(a,material,version,mode,auth));a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(version,mode,auth,'session RESP2/3 passed',flush=True)
    result['complete']=True;result['runtime_material_removed']=not material.exists();a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
