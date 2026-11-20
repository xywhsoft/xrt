"""Exercise asynchronous Cluster clients and actual slot owner changes against an owned migrating/importing Cluster slot."""
from functools import partial
from pathlib import Path
import argparse,hashlib,json,random,shutil,socket,subprocess,tempfile,time,traceback
from run_redis_cluster_readonly import client_path
from run_redis_role import send, frame
from run_redis_tls import TLS_CIPHER, certificates, listener_connection, listener_exchange, tls_listener

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,action='append',required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);p.add_argument('--port-candidate',type=int,action='append',default=[]);p.add_argument('--expect-command-routing',action='store_true');p.add_argument('--expect-sync-routing',action='store_true');p.add_argument('--expect-command-cache',action='store_true')
    p.add_argument('--tls', action='store_true', help='require server-authenticated TLS 1.3 on all nodes')
    p.add_argument('--openssl', type=Path, default=shutil.which('openssl'))
    a=p.parse_args()
    if a.tls and a.openssl is None:
        p.error('--tls needs OpenSSL to generate test certificates')
    clients=[x.absolute() for x in a.client];a.server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True)
    if a.report.exists():p.error('report already exists; choose a new evidence path')
    result=dict(complete=False,transport='tls' if a.tls else 'tcp',server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),server_sha256=sha(a.server),runner_sha256=sha(__file__),clients={str(x):sha(x) for x in clients},fixtures=[],cases=[],limits=['Owned real IPv4 Cluster with plaintext or server-authenticated TLS 1.3. TLS uses the same loopback certificate on every node.','Asynchronous explicit-seed client; SHARDS/SLOTS over prewarmed identity-verified owned pools. Actual owned slot reassignment after bootstrap produces real MOVED, automatic full-map refresh and a subsequent direct route; migration ASK remains ephemeral. Owned counter/fixture keys are deleted before restoring ownership. Every case subsequently denies the original seed at the cache authorization gate and verifies a new query-origin map from another discovered endpoint; original pools remain owned for administration. This is policy fault injection, not server-process death. Replica failover is covered by a separate fixture.'])
    result['synchronous_routing_required']=a.expect_sync_routing
    if a.expect_sync_routing:result['limits'].append('All four producers alternate explicit-slot and metadata-derived synchronous INCR calls, check actual wait status and retain the same exactly32 unique side effects, redirects and physical lifetimes. No real native-wait-failure injection is inferred.')
    result['cached_command_routing_required']=a.expect_command_cache
    if a.expect_command_cache:
        result['limits'].append('The APP fetches a full COMMAND snapshot through an explicit slot on the managed primary. All producers share the immutable catalogue.')
    result['metadata_command_routing_required']=a.expect_command_routing
    if a.expect_command_routing and not a.expect_command_cache:result['limits'].append('Each native case obtains a real authorized command catalogue, refuses cross-slot/keyless/incomplete inputs before admission, uses an explicit PING slot, and calculates all INCR/direct-GET slots before shared-client submission. Catalogue fetch/cache/fallback policy is application-owned; metadata preflight is not a business side effect.')
    def save():a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    children=[];logs=[];temporary=tempfile.TemporaryDirectory(prefix='cluster route ');runtime=Path(temporary.name)
    def identity(port):
        value,_=exchange(port,['INFO','SERVER']);return dict(x.split(b':',1) for x in value.split(b'\r\n') if b':' in x)
    def owned():
        for port,child in children:
            old=next(x for x in result['fixtures'] if x['port']==port);now=identity(port)
            assert child.poll() is None and int(now[b'process_id'])==child.pid and now[b'run_id'].decode()==old['run_id']
    try:
        tls = ca = None
        if a.tls:
            openssl = a.openssl.absolute()
            hashes = certificates(openssl, runtime, cluster_peer=True)
            ca = runtime / 'ca.pem'
            tls = tls_listener(runtime, 'server', 'ca')
            result['tls'] = dict(protocol='TLSv1.3', cipher=TLS_CIPHER, hostname='localhost',
                                 ca_sha256=hashes['ca.pem'], leaf_sha256=hashes['server.pem'],
                                 openssl_sha256=sha(openssl))
        exchange = partial(listener_exchange, tls=tls)
        connect = partial(listener_connection, tls=tls)

        def executed(port, command):
            info, _ = exchange(port, ['INFO', 'COMMANDSTATS'])
            key = b'cmdstat_' + command.encode() + b':'
            for line in info.splitlines():
                if line.startswith(key):
                    return int(line[len(key):].split(b',', 1)[0].removeprefix(b'calls='))
            return 0

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
            args=[str(a.server),'--bind','127.0.0.1','--save','','--appendonly','no','--dir',str(directory),'--cluster-enabled','yes','--cluster-config-file','nodes.conf','--cluster-node-timeout','1000']
            if tls:
                args += [*tls[2], '--tls-port', str(port), '--tls-cluster', 'yes',
                         '--tls-replication', 'yes', '--cluster-announce-ip', '127.0.0.1']
            else:
                args += ['--port', str(port)]
            child=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT);children.append((port,child));due=time.monotonic()+10
            while True:
                assert child.poll() is None,(port,'fixture exited')
                try:info=identity(port);assert int(info[b'process_id'])==child.pid;break
                except OSError:assert time.monotonic()<due;time.sleep(.02)
            nodeid, _ = exchange(port, ['CLUSTER', 'MYID'])
            node = dict(port=port, pid=child.pid, run_id=info[b'run_id'].decode(), node_id=nodeid.decode())
            result['fixtures'].append(node)
            if tls:
                config, _ = exchange(port, ['CONFIG', 'GET', 'port', 'tls-port', 'tls-cluster',
                                            'tls-replication', 'tls-protocols'], protocol=3)
                assert config == {b'port': b'0', b'tls-port': str(port).encode(), b'tls-cluster': b'yes',
                                  b'tls-replication': b'yes', b'tls-protocols': b'TLSv1.3'}, config
                node['tls_settings'] = {key.decode(): value.decode() for key, value in config.items()}
            save()
        owned()
        for port in ports[1:]:exchange(ports[0],['CLUSTER','MEET','127.0.0.1',port])
        ranges=list(zip(ports,(0,5461,10923),(5460,10922,16383)))
        for port,first,last in ranges:exchange(port,['CLUSTER','ADDSLOTSRANGE',first,last])
        due=time.monotonic()+20
        while not all(b'cluster_state:ok' in exchange(port,['CLUSTER','INFO'])[0] for port in ports):
            assert time.monotonic()<due,'Cluster coverage not ready';time.sleep(.05)
        key=b'xrt-owned:asking:{fixed}\0key';slot,_=exchange(ports[0],['CLUSTER','KEYSLOT',key]);source=next(port for port,first,last in ranges if first<=slot<=last);target=next(port for port in ports if port!=source);ids={x['port']:x['node_id'] for x in result['fixtures']};owned();assert exchange(source,['EXISTS',key])[0]==0
        exchange(target,['CLUSTER','SETSLOT',slot,'IMPORTING',ids[source]]);exchange(source,['CLUSTER','SETSLOT',slot,'MIGRATING',ids[target]])
        with connect(source) as s:
            send(s,['GET',key])
            with s.makefile('rb') as f:ask=f.readline(4096);assert ask.startswith(b'-ASK '+str(slot).encode()+b' ') and ask.endswith(b':'+str(target).encode()+b'\r\n')
        with connect(target) as s:
            with s.makefile('rb') as f:
                send(s,['ASKING']);assert frame(f)[0]==b'OK';send(s,['SET',key,b'value\0x']);assert frame(f)[0]==b'OK'
        result.update(slot=slot,source_port=source,target_port=target,actual_ask=ask.decode().strip());save()
        for client in clients:
            owned()
            other = next(port for port in ports if port not in (source, target))
            runs = {node['port']: node['run_id'] for node in result['fixtures']}
            args = [str(client), str(source), str(target), str(other), runs[source], runs[target], runs[other]]
            if ca:
                args.append(client_path(client, ca))
            before = {port: executed(port, 'incr') for port in ports}
            r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=90)
            row = dict(client=str(client), client_sha256=sha(client), command=args, exit_code=r.returncode, output=r.stdout,
                       expected_native_matrix_cases=36,
                       native_matrix_cases=sum(line.startswith('[case] ') for line in r.stdout.splitlines()),
                       metadata_command_routing='[PASS] metadata command routing:' in r.stdout,
                       synchronous_routing='[PASS] synchronous Cluster routing:' in r.stdout,
                       cached_routing='[PASS] cached command routing:' in r.stdout)
            result['cases'].append(row)
            save()
            assert r.returncode == 0 and row['native_matrix_cases'] == 36 and '[PASS] real Cluster client: 36 ' in r.stdout, r.stdout
            owned()
            row['executed_incr'] = {port: executed(port, 'incr') - before[port] for port in ports}
            assert row['executed_incr'] == {source: 0, target: 36 * 32, other: 0}, row['executed_incr']
            assert not a.expect_command_routing or row['metadata_command_routing'], r.stdout
            assert not a.expect_sync_routing or row['synchronous_routing'], r.stdout
            assert not a.expect_command_cache or row['cached_routing'], r.stdout
            print(client.name,'native 36-case managed bootstrap/real MOVED/ASK matrix passed',flush=True)
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
