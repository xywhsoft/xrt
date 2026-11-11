"""Capture independent RESP2/RESP3 Pub/Sub command/reply bytes from owned Redis."""
from pathlib import Path
import argparse,hashlib,json,shutil,socket,ssl,struct,subprocess,tempfile,time
from run_redis_tls import certificates

TAGS={b'message':0,b'pmessage':1,b'smessage':2,b'subscribe':3,b'psubscribe':4,b'ssubscribe':5,b'unsubscribe':6,b'punsubscribe':7,b'sunsubscribe':8,b'pong':9}
CMDS={b'SUBSCRIBE':0,b'PSUBSCRIBE':1,b'SSUBSCRIBE':2,b'UNSUBSCRIBE':3,b'PUNSUBSCRIBE':4,b'SUNSUBSCRIBE':5,b'PING':6}
def digest(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def encode(args):return b'*'+str(len(args)).encode()+b'\r\n'+b''.join(b'$'+str(len(v)).encode()+b'\r\n'+v+b'\r\n' for v in args)
class Connection:
    def __init__(self,port,tls):
        raw=socket.create_connection(('127.0.0.1',port),timeout=5)
        self.sock=tls.wrap_socket(raw,server_hostname='localhost') if tls else raw
        self.stream=self.sock.makefile('rb')
    def close(self):self.stream.close();self.sock.close()
    def read(self):
        line=self.stream.readline(1048576);assert line.endswith(b'\r\n') and line,line
        tag=line[:1];body=line[1:-2];wire=bytearray(line)
        if tag in (b'+',b'-'):return body,bytes(wire)
        if tag==b':':return int(body),bytes(wire)
        if tag==b'_':return None,bytes(wire)
        if tag==b'$':
            n=int(body)
            if n==-1:return None,bytes(wire)
            assert 0<=n<=16777216
            data=self.stream.read(n+2);assert len(data)==n+2 and data[-2:]==b'\r\n';wire+=data;return data[:-2],bytes(wire)
        assert tag in (b'*',b'>',b'%'),line
        n=int(body)
        if n==-1:return None,bytes(wire)
        assert 0<=n<=4096
        values=[]
        for _ in range(n*(2 if tag==b'%' else 1)):
            value,raw=self.read();values.append(value);wire+=raw
        return values,bytes(wire)
    def call(self,*args):self.sock.sendall(encode(args));return self.read()

def record(records,wire,kind=255,flags=0,pong=0,channel=b'',pattern=b'',payload=b'',count=0):
    envelope={b'*':1,b'>':2,b'+':3,b'$':4}.get(wire[:1],0)
    records.append(struct.pack('<IBBBBQIII',len(wire),kind,flags,pong,envelope,count,len(channel),len(pattern),len(payload))+channel+pattern+payload+wire)

def scenario(port,tls,protocol,records):
    sub=Connection(port,tls);pub=Connection(port,tls)
    try:
        if protocol==3:
            hello,_=sub.call(b'HELLO',b'3');assert hello[hello.index(b'proto')+1]==3
        def command(name,*names):
            raw=encode((name,*names));sub.sock.sendall(raw);record(records,raw,0x80+CMDS[name])
        def ack(tag,name,count):
            value,wire=sub.read();assert value==[tag,name,count],value
            record(records,wire,TAGS[tag],1 if name is None else 0,channel=(name or b'') if tag not in (b'psubscribe',b'punsubscribe') else b'',pattern=(name or b'') if tag in (b'psubscribe',b'punsubscribe') else b'',count=count)
        def anyacks(tag,n,initial):
            seen=set()
            for i in range(n):
                value,wire=sub.read();assert value[0]==tag and value[2]==initial-i-1 and value[1] not in seen,value;seen.add(value[1])
                record(records,wire,TAGS[tag],channel=value[1] or b'' if tag!=b'punsubscribe' else b'',pattern=value[1] or b'' if tag==b'punsubscribe' else b'',count=value[2])
            return seen
        channel=b'ps\0x';payload=b'value\0\r\n'
        command(b'SUBSCRIBE',channel,b'',channel);ack(b'subscribe',channel,1);ack(b'subscribe',b'',2);ack(b'subscribe',channel,2)
        for expected in (None,b'',b'a\0\r\n'):
            command(b'PING',*(() if expected is None else (expected,)));value,wire=sub.read()
            assert value==([b'pong',expected or b''] if protocol==2 else (b'PONG' if expected is None else expected)),value
            record(records,wire,9,pong=1 if expected is None else 2,payload=expected or b'')
        command(b'PSUBSCRIBE',b'ps*',b'');ack(b'psubscribe',b'ps*',3);ack(b'psubscribe',b'',4)
        for target,pattern in ((channel,b'ps*'),(b'',b'')):
            value,_=pub.call(b'PUBLISH',target,payload);assert value==2,value;seen=set()
            for _ in range(2):
                value,wire=sub.read();assert value[0] in (b'message',b'pmessage') and value[0] not in seen,value;seen.add(value[0])
                assert value[-2:]==[target,payload],value
                if value[0]==b'pmessage':assert value[1]==pattern
                record(records,wire,TAGS[value[0]],channel=target,pattern=pattern if value[0]==b'pmessage' else b'',payload=payload)
        command(b'SSUBSCRIBE',channel,b'shard2');ack(b'ssubscribe',channel,1);ack(b'ssubscribe',b'shard2',2)
        value,_=pub.call(b'SPUBLISH',channel,payload);assert value==1,value;value,wire=sub.read();assert value==[b'smessage',channel,payload],value;record(records,wire,2,channel=channel,payload=payload)
        command(b'SUNSUBSCRIBE',channel,b'unknown');ack(b'sunsubscribe',channel,1);ack(b'sunsubscribe',b'unknown',1)
        command(b'SUNSUBSCRIBE');ack(b'sunsubscribe',b'shard2',0);command(b'SUNSUBSCRIBE');ack(b'sunsubscribe',None,0)
        command(b'UNSUBSCRIBE',channel,channel);ack(b'unsubscribe',channel,3);ack(b'unsubscribe',channel,3)
        command(b'PUNSUBSCRIBE');assert anyacks(b'punsubscribe',2,3)=={b'ps*',b''}
        command(b'UNSUBSCRIBE');ack(b'unsubscribe',b'',0);command(b'UNSUBSCRIBE');ack(b'unsubscribe',None,0)
        command(b'PUNSUBSCRIBE');ack(b'punsubscribe',None,0)
        for expected in (None,b'a\0\r\n'):
            command(b'PING',*(() if expected is None else (expected,)));value,wire=sub.read();assert value==(b'PONG' if expected is None else expected)
            record(records,wire,9,pong=1 if expected is None else 2,payload=expected or b'')
        # Non-Pub/Sub errors/ordinary values remain separate from event matching.
        value,wire=sub.call(b'NOT_A_REDIS_COMMAND');assert wire.startswith(b'-');record(records,wire)
        value,wire=sub.call(b'GET',b'missing');assert value is None;record(records,wire)
        recipient,_=sub.call(b'CLIENT',b'ID');assert isinstance(recipient,int) and recipient>0
        command(b'SUBSCRIBE',b'__redis__:invalidate');ack(b'subscribe',b'__redis__:invalidate',1)
        value,_=pub.call(b'CLIENT',b'TRACKING',b'ON',b'REDIRECT',str(recipient).encode());assert value==b'OK'
        key=b'pt:\0key'
        for flush in (False,True):
            pub.call(b'GET',key)
            value,_=pub.call(b'FLUSHDB') if flush else pub.call(b'SET',key,b'changed');assert value==b'OK'
            value,wire=sub.read()
            if protocol==2:
                assert value==[b'message',b'__redis__:invalidate',None if flush else [key]],value
                record(records,wire,0,flags=2,channel=b'__redis__:invalidate')
            else:
                assert value==[b'invalidate',None if flush else [key]],value;record(records,wire)
        value,_=pub.call(b'CLIENT',b'TRACKING',b'OFF');assert value==b'OK'
        command(b'UNSUBSCRIBE',b'__redis__:invalidate');ack(b'unsubscribe',b'__redis__:invalidate',0)
    finally:sub.close();pub.close()

def capture(server,material,version,records):
    with socket.socket() as reservation:reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    tls=None
    if version:
        tls=ssl.create_default_context(cafile=str(material/'ca.pem'));v=ssl.TLSVersion.TLSv1_2 if version==12 else ssl.TLSVersion.TLSv1_3;tls.minimum_version=v;tls.maximum_version=v
    with tempfile.TemporaryDirectory(prefix='xredis-pubsub-server-') as temporary:
        root=Path(temporary);listener=f'port {port}\n'
        if version:listener=f'port 0\ntls-port {port}\ntls-auth-clients no\ntls-cert-file "{material}/server.pem"\ntls-key-file "{material}/server.key"\ntls-ca-cert-file "{material}/ca.pem"\ntls-protocols "TLSv1.{version-10}"\n'
        config=root/'redis.conf';config.write_text('bind 127.0.0.1\nprotected-mode yes\nsave ""\nappendonly no\n'+listener,encoding='utf-8')
        with (root/'server.log').open('w') as log:
            process=subprocess.Popen([str(server),str(config)],cwd=root,stdout=log,stderr=subprocess.STDOUT)
            try:
                end=time.monotonic()+10
                while time.monotonic()<end:
                    if process.poll() is not None:raise RuntimeError((root/'server.log').read_text())
                    try:
                        connection=Connection(port,tls)
                        try:info,_=connection.call(b'INFO',b'SERVER')
                        finally:connection.close()
                        fields=dict(line.split(b':',1) for line in info.split(b'\r\n') if b':' in line)
                        assert int(fields[b'process_id'])==process.pid and len(fields[b'run_id'])==40;break
                    except OSError:time.sleep(0.025)
                else:raise TimeoutError('owned Redis startup')
                rows=[]
                for protocol in (2,3):
                    before=len(records);scenario(port,tls,protocol,records);rows.append(dict(protocol=protocol,transport=version,records=len(records)-before,passed=True,server_pid_verified=True,server_run_id=fields[b'run_id'].decode()))
                assert process.poll() is None
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            for row in rows:row['server_reaped']=process.poll() is not None
            return rows
def main():
    p=argparse.ArgumentParser();p.add_argument('--server',type=Path,default=Path(shutil.which('redis-server') or 'redis-server'));p.add_argument('--corpus',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    a.server=a.server.absolute();a.corpus.parent.mkdir(parents=True,exist_ok=True);a.report.parent.mkdir(parents=True,exist_ok=True)
    records=[];result=dict(schema=1,complete=False,server_version=subprocess.check_output([str(a.server),'--version'],text=True).strip(),server_sha256=digest(a.server),runner_sha256=digest(__file__),cases=[])
    with tempfile.TemporaryDirectory(prefix='runtime-material-',dir=a.report.parent) as temporary:
        material=Path(temporary);certificates(Path(shutil.which('openssl')),material)
        for version in (0,12,13):result['cases']+=capture(a.server,material,version,records)
        result['public_certificate_sha256']={n:digest(material/n) for n in ('ca.pem','server.pem')}
    a.corpus.write_bytes(b'XPS1'+b''.join(records));result.update(complete=True,records=len(records),corpus_sha256=digest(a.corpus),runtime_material_removed=not material.exists());a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('captured',len(records),'records across',len(result['cases']),'owned Redis scenarios')
if __name__=='__main__':main()
