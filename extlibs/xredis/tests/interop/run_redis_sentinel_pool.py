"""Keep a native C pool alive across owned real Redis/Sentinel failover."""
from pathlib import Path
import argparse,json,subprocess,tempfile,socket,time,traceback,shutil,hashlib,select
from run_redis_role import exchange

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 a=argparse.ArgumentParser();a.add_argument('--client',type=Path,required=True);a.add_argument('--report',type=Path,required=True);a.add_argument('--server',default=shutil.which('redis-server') or 'redis-server');a=a.parse_args()
 client=a.client.absolute();report=a.report.absolute();report.parent.mkdir(parents=True,exist_ok=True)
 result=dict(schema=1,complete=False,client_sha256=sha(client),server_version=subprocess.check_output([a.server,'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),fixtures=[],cases=[],limits=['Plaintext loopback real Redis/Sentinel; TLS/auth are covered only by local managed-client fixtures at this stage.','Each C pool remains alive across a real manual failover with three held old leases and one queued acquisition; old BLPOP is observed server-side before failure and must not replay.']);children=[];logs=[]
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
  def stable():
   due=time.monotonic()+20
   while True:
    endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache']);current=int(endpoint[1]);other=replica if current==master else master
    role,_=exchange(other,['ROLE']);stats,_=exchange(sentinel,['SENTINEL','MASTER','cache']);stats=dict(zip(stats[::2],stats[1::2]));peers,_=exchange(sentinel,['SENTINEL','REPLICAS','cache'])
    good=False
    for item in peers:
     fields=dict(zip(item[::2],item[1::2]));flags=fields.get(b'flags',b'')
     if int(fields[b'port'])==other and not any(x in flags for x in (b's_down',b'o_down',b'disconnected')):good=True
    if role[0]==b'slave' and role[3]==b'connected' and b'failover' not in stats[b'flags'] and good:return current,other
    assert time.monotonic()<due,'replica not ready for the next controlled failover';time.sleep(.05)
  for factory in range(3):
   for protocol in (2,3,0):
    current,target=stable();owned();run=lambda port:next(x['run_id'] for x in result['fixtures'] if x['port']==port)
    args=[str(client),str(sentinel),str(target),run(current),run(target),str(factory),str(protocol),'3'];probe=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);prefix=[]
    case=dict(factory=factory,protocol=protocol,initial_port=current,target_port=target,process_id=probe.pid,ready=False,exit_code=None);result['cases'].append(case);save()
    try:
     due=time.monotonic()+25
     while True:
      ready,_,_=select.select([probe.stdout],[],[],.2)
      if ready:
       line=probe.stdout.readline();prefix.append(line)
       if line.strip()=='READY':case['ready']=True;save();break
      if probe.poll() is not None:raise AssertionError('probe ended before READY: '+''.join(prefix)+probe.stdout.read())
      assert time.monotonic()<due,'C pool did not verify its initial native members'
     owned();exchange(sentinel,['SENTINEL','FAILOVER','cache']);due=time.monotonic()+25
     while True:
      endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache']);new,_=exchange(target,['ROLE']);old,_=exchange(current,['ROLE'])
      if int(endpoint[1])==target and new[0]==b'master' and old[0]==b'slave':break
      assert time.monotonic()<due,'manual failover did not complete';time.sleep(.05)
     output,_=probe.communicate('GO\n',timeout=35);case.update(exit_code=probe.returncode,output=''.join(prefix)+output,probe_reaped=probe.poll() is not None);save();assert probe.returncode==0,case['output']
     owned();print('factory',factory,'protocol',protocol,'persistent pool failover passed',flush=True)
    finally:
     if probe.poll() is None:probe.kill();tail,_=probe.communicate(timeout=5);case['aborted_output']=''.join(prefix)+tail
     case['probe_reaped']=probe.poll() is not None;case['exit_code']=probe.returncode;save()
  result['complete']=True
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
