"""Independent binary CRC oracle and semantic topology corpus for a C stdin probe."""
from pathlib import Path
import argparse,binascii,copy,hashlib,json,random,struct,subprocess

def enc(v,resp3=True):
    if isinstance(v,dict):
        return (b'%' + str(len(v)).encode() if resp3 else b'*' + str(2*len(v)).encode())+b'\r\n'+b''.join(enc(k,resp3)+enc(x,resp3) for k,x in v.items())
    if isinstance(v,list):return b'*'+str(len(v)).encode()+b'\r\n'+b''.join(enc(x,resp3) for x in v)
    if isinstance(v,int):return b':'+str(v).encode()+b'\r\n'
    if v is None:return b'_\r\n' if resp3 else b'$-1\r\n'
    v=v.encode() if isinstance(v,str) else v
    return b'$'+str(len(v)).encode()+b'\r\n'+v+b'\r\n'
def keyslot(key):
    start=key.find(b'{')
    if start>=0:
        end=key.find(b'}',start+1)
        if end>start+1:key=key[start+1:end]
    return binascii.crc_hqx(key,0)%16384

def invoke(probe,mode,wire):
    r=subprocess.run([str(probe),str(mode)],input=wire,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
    assert r.returncode==0,(r.returncode,r.stdout.decode(errors='replace'),r.stderr.decode(errors='replace'))
    return json.loads(r.stdout)

def hx(v):return (v.encode() if isinstance(v,str) else v or b'').hex()
def expected(value,legacy=False):
    nodes=[];ranges=[];assigned={}
    for shard,row in enumerate(value):
        source=row[2:] if legacy else row['nodes'];master=None
        for index,n in enumerate(source):
            role=('master' if index==0 else 'replica') if legacy else n['role']
            if role=='master':
                assert master is None
                master=len(nodes)
            host=n[0] if legacy else n['endpoint'];ip=n[3].get('ip',b'') if legacy and len(n)>3 else b'' if legacy else n['ip']
            hostname=n[3].get('hostname',b'') if legacy and len(n)>3 else b'' if legacy else n.get('hostname',b'')
            health='unreported' if legacy else n['health']
            nodes.append(dict(port=n[1] if legacy else n.get('port',0),tls=0 if legacy else n.get('tls-port',0),endpoint=2 if not host else 3 if host in ('?',b'?') else 1,role={'master':1,'replica':2}.get(role,0),health={'online':1,'failed':2,'loading':3,'unreported':4}.get(health,0),offset=-1 if legacy else n['replication-offset'],host=hx(host),id=hx(n[2] if len(n)>2 else b'') if legacy else hx(n['id']),ip=hx(ip),hostname=hx(hostname),role_text='' if legacy else hx(role),health_text='' if legacy else hx(health),shard=shard))
        assert master is not None
        pairs=[row[:2]] if legacy else list(zip(row['slots'][::2],row['slots'][1::2]))
        for first,last in pairs:
            ranges.append([first,last,master,shard]);weight=nodes[master]['port']+(nodes[master]['tls']<<16)+1
            for slot in range(first,last+1):assert slot not in assigned;assigned[slot]=(slot+1)*weight
    return dict(status=1,shards=len(value),assigned=len(assigned),nodes=nodes,ranges=ranges,checksum=sum(assigned.values()))

def corpus(probe,report):
    rng=random.Random(0x434c5553544552);rows=[]
    def check(name,value,legacy=False,status=None,resp3=True,prefix=b''):
        wire=prefix+enc(value,resp3);want=expected(value,legacy) if status is None else dict(status=status)
        got=invoke(probe,2 if legacy else 1,wire);assert got==want,(name,got,want)
        rows.append(dict(name=name,format=2 if legacy else 1,resp3=resp3,wire_sha256=hashlib.sha256(wire).hexdigest(),status=got['status']))
    def node(role='master',host='node.example',health='online'):
        return dict(id='id-'+str(rng.randrange(1000000)),endpoint=host,ip='127.0.0.1',port=rng.randrange(1,65536),role=role,**{'replication-offset':rng.choice([-1,0,2**63-1]),'health':health,'tls-port':rng.randrange(65536)})
    for i in range(36):
        n=node(host=rng.choice([None,'','?','::1','example.test']),health=rng.choice(['online','loading','failed','future']))
        value=[dict(slots=[0,5,100,16383],nodes=[n,node('replica'),node('observer')],future=[b'a\x00b',{'nested':4}])]
        for resp3 in (False,True):check('random-shards-'+str(i),value,resp3=resp3)
        slots=[[100,16383,[n['endpoint'],n['port'],'a',{'ip':'127.0.0.1','hostname':'test'}],[None,0]],[0,5,['127.0.0.1',6379]]]
        for resp3 in (False,True):check('random-slots-'+str(i),slots,True,resp3=resp3)
    base=[dict(slots=[0,16383],nodes=[node()])]
    check('attributes',base,prefix=b'|1\r\n+future\r\n*2\r\n:1\r\n:2\r\n')
    wire=enc(base);wire=wire.replace(b'*1\r\n',b'*?\r\n',1)+b'.\r\n';assert invoke(probe,1,wire)==expected(base);rows.append(dict(name='streamed-root',wire_sha256=hashlib.sha256(wire).hexdigest(),status=1))
    check('empty-shards',[]);empty=copy.deepcopy(base);empty[0]['slots']=[];check('empty-shard-ranges',empty)
    for key in ('id','endpoint','ip','role','replication-offset','health'):
        value=copy.deepcopy(base);del value[0]['nodes'][0][key]
        for resp3 in (False,True):check('missing-'+key,value,status=-2,resp3=resp3)
    for key,bad in [('port',65536),('tls-port',-1),('replication-offset',-2),('endpoint','a b'),('endpoint','a@b'),('endpoint','http://a'),('id',None),('role',0),('health',[]),('endpoint',b'a\x00b')]:
        value=copy.deepcopy(base);value[0]['nodes'][0][key]=bad;check('bad-'+key,value,status=-2)
    value=copy.deepcopy(base);value[0]['nodes'][0]['endpoint']='a'*1025;check('endpoint-limit',value,status=-3)
    for bad in ([0,16384],[-1,2],[2,1],[0],[0,9,9,10]):
        value=copy.deepcopy(base);value[0]['slots']=bad;check('bad-ranges-'+str(bad),value,status=-2)
    value=copy.deepcopy(base);value[0]['nodes'][0]['role']='replica';check('no-master',value,status=-2)
    value=copy.deepcopy(base);value[0]['nodes'].append(node());check('two-masters',value,status=-2)
    value=copy.deepcopy(base);del value[0]['nodes'][0]['port'];del value[0]['nodes'][0]['tls-port'];check('no-port-fields',value,status=-2)
    value=copy.deepcopy(base);del value[0]['nodes'][0]['port'];check('tls-only',value)
    for resp3 in (False,True):
        n=base[0]['nodes'][0];pairs=list(n.items())+[('port',1234)];dup=((b'%' if resp3 else b'*')+str(len(pairs) if resp3 else 2*len(pairs)).encode()+b'\r\n')+b''.join(enc(k,resp3)+enc(v,resp3) for k,v in pairs)
        wire=(b'*1\r\n%2\r\n' if resp3 else b'*1\r\n*4\r\n')+enc('slots',resp3)+enc([0,16383],resp3)+enc('nodes',resp3)+b'*1\r\n'+dup
        assert invoke(probe,1,wire)==dict(status=-2);rows.append(dict(name='duplicate-port',resp3=resp3,status=-2,wire_sha256=hashlib.sha256(wire).hexdigest()))
    keys=[b'',b'123456789',b'foo',b'foo{}{bar}',b'foo{{bar}}zap',b'a\0b{tag}',b'{x}{y}']+[rng.randbytes(rng.randrange(257)) for _ in range(4096)]
    wire=b''.join(struct.pack('<I',len(k))+k for k in keys);assert invoke(probe,0,wire)==[keyslot(k) for k in keys]
    result=dict(complete=True,probe=str(probe),probe_sha256=hashlib.sha256(Path(probe).read_bytes()).hexdigest(),runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=rows,binary_keys=len(keys))
    report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('independent topology cases',len(rows),'binary keys',len(keys),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args();corpus(a.probe.absolute(),a.report)
