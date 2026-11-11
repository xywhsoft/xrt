"""Owned real Redis/Sentinel master discovery before and after manual failover."""
from pathlib import Path
import argparse,json,subprocess,tempfile,socket,time,traceback,shutil,hashlib
from run_redis_role import exchange

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 a=argparse.ArgumentParser();a.add_argument('--client',type=Path,required=True);a.add_argument('--report',type=Path,required=True);a.add_argument('--server',default=shutil.which('redis-server') or 'redis-server');a=a.parse_args()
 client=a.client.absolute();report=a.report.absolute();report.parent.mkdir(parents=True,exist_ok=True)
 result=dict(schema=1,complete=False,client_sha256=sha(client),server_version=subprocess.check_output([a.server,'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),fixtures=[],cases=[],limits=['Plaintext loopback real Redis/Sentinel; TLS/auth are covered only by local managed-client fixtures at this stage.','Each probe performs a fresh discovery; persistent pool-wide replacement and outage acceptance remain open.']);children=[];logs=[]
 def save():report.write_text(json.dumps(result,indent=2)+'\n')
 def identity(port):
  data,_=exchange(port,['INFO','SERVER']);return dict(line.split(b':',1) for line in data.split(b'\r\n') if b':' in line)
 def owned():
  for name,port,p in children:
   info=identity(port);expected=next(x for x in result['fixtures'] if x['name']==name);assert p.poll() is None and int(info[b'process_id'])==p.pid and info[b'run_id'].decode()==expected['run_id']
 temporary=tempfile.TemporaryDirectory(prefix='sentinel-discovery ')
 try:
  runtime=Path(temporary.name);ports=[]
  while len(ports)<3:
   with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
   if port not in ports:ports.append(port)
  master,replica,sentinel=ports
  for name,port in zip(('master','replica','sentinel'),ports):
   directory=runtime/name;directory.mkdir();log=(directory/'server.log').open('wb');logs.append((name,log));args=[a.server,'--bind','127.0.0.1','--port',str(port),'--dir',str(directory),'--save','','--appendonly','no','--repl-diskless-sync-delay','0']
   if name=='replica':args+=['--replicaof','127.0.0.1',str(master)]
   if name=='sentinel':
    config=directory/'sentinel.conf';config.write_text(f'bind 127.0.0.1\nport {port}\ndir {json.dumps(str(directory))}\nsentinel monitor cache 127.0.0.1 {master} 1\nsentinel down-after-milliseconds cache 5000\nsentinel failover-timeout cache 10000\n');args=[a.server,str(config),'--sentinel']
   p=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT);children.append((name,port,p));due=time.monotonic()+10
   while True:
    assert p.poll() is None,(name,'process exited',p.returncode)
    try:info=identity(port);assert int(info[b'process_id'])==p.pid;break
    except OSError:
     assert time.monotonic()<due;time.sleep(.02)
   result['fixtures'].append(dict(name=name,port=port,pid=p.pid,run_id=info[b'run_id'].decode()));save()
  due=time.monotonic()+15
  while True:
   value,_=exchange(replica,['ROLE'])
   peers,_=exchange(sentinel,['SENTINEL','REPLICAS','cache'])
   if value[3]==b'connected' and peers:break
   assert time.monotonic()<due,'replica not observed by Sentinel';time.sleep(.05)
  def probe(stage,port,service,status):
   owned();runid=next(x['run_id'] for x in result['fixtures'] if x['port']==port)
   for factory in range(3):
    for protocol in (2,3,0):
     args=[str(client),str(sentinel),str(port),runid,service,str(factory),str(protocol),str(status)];p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=25);result['cases'].append(dict(stage=stage,port=port,service=service,status=status,factory=factory,protocol=protocol,exit_code=p.returncode,output=p.stdout));save();assert p.returncode==0,p.stdout
   print(stage,'9 real discovery probes passed',flush=True)
  probe('initial-master',master,'cache',1);probe('unknown-service',master,'missing',3)
  owned();exchange(sentinel,['SENTINEL','FAILOVER','cache']);due=time.monotonic()+25
  while True:
   endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache']);role,_=exchange(replica,['ROLE']);old,_=exchange(master,['ROLE'])
   if endpoint and int(endpoint[1])==replica and role[0]==b'master' and old[0]==b'slave':break
   assert time.monotonic()<due,'manual failover not completed';time.sleep(.05)
  probe('rediscovered-promoted-master',replica,'cache',1);owned();result['complete']=True
 except BaseException:
  result['failure']=traceback.format_exc();raise
 finally:
  for name,port,p in children:
   if p.poll() is None:p.terminate()
  for name,port,p in children:
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
  for name,log in logs:
   log.close();shutil.copy2(Path(temporary.name)/name/'server.log',report.with_name(report.stem+'-'+name+'.log'))
  temporary.cleanup();result['runtime_material_removed']=not Path(temporary.name).exists();result['processes_terminal']=all(p.poll() is not None for _,_,p in children);save()
if __name__=='__main__':main()
