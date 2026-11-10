"""Pinned disposable Redis Tracking matrix; identity verified before mutation."""
from pathlib import Path
import argparse,hashlib,json,os,platform,shutil,socket,ssl,subprocess,tempfile,time
from run_redis_tls import certificates
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def command(connection,*args):
 wire=b'*'+str(len(args)).encode()+b'\r\n'+b''.join(b'$'+str(len(v)).encode()+b'\r\n'+v+b'\r\n' for v in args);connection.sendall(wire)
 stream=connection.makefile('rb');line=stream.readline()
 if line[:1]==b'+':return line[1:-2]
 if line[:1]==b'-':raise RuntimeError(line.decode('utf-8',errors='replace'))
 assert line[:1]==b'$',line
 size=int(line[1:]);data=stream.read(size);assert stream.read(2)==b'\r\n';return data

def run(args,material,version,mode,scenario):
 with socket.socket() as reservation:reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
 native=args.client.suffix.lower()=='.exe' and os.name!='nt'
 hostpath=lambda p:subprocess.check_output(['wslpath','-w',str(p)],encoding='utf-8').strip() if native else str(p)
 ca=material/'ca.pem';tls=ssl.create_default_context(cafile=str(ca)) if version else None
 with tempfile.TemporaryDirectory(prefix='xredis-tracking-peer-') as temporary:
  root=Path(temporary);auth='' if scenario==0 else 'requirepass xredis-fixture-password\n' if scenario==1 else 'user default off\nuser xredis-fixture-user on >xredis-fixture-password ~* +@all\n'
  listener=f'port {port}\n'
  if version:listener=f'port 0\ntls-port {port}\ntls-auth-clients no\ntls-cert-file "{material}/server.pem"\ntls-key-file "{material}/server.key"\ntls-ca-cert-file "{ca}"\ntls-protocols "TLSv1.{version-10}"\n'
  config=root/'redis.conf';config.write_text('bind 127.0.0.1\nprotected-mode yes\nsave ""\nappendonly no\n'+listener+auth,encoding='utf-8')
  with (root/'server.log').open('w',encoding='utf-8') as log:
   process=subprocess.Popen([str(args.server),str(config)],cwd=root,stdout=log,stderr=subprocess.STDOUT)
   try:
    end=time.monotonic()+10;server_info={}
    while time.monotonic()<end:
     if process.poll() is not None:raise RuntimeError((root/'server.log').read_text(encoding='utf-8',errors='replace'))
     try:
      with socket.create_connection(('127.0.0.1',port),timeout=0.5) as raw:
       with (tls.wrap_socket(raw,server_hostname='localhost') if tls else raw) as connection:
        if scenario:command(connection,*([b'AUTH',b'xredis-fixture-password'] if scenario==1 else [b'AUTH',b'xredis-fixture-user',b'xredis-fixture-password']))
        data=command(connection,b'INFO',b'SERVER');server_info=dict(line.split(b':',1) for line in data.split(b'\r\n') if b':' in line)
        assert int(server_info[b'process_id'])==process.pid
      if native:
       health=[str(args.windows_python),hostpath(Path(__file__).with_name('health_tracking.py')) ,str(port),hostpath(ca) if version else '-']
       check=subprocess.run(health,capture_output=True,encoding='utf-8',timeout=3)
       if check.returncode:time.sleep(0.025);continue
      break
     except (OSError,ssl.SSLError):time.sleep(0.025)
    else:raise TimeoutError('disposable peer/native-loopback startup timed out')
    identity=server_info[b'run_id'].decode('ascii');assert len(identity)==40 and process.poll() is None
    commandline=[str(args.client),str(port),str(scenario),str(mode),str(version),hostpath(ca) if version else '-',identity]
    child=subprocess.Popen(commandline,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',env=os.environ)
    try:stdout,_=child.communicate(timeout=120)
    except BaseException:
     child.kill();child.communicate(timeout=5);raise
    assert child.returncode==0,stdout[-4000:]
    assert process.poll() is None
    row={'version':version,'mode':mode,'auth':scenario,'passed':True,'server_run_id':identity,'server_pid_verified':True,'client_arguments':commandline[1:],'client_stdout':stdout,'probe_reaped':child.poll() is not None}
   finally:
    if process.poll() is None:
     process.terminate()
     try:process.wait(timeout=5)
     except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
   row['server_reaped']=process.poll() is not None;return row

def main():
 p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);p.add_argument('--quick',action='store_true');p.add_argument('--windows-python',type=Path);a=p.parse_args();a.client=a.client.resolve(strict=True);a.server=a.server.absolute();a.report=a.report.resolve();a.report.parent.mkdir(parents=True,exist_ok=True)
 assert a.windows_python or a.client.suffix.lower()!='.exe' or os.name=='nt'
 result={'schema':1,'client_sha256':sha(a.client),'server_sha256':sha(a.server),'server_version':subprocess.check_output([str(a.server),'--version'],encoding='utf-8').strip(),'platform':platform.platform(),'runner_sha256':sha(__file__),'cases':[],'complete':False}
 with tempfile.TemporaryDirectory(prefix='runtime-material-',dir=a.report.parent) as temporary:
  material=Path(temporary);certificates(Path(shutil.which('openssl')),material);result['public_certificate_sha256']={name:sha(material/name) for name in ['ca.pem','server.pem']}
  for version in ([0] if a.quick else [0,12,13]):
   for mode in ([0] if a.quick else [0,1,2]):
    for scenario in ([0] if a.quick else [0,1,2]):
     row=run(a,material,version,mode,scenario);result['cases'].append(row);a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(version,mode,scenario,'passed',flush=True)
 result['complete']=True;result['runtime_material_removed']=not material.exists();a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
