"""Capture ROLE from disposable owned master/replicas/Sentinel; run C view probes."""
from pathlib import Path
import argparse,hashlib,json,shutil,socket,subprocess,tempfile,time,traceback

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def frame(f):
 line=f.readline(1048577)
 if not line.endswith(b'\r\n'):raise ValueError('incomplete fixture RESP line')
 tag=line[:1];v=line[1:-2]
 if tag in (b'+',b'-',b':'):
  if tag==b'-':raise ValueError('fixture server error '+repr(v))
  return (int(v) if tag==b':' else v),line
 if tag in (b'$',b'!'):
  n=int(v)
  if n<0:return None,line
  assert n<=1048576;data=f.read(n+2);assert len(data)==n+2 and data[-2:]==b'\r\n'
  return data[:-2],line+data
 if tag in (b'*',b'%',b'~',b'>'):
  count=int(v);assert 0<=count<=100000
  values=[];raw=line
  for _ in range(count*(2 if tag==b'%' else 1)):
   item,wire=frame(f);values.append(item);raw+=wire
  return (dict(zip(values[::2],values[1::2])) if tag==b'%' else values),raw
 if tag==b'_':return None,line
 raise ValueError('unsupported fixture RESP type '+repr(tag))
def send(s,args):
 values=[x if isinstance(x,bytes) else str(x).encode() for x in args]
 s.sendall(b'*'+str(len(values)).encode()+b'\r\n'+b''.join(b'$'+str(len(x)).encode()+b'\r\n'+x+b'\r\n' for x in values))
def exchange(port,args,protocol=2):
 with socket.create_connection(('127.0.0.1',port),timeout=2) as s:
  s.settimeout(2)
  with s.makefile('rb') as f:
   if protocol==3:
    send(s,['HELLO',3]);hello,_=frame(f);assert hello[b'proto']==3
   send(s,args);return frame(f)
def main():
 a=argparse.ArgumentParser();a.add_argument('--client',type=Path,action='append',required=True);a.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));a.add_argument('--report',type=Path,required=True);a.add_argument('--runtime-root',type=Path,help='Native server filesystem for temporary replication state; default is the system temporary directory');a=a.parse_args()
 a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True);clients=[c.absolute() for c in a.client]
 result=dict(schema=1,complete=False,server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),clients={str(c):sha(c) for c in clients},fixtures=[],cases=[],limits=['Reply projection only; no C transport, Sentinel discovery/failover or network performance acceptance.','Disposable loopback plaintext RESP2/3 fixtures. TLS/auth transport is not exercised by this parser-only corpus.'])
 def save():a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 children=[];logs=[];reserved=None;temporary=None;runtime=None
 try:
  temporary=tempfile.TemporaryDirectory(prefix="role-runtime-",dir=a.runtime_root)
  runtime=Path(temporary.name);result['runtime_directory_root']=str(runtime.parent);ports=[]
  while len(ports)<5:
   with socket.socket() as s:
    s.bind(('127.0.0.1',0));port=s.getsockname()[1]
   if port not in ports:ports.append(port)
  master,replica,offline,sentinel,unused=ports;reserved=socket.socket();reserved.bind(('127.0.0.1',unused))
  for name,port,extra in [('master',master,[]),('replica',replica,['--replicaof','127.0.0.1',str(master)]),('offline',offline,['--replicaof','127.0.0.1',str(unused)]),('sentinel',sentinel,[])]:
   directory=runtime/name;directory.mkdir();log=(directory/'server.log').open('wb');logs.append((name,log));cmd=[str(a.server),'--bind','127.0.0.1','--port',str(port),'--save','','--appendonly','no','--dir',str(directory),'--repl-diskless-sync-delay','0']+extra
   if name=='sentinel':
    config=directory/'sentinel.conf';config.write_text(f'bind 127.0.0.1\nport {port}\ndir {json.dumps(str(directory),ensure_ascii=False)}\nsentinel monitor cache 127.0.0.1 {master} 1\nsentinel down-after-milliseconds cache 5000\n',encoding='utf-8');cmd=[str(a.server),str(config),'--sentinel']
   process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT);children.append((name,port,process));deadline=time.monotonic()+10
   while True:
    assert process.poll() is None,(name,'fixture exited',process.returncode)
    try:
     info,_=exchange(port,['INFO','server']);fields=dict(line.split(b':',1) for line in info.split(b'\r\n') if b':' in line);assert int(fields[b'process_id'])==process.pid;identity=fields[b'run_id'].decode();break
    except (OSError,EOFError):
     if time.monotonic()>=deadline:raise
     time.sleep(.02)
   result['fixtures'].append(dict(name=name,pid=process.pid,port=port,run_id=identity))
  deadline=time.monotonic()+10
  while True:
   value,_=exchange(replica,['ROLE'])
   if value[3]==b'connected' and len(exchange(master,['ROLE'])[0][2])==1:break
   assert time.monotonic()<deadline,'replication did not become online';time.sleep(.02)
  corpus=a.report.parent/'corpus';corpus.mkdir(exist_ok=True)
  for name,port,process in children:
   identity=next(x for x in result['fixtures'] if x['name']==name)
   info,_=exchange(port,['INFO','server']);fields=dict(line.split(b':',1) for line in info.split(b'\r\n') if b':' in line);assert int(fields[b'process_id'])==process.pid and fields[b'run_id'].decode()==identity['run_id'] and process.poll() is None
   for protocol in (2,3):
    value,wire=exchange(port,['ROLE'],protocol);kind=1 if name=='master' else 3 if name=='sentinel' else 2;count=len(value[2]) if kind==1 else len(value[1]) if kind==3 else 0;target=replica if kind==1 else master if name=='replica' else unused if name=='offline' else 0;link={b'connect':1,b'connecting':2,b'sync':3,b'connected':4}.get(value[3],0) if kind==2 else 0
    assert value[0]=={1:b'master',2:b'slave',3:b'sentinel'}[kind]
    if name=='offline':assert value[3]!=b'connected' and value[4]>=-1
    path=corpus/(name+'-'+str(protocol)+'.resp');path.write_bytes(wire);case=dict(fixture=name,protocol=protocol,role=kind,entries=count,port=target,link=link,offset=value[4] if kind==2 else value[1] if kind==1 else None,wire=path.name,wire_sha256=sha(path),probes=[])
    result['cases'].append(case)
    for client in clients:
     filename=str(path)
     if client.suffix.lower()=='.exe':filename=subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
     cmd=[str(client),filename,str(kind),str(count),str(target),str(link)];completed=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30);case['probes'].append(dict(client=str(client),exit_code=completed.returncode,output=completed.stdout));save();assert completed.returncode==0,completed.stdout
    print(name,protocol,'ROLE passed',flush=True)
  result['complete']=True
 except BaseException:
  result['failure']=traceback.format_exc();raise
 finally:
  for name,port,process in children:
   if process.poll() is None:process.terminate()
  for name,port,process in children:
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
  for name,log in logs:log.close()
  if reserved:reserved.close()
  if temporary:
   saved=a.report.parent/'fixture-logs';saved.mkdir(exist_ok=True)
   for name,log in logs:
    source=runtime/name/'server.log'
    if source.exists():shutil.copy2(source,saved/(name+'.log'))
   temporary.cleanup()
  result['children_reaped']=all(p.poll() is not None for _,_,p in children);result['runtime_material_removed']=runtime is None or not runtime.exists();save()
if __name__=='__main__':main()
