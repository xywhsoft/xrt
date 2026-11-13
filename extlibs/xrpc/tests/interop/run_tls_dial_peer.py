"""Independent stdlib/OpenSSL TLS13 + JSON/framing peer in DNS/prepared client roles.
Credentials are generated at runtime under the requested output directory and
removed on exit. Public certificate hashes only are recorded, never keys.
"""
import argparse,hashlib,json,os,platform,queue,shutil,socket,ssl,struct,subprocess,sys,tempfile,threading
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('--output-dir',type=Path,default=ROOT/'out/local-interop/tls-peer');p.add_argument('--report',type=Path);p.add_argument('--quick',action='store_true');a=p.parse_args();binary=a.binary if a.binary.is_absolute() else ROOT/a.binary;assert binary.is_file()
out=a.output_dir if a.output_dir.is_absolute() else ROOT/a.output_dir;out.mkdir(parents=True,exist_ok=True)
openssl=shutil.which('openssl')
if not openssl and os.name=='nt':
 candidate=Path('C:/Program Files/Git/usr/bin/openssl.exe')
 if candidate.is_file():openssl=str(candidate)
assert openssl,'OpenSSL CLI required for ephemeral credentials'

def exact(s,n):
 data=b''
 while len(data)<n:
  part=s.recv(n-len(data))
  if not part:raise EOFError('partial application frame')
  data+=part
 return data

def receive(s,fmt):
 if fmt==2:n=struct.unpack('!I',exact(s,4))[0]
 else:
  head=b''
  while not head.endswith(b'\r\n\r\n'):
   head+=exact(s,1);assert len(head)<8192
  fields=dict(x.split(b':',1) for x in head[:-4].split(b'\r\n'));n=int(fields[b'Content-Length'])
 assert n<1048576
 return json.loads(exact(s,n))

def send(s,fmt,value,fragment):
 body=json.dumps(value,ensure_ascii=False,separators=(',',':')).encode();wire=(struct.pack('!I',len(body)) if fmt==2 else b'Content-Length: '+str(len(body)).encode()+b'\r\n\r\n')+body
 for i in range(0,len(wire),fragment):s.sendall(wire[i:i+fragment])

def exchange(s,fmt,fragment):
 assert s.version()=='TLSv1.3' and s.selected_alpn_protocol()=='xrpc-local-test'
 send(s,fmt,{'jsonrpc':'2.0','id':'py\x00\u4e2d\U0001f63a','method':'echo','params':{'text':'hello\u4e16\u754c','n':2**70}},fragment)
 send(s,fmt,[{'jsonrpc':'2.0','id':23,'method':'echo','params':[1,2]},{'jsonrpc':'2.0','method':'note'},{'jsonrpc':'2.0','id':None,'method':'echo','params':[]}],fragment)
 send(s,fmt,{'jsonrpc':'2.0','id':2**70+17,'method':'echo','params':['wide-id']},fragment)
 single=batch=wide=callback=False
 while not (single and batch and wide and callback):
  msg=receive(s,fmt)
  if isinstance(msg,list):
   assert len(msg)==2 and {x['id']:x['result'] for x in msg}=={23:[1,2],None:[]};batch=True
  elif 'method' in msg:
   assert msg['method']=='python_echo' and msg['params']==[42];send(s,fmt,{'jsonrpc':'2.0','id':msg['id'],'result':msg['params']},fragment);callback=True
  elif msg['id']==2**70+17:assert msg['result']==['wide-id'];wide=True
  else:assert msg['id']=='py\x00\u4e2d\U0001f63a' and msg['result']=={'text':'hello\u4e16\u754c','n':2**70};single=True
 # All application outcomes were validated before authenticated write closure.
 # The RPC peer deliberately aborts on EOF; unwrap may see its abort instead of
 # the reciprocal close_notify. No EOF/reset during an expected frame is allowed.
 try:
  raw=s.unwrap();raw.close();return 'reciprocal-close-notify'
 except (ssl.SSLEOFError,ConnectionResetError,ssl.SSLZeroReturnError):return 'peer-abort-after-all-application-replies'

cases=[];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with tempfile.TemporaryDirectory(prefix='framed-tls-',dir=out) as temporary:
 temp=Path(temporary);assert temp.resolve().is_relative_to(out.resolve())
 def certificate(*args):subprocess.run([openssl,*args],cwd=temp,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True,timeout=40)
 certificate('req','-x509','-newkey','rsa:2048','-nodes','-sha256','-days','2','-subj','/CN=XRT framed RPC local CA','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign','-keyout','ca.key','-out','ca.pem')
 (temp/'extensions.cnf').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:rpc.test\n')
 kinds=[('rsa',['-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048'])]
 if not a.quick:kinds += [('p256',['-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256']),('p384',['-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-384']),('ed25519',['-algorithm','ED25519'])]
 for kind,key_args in kinds:
  certificate('genpkey',*key_args,'-out','peer.key');certificate('req','-new','-key','peer.key','-subj','/CN=rpc.test','-out','peer.csr');certificate('x509','-req','-in','peer.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','2','-sha256','-extfile','extensions.cnf','-out','peer.pem');certificate('x509','-in','peer.pem','-outform','DER','-out','peer.der');certificate('pkcs8','-topk8','-nocrypt','-in','peer.key','-outform','DER','-out','key.der')
  der=(temp/'peer.der').read_bytes();cert_sha=hashlib.sha256(der).hexdigest()
  for role in ('dns','prepared'):
   for fmt in (1,2):
    for fragment in (1,65536):
     listener=None;s=None;process=None
     try:
      if role in ('dns','prepared'):
       listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15);port=listener.getsockname()[1];context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(str(temp/'peer.pem'),str(temp/'peer.key'))
      else:
       port=0;context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);context.load_verify_locations(cafile=str(temp/'ca.pem'))
      context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_3;context.set_alpn_protocols(['xrpc-local-test'])
      process=subprocess.Popen([str(binary),str(fmt),str(temp/'peer.der'),str(temp/'key.der'),kind,role,str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
      lines=queue.Queue()
      def read_lines(proc=process,q=lines):
       for line in proc.stdout:q.put(line)
       q.put(None)
      reader=threading.Thread(target=read_lines);reader.start();line=lines.get(timeout=15);assert line,process.stderr.read()
      if role=='accept':
       assert line.startswith('PORT '),line;s=context.wrap_socket(socket.create_connection(('127.0.0.1',int(line.split()[1])),timeout=15),server_hostname='rpc.test')
      else:
       assert line.startswith('READY'),line;raw,_=listener.accept();s=context.wrap_socket(raw,server_side=True)
      s.settimeout(15);version=s.version();cipher=s.cipher()[0];closure=exchange(s,fmt,fragment);s.close();s=None
      code=process.wait(timeout=25);reader.join(timeout=5);error=process.stderr.read();remaining=[]
      while not lines.empty():remaining.append(lines.get())
      assert code==0 and any(x and x.startswith('PASS native TLS') for x in remaining),(code,remaining,error)
      cases.append(dict(native_role=role,identity=kind,frame_format=fmt,fragment_bytes=fragment,tls=version,cipher=cipher,public_certificate_sha256=cert_sha,closure=closure,exit_code=code))
     finally:
      if s:s.close()
      if listener:listener.close()
      if process and process.poll() is None:process.kill();process.wait(timeout=10)
      if process:
       if process.stdout:process.stdout.close()
       if process.stderr:process.stderr.close()
report={'binary':str(binary),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'python':sys.version,'python_ssl':ssl.OPENSSL_VERSION,'openssl_generator':subprocess.check_output([openssl,'version'],text=True).strip(),'platform':platform.platform(),'cases':cases,'private_keys_recorded':False,'scope':'Actual independent TLS13/JSON/framing sessions, native DNS/prepared client, two frames, Unicode/NUL/lossless wide numeric IDs, batch/null ID/notification, bidirectional calls and EOF retirement. This is local functional evidence, not production throughput/72h acceptance.'}
if a.report:a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n')
print('[PASS]',len(cases),'independent TLS13 framed RPC sessions; native DNS/prepared client, RSA/ECDSA/Ed25519 as selected, fragment/coalesced, copied ALPN, exact IDs, batch/callback and EOF')
