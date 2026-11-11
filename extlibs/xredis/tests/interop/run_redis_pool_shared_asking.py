"""Exercise native shared ASKING against an owned migrating/importing Cluster slot."""
from pathlib import Path
import argparse,hashlib,json,random,shutil,socket,subprocess,tempfile,time,traceback
from run_redis_role import exchange,send,frame

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,action='append',required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);p.add_argument('--port-candidate',type=int,action='append',default=[]);a=p.parse_args()
    clients=[x.absolute() for x in a.client];a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True)
    result=dict(complete=False,server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),clients={str(x):sha(x) for x in clients},fixtures=[],cases=[],limits=['Owned real plaintext IPv4 Cluster; no real TLS or full authentication/trust matrix in this stage.','Native shared ASKING primitive only; target is explicitly provided after real ASK, not selected/refreshed/retried by a C Cluster router.','Denied-control user has explicit nopass fixture credentials; Query ECHO still executes and is reported separately.'])
    def save():a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    children=[];logs=[];temporary=tempfile.TemporaryDirectory(prefix='pool asking ');runtime=Path(temporary.name)
    def identity(port):
        value,_=exchange(port,['INFO','SERVER']);return dict(x.split(b':',1) for x in value.split(b'\r\n') if b':' in x)
    def owned():
        for port,child in children:
            old=next(x for x in result['fixtures'] if x['port']==port);now=identity(port)
            assert child.poll() is None and int(now[b'process_id'])==child.pid and now[b'run_id'].decode()==old['run_id']
    try:
        ports=[];rng=random.Random(6713)
        for port in (a.port_candidate or rng.sample(range(12000,45000),128)):
            if len(ports)==3:break
            assert 1<=port<=55535
            if any(abs(port-prior) in (0,10000) for prior in ports):continue
            reservations=[]
            try:
                for n in (port,port+10000):s=socket.socket();s.bind(('127.0.0.1',n));reservations.append(s)
                ports.append(port)
            except OSError:pass
            finally:
                for s in reservations:s.close()
        assert len(ports)==3,'no owned Cluster fixture ports available'
        for port in ports:
            directory=runtime/str(port);directory.mkdir();log=(directory/'server.log').open('wb');logs.append((port,log))
            args=[str(a.server),'--bind','127.0.0.1','--port',str(port),'--save','','--appendonly','no','--dir',str(directory),'--cluster-enabled','yes','--cluster-config-file','nodes.conf','--cluster-node-timeout','1000']
            child=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT);children.append((port,child));due=time.monotonic()+10
            while True:
                assert child.poll() is None,(port,'fixture exited')
                try:info=identity(port);assert int(info[b'process_id'])==child.pid;break
                except OSError:assert time.monotonic()<due;time.sleep(.02)
            nodeid,_=exchange(port,['CLUSTER','MYID']);result['fixtures'].append(dict(port=port,pid=child.pid,run_id=info[b'run_id'].decode(),node_id=nodeid.decode()));save()
        owned()
        for port in ports[1:]:exchange(ports[0],['CLUSTER','MEET','127.0.0.1',port])
        ranges=list(zip(ports,(0,5461,10923),(5460,10922,16383)))
        for port,first,last in ranges:exchange(port,['CLUSTER','ADDSLOTSRANGE',first,last])
        due=time.monotonic()+20
        while not all(b'cluster_state:ok' in exchange(port,['CLUSTER','INFO'])[0] for port in ports):
            assert time.monotonic()<due,'Cluster coverage not ready';time.sleep(.05)
        key=b'xrt-owned:asking:{fixed}\0key';slot,_=exchange(ports[0],['CLUSTER','KEYSLOT',key]);source=next(port for port,first,last in ranges if first<=slot<=last);target=next(port for port in ports if port!=source);ids={x['port']:x['node_id'] for x in result['fixtures']};owned();assert exchange(source,['EXISTS',key])[0]==0
        exchange(target,['ACL','SETUSER','asking-denied','reset','on','nopass','allkeys','+hello','+auth','+ping','+info','+client|id','+get','+echo','-asking'])
        exchange(target,['CLUSTER','SETSLOT',slot,'IMPORTING',ids[source]]);exchange(source,['CLUSTER','SETSLOT',slot,'MIGRATING',ids[target]])
        with socket.create_connection(('127.0.0.1',source),timeout=2) as s:
            send(s,['GET',key])
            with s.makefile('rb') as f:ask=f.readline(4096);assert ask.startswith(b'-ASK '+str(slot).encode()+b' ') and ask.endswith(b':'+str(target).encode()+b'\r\n')
        with socket.create_connection(('127.0.0.1',target),timeout=2) as s:
            with s.makefile('rb') as f:
                send(s,['ASKING']);assert frame(f)[0]==b'OK';send(s,['SET',key,b'value\0x']);assert frame(f)[0]==b'OK'
        result.update(slot=slot,source_port=source,target_port=target,actual_ask=ask.decode().strip());run=next(x['run_id'] for x in result['fixtures'] if x['port']==target);save()
        for client in clients:
            owned();args=[str(client),str(target),str(source),run];r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
            row=dict(client=str(client),client_sha256=sha(client),command=args,exit_code=r.returncode,output=r.stdout,expected_native_matrix_cases=36,native_matrix_cases=36 if r.returncode==0 else None);result['cases'].append(row);save();assert r.returncode==0 and '[PASS] real shared ASKING: 36 ' in r.stdout,r.stdout
            print(client.name,'native 36-case shared ASKING matrix passed',flush=True)
        owned();exchange(source,['CLUSTER','SETSLOT',slot,'STABLE']);exchange(target,['CLUSTER','SETSLOT',slot,'STABLE']);result['complete']=True
    except BaseException:result['failure']=traceback.format_exc();raise
    finally:
        for port,child in children:
            if child.poll() is None:child.terminate()
        for port,child in children:
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        for port,log in logs:
            log.close();shutil.copy2(runtime/str(port)/'server.log',a.report.with_name(a.report.stem+'-'+str(port)+'.log'))
        temporary.cleanup();result['runtime_material_removed']=not runtime.exists();result['processes_terminal']=all(child.poll() is not None for _,child in children);save()
if __name__=='__main__':main()
