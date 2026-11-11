"""Owned real Cluster wire/projection corpus; C probe performs no transport/retry."""
from pathlib import Path
import argparse,hashlib,json,random,shutil,socket,struct,subprocess,sys,tempfile,time,traceback
from run_redis_role import exchange,send,frame
sys.path.insert(0,str(Path(__file__).parents[1]))
from test_cluster_topology_reference import expected,invoke,keyslot

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def fields(v):return {k.decode():x for k,x in (v.items() if isinstance(v,dict) else zip(v[::2],v[1::2]))}
def normalize(v,legacy):
    if legacy:
        for row in v:
            for n in row[2:]:
                if len(n)>3:n[3]=fields(n[3])
    else:
        v=[fields(row) for row in v]
        for row in v:
            row['nodes']=[fields(n) for n in row['nodes']]
            for n in row['nodes']:
                for key in ('role','health'):n[key]=n[key].decode()
    return v

def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,action='append',required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    probes=[x.absolute() for x in a.probe];a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True)
    result=dict(complete=False,server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),probes={str(x):sha(x) for x in probes},fixtures=[],cases=[],limits=['Real plaintext loopback Cluster replies, keyslot oracle and redirect views; no C transport/automatic routing/resharding-performance acceptance.'])
    def save():a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    children=[];logs=[];temporary=tempfile.TemporaryDirectory(prefix='cluster topology ');runtime=Path(temporary.name)
    def identity(port):
        info,_=exchange(port,['INFO','SERVER']);return dict(x.split(b':',1) for x in info.split(b'\r\n') if b':' in x)
    def owned():
        for port,child in children:
            info=identity(port);prior=next(x for x in result['fixtures'] if x['port']==port)
            assert child.poll() is None and int(info[b'process_id'])==child.pid and info[b'run_id'].decode()==prior['run_id']
    def error(port,args):
        with socket.create_connection(('127.0.0.1',port),timeout=2) as s:
            s.settimeout(2);send(s,args)
            with s.makefile('rb') as f:wire=f.readline(4096);assert wire.startswith(b'-') and wire.endswith(b'\r\n');return wire
    try:
        ports=[];rng=random.Random(6712)
        while len(ports)<3:
            port=rng.randrange(12000,45000)
            if port in ports:continue
            sockets=[]
            try:
                for n in (port,port+10000):s=socket.socket();s.bind(('127.0.0.1',n));sockets.append(s)
                ports.append(port)
            except OSError:pass
            finally:
                for s in sockets:s.close()
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
        for port,first,last in zip(ports,(0,5461,10923),(5460,10922,16383)):exchange(port,['CLUSTER','ADDSLOTSRANGE',first,last])
        due=time.monotonic()+20
        while True:
            if all(b'cluster_state:ok' in exchange(port,['CLUSTER','INFO'])[0] for port in ports):break
            assert time.monotonic()<due,'Cluster did not cover slots';time.sleep(.05)
        latest=None;corpus=a.report.parent/'cluster-corpus';corpus.mkdir(exist_ok=True)
        for preference in ('ip','unknown-endpoint','hostname'):
            owned()
            for port in ports:
                exchange(port,['CONFIG','SET','cluster-announce-hostname','localhost']);exchange(port,['CONFIG','SET','cluster-preferred-endpoint-type',preference])
            for port in ports:
                for protocol in (2,3):
                    for legacy in (False,True):
                        value,wire=exchange(port,['CLUSTER','SLOTS' if legacy else 'SHARDS'],protocol);value=normalize(value,legacy);want=expected(value,legacy);assert want['assigned']==16384
                        if preference=='unknown-endpoint':assert any(n['endpoint']==2 for n in want['nodes'])
                        name=f'{preference}-{port}-{protocol}-{int(legacy)}.resp';(corpus/name).write_bytes(wire)
                        row=dict(preference=preference,port=port,protocol=protocol,legacy=legacy,wire=name,wire_sha256=hashlib.sha256(wire).hexdigest(),probes=[]);result['cases'].append(row)
                        for probe in probes:
                            got=invoke(probe,2 if legacy else 1,wire);assert got==want,(probe,name,got,want);row['probes'].append(str(probe));save()
                        if preference=='ip' and not legacy:latest=want
            print(preference,'SHARDS/SLOTS RESP2/3 passed',flush=True)
        assert latest is not None
        keys=[b'',b'foo',b'foo{}{bar}',b'foo{{bar}}zap',b'binary\0{tag}']+[rng.randbytes(rng.randrange(257)) for _ in range(128)]
        slotwire=b''.join(struct.pack('<I',len(k))+k for k in keys);oracle=[exchange(ports[0],['CLUSTER','KEYSLOT',k])[0] for k in keys];assert oracle==[keyslot(k) for k in keys]
        for probe in probes:assert invoke(probe,0,slotwire)==oracle
        result['redis_keyslot_oracles']=len(keys)
        key=b'xrt-owned-cluster:{routing-test}:absent';slot=keyslot(key);route=next(r for r in latest['ranges'] if r[0]<=slot<=r[1]);source=latest['nodes'][route[2]]['port'];target=next(p for p in ports if p!=source)
        assert source in ports;owned()
        moved=error(target,['GET',key]);assert moved.startswith(b'-MOVED ')
        for probe in probes:
            view=invoke(probe,3,moved);assert view['status']==1 and view['kind']==1 and view['slot']==slot and view['port']==source
        ids={x['port']:x['node_id'] for x in result['fixtures']}
        try:
            exchange(target,['CLUSTER','SETSLOT',slot,'IMPORTING',ids[source]]);exchange(source,['CLUSTER','SETSLOT',slot,'MIGRATING',ids[target]])
            ask=error(source,['GET',key]);assert ask.startswith(b'-ASK ')
            for probe in probes:
                view=invoke(probe,3,ask);assert view['status']==1 and view['kind']==2 and view['slot']==slot and view['port']==target
            with socket.create_connection(('127.0.0.1',target),timeout=2) as s:
                with s.makefile('rb') as f:
                    send(s,['ASKING']);assert frame(f)[0]==b'OK';send(s,['GET',key]);assert frame(f)[0] is None
        finally:
            owned();exchange(source,['CLUSTER','SETSLOT',slot,'STABLE']);exchange(target,['CLUSTER','SETSLOT',slot,'STABLE'])
        (corpus/'moved.resp').write_bytes(moved);(corpus/'ask.resp').write_bytes(ask);result['real_redirects']=['MOVED','ASK'];result['complete']=True
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
