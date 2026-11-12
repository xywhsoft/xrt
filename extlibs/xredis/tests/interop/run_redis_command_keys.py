"""Independent key oracle: actual owned Redis COMMAND and GETKEYSANDFLAGS, no business execution."""
from pathlib import Path
import argparse,binascii,hashlib,json,random,shutil,socket,struct,subprocess,tempfile,time,traceback
from run_redis_role import exchange,send,frame

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def slot(key):
    start=key.find(b'{')
    if start>=0:
        end=key.find(b'}',start+1)
        if end>start+1:key=key[start+1:end]
    return binascii.crc_hqx(key,0)%16384
def corpus(metadata,cases):
    output=struct.pack('<I',len(metadata))+metadata+struct.pack('<I',len(cases))
    for case in cases:
        args=case['arguments'];output+=struct.pack('<I',len(args))
        for arg in args:output+=struct.pack('<I',len(arg))+arg
    return output
def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,action='append',required=True);p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    probes=[p.absolute() for p in a.probe];server=a.server.absolute();a.report=a.report.absolute();a.report.parent.mkdir(parents=True,exist_ok=True)
    if a.report.exists():p.error('report already exists; choose a new evidence path')
    result=dict(complete=False,server_version=subprocess.check_output([str(server),'--version'],text=True).strip(),server_sha256=sha(server),runner_sha256=sha(__file__),probes={str(p):sha(p) for p in probes},cases=[],limits=['Parser/slot extraction oracle only. Business invocations are arguments to COMMAND GETKEYSANDFLAGS, never executed. No automatic metadata fetch/fallback/client submission, replica policy, TLS or end-to-end performance acceptance.'])
    def save():a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    temporary=tempfile.TemporaryDirectory(prefix='command key oracle ');runtime=Path(temporary.name);log=(runtime/'server.log').open('wb');child=None
    try:
        port=None
        for candidate in random.Random(6716).sample(range(12000,45000),128):
            sockets=[]
            try:
                for value in (candidate,candidate+10000):
                    s=socket.socket();sockets.append(s);s.bind(('127.0.0.1',value))
                port=candidate;break
            except OSError:pass
            finally:
                for s in sockets:s.close()
        assert port is not None,'no owned Cluster fixture port'
        child=subprocess.Popen([str(server),'--bind','127.0.0.1','--port',str(port),'--save','','--appendonly','no','--dir',str(runtime),'--cluster-enabled','yes','--cluster-config-file','nodes.conf'],stdout=log,stderr=subprocess.STDOUT)
        def identity():
            assert child.poll() is None
            raw,_=exchange(port,['INFO','SERVER']);fields=dict(x.split(b':',1) for x in raw.split(b'\r\n') if b':' in x);assert int(fields[b'process_id'])==child.pid;return fields[b'run_id'].decode()
        due=time.monotonic()+10
        while True:
            assert child.poll() is None
            try:run=identity();break
            except OSError:assert time.monotonic()<due;time.sleep(.02)
        result['fixture']=dict(port=port,pid=child.pid,run_id=run);save()
        cases=[]
        def add(args,status=None):
            values=[x if isinstance(x,bytes) else str(x).encode() for x in args];row=dict(arguments=values)
            if status is not None:row['status']=status
            elif values[0].upper()==b'SPUBLISH':
                try:
                    returned,_=exchange(port,[b'COMMAND',b'GETKEYSANDFLAGS',*values]);assert not returned
                except ValueError as error:assert 'The command has no key arguments' in str(error)
                target=exchange(port,['CLUSTER','KEYSLOT',values[1]])[0];assert target==slot(values[1])
                row.update(keys=[values[1]],flags=None,slot=target,status=1,not_key=True)
            else:
                try:keys,_=exchange(port,[b'COMMAND',b'GETKEYSANDFLAGS',*values])
                except ValueError as error:raise AssertionError(('GETKEYSANDFLAGS',values)) from error
                expected=[x[0] for x in keys];slots=[exchange(port,['CLUSTER','KEYSLOT',k])[0] for k in expected];assert slots==[slot(k) for k in expected]
                row.update(keys=expected,flags=[x[1] for x in keys],slot=slots[0] if slots else 0,status=5 if len(set(slots))>1 else 1 if expected else 2)
            cases.append(row)
        for args in ([b'GET',b'\0{123456789}\xff'],['MSET','a{t}','x','b{t}','y'],['MSET','a{a}','x','b{b}','y'],['RENAME','a{t}','b{t}'],['SMOVE','a{t}','b{t}','member'],['BITOP','AND','d{t}','a{t}','b{t}'],['BLPOP','a{t}','b{t}','0'],['LMPOP',2,'a{t}','b{t}','LEFT'],['BZMPOP','0.1',2,'a{t}','b{t}','MIN'],['ZUNIONSTORE','d{t}',2,'a{t}','b{t}','WEIGHTS',1,2],['ZINTERCARD',2,'a{t}','b{t}','LIMIT',1],['XREAD','COUNT',1,'STREAMS','a{t}','b{t}','0','0'],['XREADGROUP','GROUP','g','c','STREAMS','a{t}','b{t}','>','>'],['XGROUP','CREATE','a{t}','g','0'],['SET','a{t}','value'],['SET','a{t}','value','GET'],['SPUBLISH','channel{t}','data']):add(args)
        rng=random.Random(6716)
        for _ in range(64):
            n=rng.randrange(1,65);keys=[b'key'+str(i).encode()+b'\0{tag}' for i in range(n)];add(['EVAL','return 1',n,*keys,'ARGV']);add(['MGET',*keys]);add(['XREAD','STREAMS',*keys,*([b'0-0']*n)])
        for args,status in [(['PING'],2),(['CONFIG','GET','timeout'],2),(['EVAL','return 1',0,'ARGV'],2),(['SORT','a{t}','BY','b*','STORE','out{t}'],4),(['MIGRATE','127.0.0.1','1','','0','1','KEYS','a{t}','b{t}'],4),(['__xrt_unknown_command__','key'],3),(['GET'],6)]:add(args,status)
        folder=a.report.parent/(a.report.stem+'-corpus');folder.mkdir(exist_ok=False);result['corpus_directory']=folder.name
        for protocol in (2,3):
            assert identity()==run
            for mode,query in [('full',['COMMAND']),('partial',['COMMAND','INFO','GET','MSET','XGROUP','SORT','__xrt_unknown_command__'])]:
                _,metadata=exchange(port,query,protocol);chosen=cases if mode=='full' else [x for x in cases if x['arguments'][0].upper() in (b'GET',b'MSET',b'XGROUP',b'SORT',b'__XRT_UNKNOWN_COMMAND__')]
                data=corpus(metadata,chosen);path=folder/(mode+'-'+str(protocol)+'.bin');path.write_bytes(data);row=dict(mode=mode,protocol=protocol,corpus=path.name,corpus_sha256=sha(path),invocations=len(chosen),probes=[]);result['cases'].append(row)
                for probe in probes:
                    process=subprocess.run([str(probe)],input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30);assert process.returncode==0,process.stderr.decode(errors='replace');got=json.loads(process.stdout);assert got['status']==1,(probe,mode,protocol,got)
                    assert len(got['cases'])==len(chosen)
                    for want,actual in zip(chosen,got['cases']):
                        assert actual['status']==want['status'],(want['arguments'],actual,want['status'])
                        if 'keys' in want and want['status']==1:
                            assert [bytes.fromhex(x['key']) for x in actual['keys']]==want['keys'],(want['arguments'],actual,want['keys']);assert actual['slot']==want['slot']
                            if want.get('not_key'):assert all(x['flags']&256 for x in actual['keys'])
                            if want['flags'] is not None and not actual['variable'] and not actual['legacy']:
                                names={b'RO':1,b'RW':2,b'OW':4,b'RM':8,b'access':16,b'update':32,b'insert':64,b'delete':128,b'not_key':256,b'variable_flags':512}
                                flags=[sum(names[f] for f in group) for group in want['flags']];assert [x['flags'] for x in actual['keys']]==flags,(want['arguments'],actual,flags)
                    row['probes'].append(dict(probe=str(probe),sha256=sha(probe),commands=got['commands'],specifications=got['specifications'],invocations=len(chosen)));save()
        assert identity()==run;result['complete']=True;result['oracle_invocations']=len(cases);result['getkeysandflags_invocations']=sum(x.get('flags') is not None for x in cases);result['not_key_standard_and_server_slot_cases']=sum(x.get('not_key',False) for x in cases);result['business_executions']=0;print('real COMMAND RESP2/3, subcommand/null INFO and',len(cases),'invocation checks passed (not_key channel uses standard position plus server slot, not GETKEYS)',flush=True)
    except BaseException:result['failure']=traceback.format_exc();raise
    finally:
        if child is not None and child.poll() is None:child.terminate()
        if child is not None:
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        log.close();shutil.copy2(runtime/'server.log',a.report.with_name(a.report.stem+'-server.log'));temporary.cleanup();result['processes_terminal']=child is None or child.poll() is not None;result['runtime_material_removed']=not runtime.exists();save()
if __name__=='__main__':main()
