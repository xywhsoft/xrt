"""Independent stdlib/OpenSSL HTTP Upgrade and RFC6455 JSON-RPC peers.

Only local loopback; ephemeral keys live under output-dir and are removed.
No implementation library or WebSocket package is imported by the Python peer.
"""
import argparse,base64,hashlib,json,os,platform,queue,shutil,socket,ssl,struct,subprocess,sys,tempfile,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--quick',action='store_true');p.add_argument('--only');a=p.parse_args()
binary=a.binary if a.binary.is_absolute() else ROOT/a.binary;out=a.output_dir if a.output_dir.is_absolute() else ROOT/a.output_dir;out.mkdir(parents=True,exist_ok=True);assert binary.is_file()
openssl=shutil.which('openssl')
if not openssl and os.name=='nt':
 candidate=Path('C:/Program Files/Git/usr/bin/openssl.exe')
 if candidate.is_file():openssl=str(candidate)
assert openssl,'OpenSSL CLI required for ephemeral local identity'
GUID=b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11';keys=[];cases=[]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')

def exact(s,n):
 data=b''
 while len(data)<n:
  part=s.recv(n-len(data))
  if not part:raise EOFError('incomplete HTTP/WS message')
  data+=part
 return data

def head(s):
 wire=b''
 while not wire.endswith(b'\r\n\r\n'):
  wire+=exact(s,1);assert len(wire)<=65536
 lines=wire[:-4].split(b'\r\n');fields={}
 for line in lines[1:]:
  name,value=line.split(b':',1);fields.setdefault(name.lower(),[]).append(value.strip())
 return lines[0],fields

def no_rpc(s):
 s.settimeout(.05)
 try:assert not s.recv(1),'RPC/application bytes before checked101'
 except socket.timeout:pass
 else:raise AssertionError('connection ended before expected101')
 finally:s.settimeout(10)

def wire(value,masked,opcode=1,fin=True):
 body=value if isinstance(value,bytes) else json.dumps(value,ensure_ascii=False,separators=(',',':')).encode()
 n=len(body);assert n<1048576;prefix=bytes([(128 if fin else 0)|opcode,(128 if masked else 0)|(n if n<126 else 126 if n<65536 else 127)])
 if n>=126:prefix+=struct.pack('!H' if n<65536 else '!Q',n)
 if masked:
  key=os.urandom(4);return prefix+key+bytes(b^key[i%4] for i,b in enumerate(body))
 return prefix+body

def receive(s,masked):
 h=exact(s,2);assert h[0]&128 and not h[0]&112 and bool(h[1]&128)==masked
 n=h[1]&127
 if n==126:n=struct.unpack('!H',exact(s,2))[0]
 if n==127:n=struct.unpack('!Q',exact(s,8))[0]
 assert n<1048576;key=exact(s,4) if masked else None;body=exact(s,n)
 if key:body=bytes(b^key[i%4] for i,b in enumerate(body))
 return h[0]&15,body

def send(s,data,fragment):
 for at in range(0,len(data),fragment):s.sendall(data[at:at+fragment])

def exchange(s,native_client,fragment,already_sent=False,upgrade_ping=False):
 masked=not native_client
 requests=[{'jsonrpc':'2.0','id':2**70+17,'method':'echo','params':['hello\u0000\u4e2d\U0001f63a']},[{'jsonrpc':'2.0','id':23,'method':'echo','params':[]},{'jsonrpc':'2.0','method':'note'},{'jsonrpc':'2.0','id':None,'method':'echo','params':[1]}]]
 if not native_client:requests.append({'jsonrpc':'2.0','id':'identity','method':'who'})
 if not already_sent:
  for value in requests:send(s,wire(value,masked),fragment)
 done=set();pong=False;expected={'batch','wide','callback','inner'}
 if not native_client:expected.add('identity')
 if upgrade_ping:expected.add('upgrade')
 send(s,wire(b'pulse',masked,9),fragment)
 while not expected<=done or not pong:
  opcode,body=receive(s,native_client)
  if opcode==10:
   if body==b'pulse':pong=True
   elif body==b'upgrade':assert upgrade_ping;done.add('upgrade')
   else:assert body==b'inner';done.add('inner')
   continue
  assert opcode==1;value=json.loads(body)
  if isinstance(value,list):assert {x['id']:x['result'] for x in value}=={23:[],None:[1]};done.add('batch')
  elif 'method' in value:
   assert value['method']=='python_echo' and value['params']==[42]
   answer=json.dumps({'jsonrpc':'2.0','id':value['id'],'result':[42]},separators=(',',':')).encode();cut=5
   send(s,wire(answer[:cut],masked,1,False)+wire(b'inner',masked,9)+wire(answer[cut:],masked,0,True),fragment);done.add('callback')
  elif value['id']==2**70+17:assert value['result']==['hello\u0000\u4e2d\U0001f63a'];done.add('wide')
  elif value['id']=='identity':assert value['result']==[42];done.add('identity')
  else:raise AssertionError(value)
 # An acknowledged RPC barrier precedes normal WS/TLS shutdown. A notification
 # alone is not an execution acknowledgement and must not be used as a join.
 send(s,wire({'jsonrpc':'2.0','id':'final-barrier','method':'finish','params':[]},masked),fragment)
 while True:
  opcode,body=receive(s,native_client)
  if opcode==10:assert body in (b'pulse',b'inner');continue
  assert opcode==1;value=json.loads(body);assert value['id']=='final-barrier' and value['result']==[];break
 send(s,wire(struct.pack('!H',1000),masked,8),fragment)
 while True:
  opcode,body=receive(s,native_client)
  if opcode==10:continue
  assert opcode==8 and (not body or body[:2]==struct.pack('!H',1000));break

def client_case(s,mode,fragment):
 start,f=head(s);assert start==b'GET /rpc HTTP/1.1' and f[b'host']==[b'rpc.test'] and f[b'authorization']==[b'Bearer fixture'] and f[b'sec-websocket-protocol']==[b'jsonrpc']
 key=f[b'sec-websocket-key'][0];assert len(base64.b64decode(key,validate=True))==16;keys.append(key.decode());no_rpc(s)
 if mode=='timeout':time.sleep(.9);return None
 if mode=='reject':send(s,b'HTTP/1.1 403 Forbidden\r\nConnection: close\r\nContent-Length: 0\r\n\r\n',fragment);return 403
 accept=base64.b64encode(hashlib.sha1(key+GUID).digest());fields=[b'Upgrade: websocket',b'Connection: Upgrade',b'Sec-WebSocket-Accept: '+accept,b'Sec-WebSocket-Protocol: jsonrpc']
 if mode=='bad-accept':fields[2]=b'Sec-WebSocket-Accept: '+b'A'*28
 if mode=='wrong-protocol':fields[3]=b'Sec-WebSocket-Protocol: other'
 if mode=='missing-protocol':fields.pop()
 if mode=='duplicate-accept':fields.append(fields[2])
 if mode=='body-framing':fields.append(b'Content-Length: 0')
 response=b'HTTP/1.1 101 Switching Protocols\r\n'+b'\r\n'.join(fields)+b'\r\n\r\n'
 if mode=='normal':response+=wire(b'upgrade',False,9)
 send(s,response,fragment)
 if mode=='normal':exchange(s,True,fragment,upgrade_ping=True)
 return 101

def server_case(s,mode,fragment):
 key=base64.b64encode(os.urandom(16));target=b'/wrong' if mode=='route' else b'/rpc';method=b'POST' if mode=='method' else b'GET';version=b'12' if mode=='version' else b'13';auth=b'wrong' if mode=='unauthorized' else b'Bearer fixture'
 fields=[b'Host: rpc.test',b'Upgrade: websocket',b'Connection: Upgrade',b'Sec-WebSocket-Key: '+key,b'Sec-WebSocket-Version: '+version,b'Sec-WebSocket-Protocol: jsonrpc',b'Authorization: '+auth]
 if mode=='duplicate-origin':fields += [b'Origin: https://one',b'Origin: https://two']
 if mode=='duplicate-auth':fields.append(b'Authorization: Bearer fixture')
 if mode=='many-fields':fields += [b'X-Pad: a' for _ in range(140)]
 request=method+b' '+target+b' HTTP/1.1\r\n'+b'\r\n'.join(fields)+b'\r\n\r\n';send(s,request,fragment)
 if mode=='auth-timeout':
  try:assert not s.recv(1),'no HTTP101 or RPC after expired authorization'
  except (ssl.SSLEOFError,ConnectionResetError):pass
  return 0
 start,f=head(s);status=int(start.split()[1]);expected={'normal':101,'route':404,'method':405,'version':426,'unauthorized':401,'duplicate-origin':400,'duplicate-auth':400,'many-fields':431}[mode];assert status==expected,(mode,status)
 if status!=101:
  assert f[b'content-length']==[b'0'] and f[b'connection']==[b'close']
  if status==401:assert f[b'www-authenticate']==[b'Bearer']
  if status==426:assert f[b'sec-websocket-version']==[b'13'] and f[b'upgrade']==[b'websocket']
  if status==405:assert f[b'allow']==[b'GET']
  return status
 assert f[b'sec-websocket-accept']==[base64.b64encode(hashlib.sha1(key+GUID).digest())] and f[b'sec-websocket-protocol']==[b'jsonrpc'];exchange(s,False,fragment);return status

with tempfile.TemporaryDirectory(prefix='ws-http-',dir=out) as temporary:
 temp=Path(temporary);assert temp.resolve().is_relative_to(out.resolve())
 def certificate(*args):subprocess.run([openssl,*args],cwd=temp,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True,timeout=40)
 certificate('req','-x509','-newkey','rsa:2048','-nodes','-sha256','-days','2','-subj','/CN=XRT WS RPC local CA','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign','-keyout','ca.key','-out','ca.pem')
 (temp/'extensions.cnf').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:rpc.test\n')
 certificate('genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out','peer.key');certificate('req','-new','-key','peer.key','-subj','/CN=rpc.test','-out','peer.csr');certificate('x509','-req','-in','peer.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','2','-sha256','-extfile','extensions.cnf','-out','peer.pem');certificate('x509','-in','peer.pem','-outform','DER','-out','peer.der');certificate('pkcs8','-topk8','-nocrypt','-in','peer.key','-outform','DER','-out','key.der')
 cert_sha=hashlib.sha256((temp/'peer.der').read_bytes()).hexdigest()
 for secure in (False,True):
  for role in ('client','server'):
   modes=['normal'] if a.quick else (['normal','bad-accept','wrong-protocol','missing-protocol','duplicate-accept','body-framing','reject','timeout'] if role=='client' else ['normal','route','method','version','unauthorized','duplicate-origin','duplicate-auth','many-fields','auth-timeout'])
   for mode in modes:
    fragments=(1,65536) if mode=='normal' else (65536,)
    for fragment in fragments:
     if a.only and a.only!=':'.join((role,'tls' if secure else 'tcp',mode,str(fragment))):continue
     listener=s=process=reader=None
     try:
      if role=='client':listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15);port=listener.getsockname()[1]
      else:port=0
      context=None
      if secure:
       context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER if role=='client' else ssl.PROTOCOL_TLS_CLIENT)
       if role=='client':context.load_cert_chain(str(temp/'peer.pem'),str(temp/'peer.key'))
       else:context.load_verify_locations(cafile=str(temp/'ca.pem'))
       context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_3
      process=subprocess.Popen([str(binary),role,'tls' if secure else 'tcp',mode,str(port),str(temp/'peer.der'),str(temp/'key.der')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
      lines=queue.Queue()
      def read_lines(proc=process,q=lines):
       for line in proc.stdout:q.put(line)
       q.put(None)
      reader=threading.Thread(target=read_lines);reader.start();line=lines.get(timeout=15);assert line,process.stderr.read()
      if role=='client':assert line.startswith('READY'),line;raw,_=listener.accept();s=context.wrap_socket(raw,server_side=True) if secure else raw
      else:assert line.startswith('PORT '),line;raw=socket.create_connection(('127.0.0.1',int(line.split()[1])),timeout=15);s=context.wrap_socket(raw,server_hostname='rpc.test') if secure else raw
      s.settimeout(10);version=s.version() if secure else None;status=(client_case if role=='client' else server_case)(s,mode,fragment)
      if secure and mode=='normal':
       try:raw=s.unwrap();raw.close()
       except (ssl.SSLEOFError,ConnectionResetError,ssl.SSLZeroReturnError):pass
      s.close();s=None
      code=process.wait(timeout=20);reader.join(timeout=5);error=process.stderr.read();remaining=[]
      while not lines.empty():remaining.append(lines.get())
      assert code==0 and any(x and x.startswith('PASS role=') for x in remaining),(role,secure,mode,code,remaining,error)
      cases.append(dict(native_role=role,secure=secure,mode=mode,fragment_write_bytes=fragment,status=status,tls=version,exit_code=code,summary=[x.strip() for x in remaining if x]));print('[case]',role,'tls' if secure else 'tcp',mode,fragment,'PASS',flush=True)
     except Exception:
      if process:
       if process.poll() is None:process.kill();process.wait(timeout=10)
       error=process.stderr.read()
       if error:print('[child diagnostic]',role,secure,mode,error,flush=True)
      raise
     finally:
      if s:s.close()
      if listener:listener.close()
      if process and process.poll() is None:process.kill();process.wait(timeout=10)
      if process:
       if process.stdout:process.stdout.close()
       if process.stderr:process.stderr.close()
      if reader:reader.join(timeout=5)
 assert len(keys)==len(set(keys)),'client nonce reused between independent connections'
report=dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),python=sys.version,openssl=ssl.OPENSSL_VERSION,platform=platform.platform(),public_certificate_sha256=cert_sha,private_keys_recorded=False,cases=cases,scope='Actual local HTTP Upgrade and RFC6455/JSON sessions, both endpoint roles, no application bytes before101, shared authorization, strict failures, original timeout and physical joins; no performance or native72h acceptance.')
a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print('[PASS]',len(cases),'independent WS HTTP sessions')
