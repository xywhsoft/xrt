"""Keep a native C pool alive across owned real Redis/Sentinel failover."""
from pathlib import Path
import argparse,json,subprocess,tempfile,ssl,time,traceback,shutil,hashlib,select,sys
from run_redis_role import exchange as tcp_exchange
from sentinel_load_report import load_report
from run_redis_sentinel_seeds import ports_for_fixture

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def windows_listeners(client):
 if client.suffix.lower()!='.exe' or sys.platform=='win32':return set()
 # WSL loopback forwarding cannot own a port already listened on by Windows.
 output=subprocess.check_output(
     ['powershell.exe','-NoProfile','-NonInteractive','-Command',
      '(Get-NetTCPConnection -State Listen -ErrorAction Stop).LocalPort'],
     text=True,encoding='ascii',errors='replace',timeout=20)
 return {int(line.strip()) for line in output.splitlines() if line.strip().isdigit()}

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 mode=parser.add_mutually_exclusive_group()
 mode.add_argument('--replicas',action='store_true')
 mode.add_argument('--refresh',action='store_true')
 mode.add_argument('--listen',choices=('master','replica'))
 parser.add_argument('--client',type=Path,required=True)
 parser.add_argument('--report',type=Path,required=True)
 parser.add_argument('--server',default=shutil.which('redis-server') or 'redis-server')
 parser.add_argument('--tls',type=int,choices=(12,13))
 parser.add_argument('--openssl',type=Path,default=shutil.which('openssl'))
 parser.add_argument('--runtime-root',type=Path)
 parser.add_argument('--case',type=int,nargs=2,metavar=('FACTORY','PROTOCOL'))
 parser.add_argument('--load',type=int,nargs=4,metavar=('COUNT','RATE','SLOTS','TIMEOUT_MS'))
 parser.add_argument('--control',action='store_true',help='run the same fixed-arrival workload without requesting failover')
 parser.add_argument('--standby',action='store_true',
                     help='add a non-promotable replica after the pool has leased from the first replica')
 a=parser.parse_args()
 if a.control and not a.load:parser.error('--control requires --load')
 if a.standby and not (a.listen=='replica' and a.load and a.case):
  parser.error('--standby requires --listen replica, --load and one --case')
 if a.load:
  count,rate,slots,timeout=a.load
  if not a.listen or not (0<rate<=100000 and 3*rate<=count<=min(300000,30*rate) and 1<=slots<=1024 and 1<=timeout<=10000):
   parser.error('--load requires --listen, a 3..30 second schedule, 1..1024 slots and a 1..10000 ms deadline')
 if a.tls and (not a.listen or not a.openssl):parser.error('--tls requires --listen and OpenSSL')
 matrix=[(factory,protocol) for factory in range(3) for protocol in (2,3,0)]
 if a.case:
  if tuple(a.case) not in matrix:parser.error('expected factory 0..2 and protocol 0, 2 or 3')
  matrix=[tuple(a.case)]
 a.replicas=a.replicas or a.listen=='replica'
 client=a.client.absolute();report=a.report.absolute();report.parent.mkdir(parents=True,exist_ok=True)
 transport='mutual TLS 1.'+str(a.tls-10)+' with Redis ACL' if a.tls else 'plaintext'
 result=dict(schema=1,complete=False,mode='events-'+a.listen if a.listen else 'replica' if a.replicas else 'refresh' if a.refresh else 'master',transport=transport,client_sha256=sha(client),server_version=subprocess.check_output([a.server,'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),fixtures=[],cases=[],limits=['Owned loopback fixture; server and client run on the same host.','Each C pool keeps three old leases and one queued acquisition; the old blocking command is observed server-side and checked for replay.','Refresh mode remaps Sentinel to another master and confirms the old node and connections remain live before the hint.' if a.refresh else 'Failover mode triggers a real Sentinel election.']);children=[];logs=[]
 if a.load:
  result['load']=dict(schedule=a.load,control=a.control,pool_members=3,engine_workers=2,
      ledger_helper_sha256=sha(Path(__file__).with_name('sentinel_load_report.py')))
  result['limits']=[
      'Disposable loopback topology. One controlled Sentinel FAILOVER request per election case.',
      'Three exclusive pool members; binary PING on each lease. Per-offer latency starts at the planned arrival.',
      'Windows probe may use loopback forwarding to the WSL Redis host. Client and controller clocks are independent.',
  ]
 if a.standby:
  result['limits'].append('The standby joins before the workload starts and has replication priority zero.')
 def save():report.write_text(json.dumps(result,indent=2)+'\n')
 def identity(port):
  data,_=exchange(port,['INFO','SERVER']);return dict(line.split(b':',1) for line in data.split(b'\r\n') if b':' in line)
 def owned():
  for name,port,p in children:
   info=identity(port);expected=next(x for x in result['fixtures'] if x['name']==name);assert p.poll() is None and int(info[b'process_id'])==p.pid and info[b'run_id'].decode()==expected['run_id']
 temporary=tempfile.TemporaryDirectory(prefix='sentinel-discovery ',dir=a.runtime_root)
 try:
  runtime=Path(temporary.name)
  reserved=windows_listeners(client)
  ports=ports_for_fixture(reserved)
  if a.load:result['fixture_port_policy']=dict(range=[38000,47999],windows_listeners_excluded=len(reserved))
  tls=None
  exchange=tcp_exchange
  if a.tls:
   from sentinel_tls_fixture import SentinelTls
   tls=SentinelTls(runtime,a.tls,a.openssl)
   result['public_certificate_sha256']=tls.public_hashes
   result['openssl_sha256']=sha(tls.openssl)
   result['tls_fixture_sha256']=sha(Path(__file__).with_name('sentinel_tls_fixture.py'))
   exchange=tls.exchange
  master,replica,sentinel,standby=ports
  def start(name,port):
   directory=runtime/name;directory.mkdir();log=(directory/'server.log').open('wb');logs.append((name,log));args=[a.server,'--bind','127.0.0.1','--port',str(port),'--dir',str(directory),'--save','','--appendonly','no','--repl-diskless-sync-delay','0']
   if name=='replica':args+=['--replicaof','127.0.0.1',str(master)]
   if name=='standby':args+=['--replica-priority','0']
   if name=='sentinel':
    config=directory/'sentinel.conf';config.write_text(f'bind 127.0.0.1\nport {port}\ndir {json.dumps(str(directory))}\nsentinel monitor cache 127.0.0.1 {master} 1\nsentinel down-after-milliseconds cache 5000\nsentinel failover-timeout cache 10000\n');args=[a.server,str(config),'--sentinel']
   if tls:
    config=directory/'redis.conf'
    config.write_text(tls.config(name,port,master,directory),encoding='utf-8')
    args=[a.server,str(config)]+(['--sentinel'] if name=='sentinel' else [])
   p=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT);children.append((name,port,p));due=time.monotonic()+10
   while True:
    assert p.poll() is None,(name,'process exited',p.returncode)
    try:info=identity(port);assert int(info[b'process_id'])==p.pid;break
    except OSError:
     assert time.monotonic()<due;time.sleep(.02)
   result['fixtures'].append(dict(name=name,port=port,pid=p.pid,run_id=info[b'run_id'].decode()));save()
  for name,port in zip(('master','replica','sentinel'),ports):start(name,port)

  def listed_replica(port):
   peers,_=exchange(sentinel,['SENTINEL','REPLICAS','cache'])
   for entry in peers:
    fields=dict(zip(entry[::2],entry[1::2]))
    flags=fields.get(b'flags',b'')
    if int(fields[b'port'])==port and not any(flag in flags for flag in (b's_down',b'o_down',b'disconnected')):
     return fields
   return None

  def follows(role,primary):
   return (len(role)==5 and role[0]==b'slave' and role[1]==b'127.0.0.1' and
           role[2]==primary and role[3]==b'connected')

  due=time.monotonic()+15
  while True:
   value,_=exchange(replica,['ROLE'])
   peers,_=exchange(sentinel,['SENTINEL','REPLICAS','cache'])
   if value[3]==b'connected' and peers:break
   assert time.monotonic()<due,'replica not observed by Sentinel';time.sleep(.05)
  if tls:
   owned()
   result['policy_checks']=tls.check_policy(sentinel,master)
   save()
  def stable():
   due=time.monotonic()+20
   while True:
    endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache']);current=int(endpoint[1]);other=replica if current==master else master
    role,_=exchange(other,['ROLE']);stats,_=exchange(sentinel,['SENTINEL','MASTER','cache']);stats=dict(zip(stats[::2],stats[1::2]))
    if role[0]==b'slave' and role[3]==b'connected' and b'failover' not in stats[b'flags'] and listed_replica(other):
     return current,other
    assert time.monotonic()<due,'replica not ready for the next controlled failover';time.sleep(.05)
  for factory,protocol in matrix:
   if a.refresh:
    endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache'])
    current=int(endpoint[1]);target=replica if current==master else master
   else:current,target=stable()
   owned();run=lambda port:next(x['run_id'] for x in result['fixtures'] if x['port']==port)
   initial,destination=(target,current) if a.replicas else (current,target)
   if a.control:destination=initial
   initial_run=run(initial)
   if a.standby:
    # Start an isolated standby so its process identity can be supplied to the
    # probe. It joins the real replication group only after the probe is ready.
    start('standby',standby)
    assert listed_replica(standby) is None
    if not a.control:destination=standby
   args=[str(client),str(sentinel),str(destination),initial_run,run(destination),str(factory),str(protocol),'3']
   if a.listen:args.append('events-'+a.listen)
   elif a.replicas:args.append('replica')
   elif a.refresh:args.append('refresh')
   if tls:args.extend(tls.probe_args(client))
   if a.load:args.extend(['load',*map(str,a.load)])
   probe=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);prefix=[]
   case=dict(factory=factory,protocol=protocol,initial_port=initial,target_port=destination,process_id=probe.pid,ready=False,exit_code=None);result['cases'].append(case);save()
   try:
    def expect(marker,seconds):
     due=time.monotonic()+seconds
     while True:
      ready,_,_=select.select([probe.stdout],[],[],.2)
      if ready:
       line=probe.stdout.readline();prefix.append(line)
       if line.strip()==marker:return
      if probe.poll() is not None:raise AssertionError(f'probe ended before {marker}: '+''.join(prefix)+probe.stdout.read())
      assert time.monotonic()<due,f'probe did not reach {marker}'
    expect('READY',25)
    case['ready']=True;save();owned()
    if a.standby:
     exchange(standby,['REPLICAOF','127.0.0.1',str(current)])
     due=time.monotonic()+15
     while True:
      role,_=exchange(standby,['ROLE'])
      fields=listed_replica(standby)
      if follows(role,current) and fields is not None:
       priority=int(fields[b'slave-priority'])
       assert priority==0
       case['standby_before_load']=dict(port=standby,upstream=current,priority=priority)
       case['promotion_port']=target;save();break
      assert time.monotonic()<due,'Sentinel did not see the connected standby'
      time.sleep(.05)
    if a.load:
     probe.stdin.write('GO\n');probe.stdin.flush()
     expect('ELECT',10);marker=time.monotonic()
     case['manual_failover_requests']=0
    if a.control:
     pass
    elif a.refresh:
     exchange(sentinel,['SENTINEL','REMOVE','cache'])
     exchange(target,['REPLICAOF','NO','ONE'])
     exchange(sentinel,['SENTINEL','MONITOR','cache','127.0.0.1',str(target),'1'])
     endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache'])
     old,_=exchange(current,['ROLE']);new,_=exchange(target,['ROLE'])
     clients,_=exchange(current,['CLIENT','LIST'])
     assert int(endpoint[1])==target and old[0]==new[0]==b'master' and b'cmd=blpop' in clients
     case['old_live_before_hint']=True
    else:
     sent=time.monotonic()
     exchange(sentinel,['SENTINEL','FAILOVER','cache'])
     ack=time.monotonic();due=ack+25
     case['manual_failover_requests']=1
     if a.load:case['controller_timing_seconds']=dict(marker_to_send=sent-marker,send_to_ack=ack-sent)
     case['interrupted_control_reads']=0
     if a.standby:
      expect('REPLACED',15)
      replacement_sec=time.monotonic()-sent
      follower,_=exchange(standby,['ROLE'])
      promoted,_=exchange(target,['ROLE'])
      old_role,_=exchange(current,['ROLE'])
      standby_role=follower[0]
      case['standby_at_replacement']=dict(
          send_to_replacement_reply_seconds=replacement_sec,
          standby_role=standby_role.decode(),
          standby_upstream=follower[2] if standby_role==b'slave' else None,
          standby_state=follower[3].decode() if standby_role==b'slave' else None,
          promoted_role=promoted[0].decode(),old_primary_role=old_role[0].decode())
      save()
      assert follows(follower,target) and promoted[0]==old_role[0]==b'master',case['standby_at_replacement']
     while True:
      try:
       endpoint,_=exchange(sentinel,['SENTINEL','GET-MASTER-ADDR-BY-NAME','cache'])
       new,_=exchange(target,['ROLE'])
       old,_=exchange(current,['ROLE'])
      except (ConnectionError,TimeoutError,ssl.SSLEOFError,ssl.SSLZeroReturnError,ValueError) as error:
       # Role changes can kill authenticated read-only side connections too.
       if not tls or (isinstance(error,ValueError) and str(error)!='incomplete fixture RESP line'):
        raise
       case['interrupted_control_reads']+=1
      else:
       if int(endpoint[1])==target and new[0]==b'master' and old[0]==b'slave':
        if not a.replicas or old[3]==b'connected':
         if a.load:case['controller_timing_seconds']['send_to_verified_roles']=time.monotonic()-sent
         break
      assert time.monotonic()<due,'manual failover did not complete';time.sleep(.05)
    output,_=probe.communicate(None if a.load else 'GO\n',timeout=45 if a.load else 35)
    case.update(exit_code=probe.returncode,output=''.join(prefix)+output,probe_reaped=probe.poll() is not None)
    assert probe.returncode==0,case['output']
    if a.load:
     ledger=report.with_name(f'{report.stem}-{factory}-{protocol}-offers.csv.gz')
     case['load']=load_report(output,a.load,a.control,ledger)
     case['output']=''.join(prefix)+'\n'.join(line for line in output.splitlines() if not line.startswith('LOAD '))
    save();owned()
    print('factory',factory,'protocol',protocol,
          'fixed-arrival workload passed' if a.load else 'persistent pool refresh passed' if a.refresh else
          'persistent pool failover passed',flush=True)
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
