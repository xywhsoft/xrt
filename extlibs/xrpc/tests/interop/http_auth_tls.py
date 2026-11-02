"""Independent verified HTTPS client for the C server's runtime authorization.

Only public fixture credentials are used. Runtime key/credential files are
removed on exit; receipts retain public certificate and binary/script hashes.
"""
import argparse
import base64
import hashlib
import http.client
import json
import os
from pathlib import Path
import platform
import queue
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

root=Path(__file__).resolve().parents[4]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',type=Path)
parser.add_argument('--report',type=Path)
parser.add_argument('--auth-mode',choices=['basic','bearer'],default='bearer')
args=parser.parse_args();name,arch,suffix=('windows','native','.exe') if os.name=='nt' else ('wsl','x64','')
out=root/'out/service-extensions/xrpc-http-auth';out.mkdir(parents=True,exist_ok=True)
binary=(args.binary or root/'out/gcc'/arch/'xrpc'/('examples_http_server_tls_main'+suffix)).resolve()
report=args.report or out/(name+'-'+args.auth_mode+'-interop.json')
openssl=shutil.which('openssl')
if not openssl and os.name=='nt':openssl=str(Path('C:/Program Files/Git/usr/bin/openssl.exe'))
assert openssl and Path(openssl).is_file() and binary.is_file()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();cases=[];accepted=0
token='Basic '+base64.b64encode(b'public:fixture').decode() if args.auth_mode=='basic' else 'Bearer public-fixture'
with tempfile.TemporaryDirectory(prefix='auth-peer-',dir=out) as temporary:
    temp=Path(temporary).resolve();assert temp.is_relative_to(out.resolve())
    def cert(*cmd):subprocess.run([openssl,*cmd],cwd=temp,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30)
    cert('req','-x509','-newkey','rsa:2048','-nodes','-sha256','-days','2','-subj','/CN=XRT local auth CA','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign','-keyout','ca.key','-out','ca.pem')
    cert('req','-new','-newkey','rsa:2048','-nodes','-sha256','-subj','/CN=rpc.test','-keyout','peer.key','-out','peer.csr')
    (temp/'extensions.cnf').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:rpc.test\n',encoding='utf-8')
    cert('x509','-req','-in','peer.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','2','-sha256','-extfile','extensions.cnf','-out','peer.pem')
    cert('x509','-in','peer.pem','-outform','DER','-out','peer.der');cert('pkcs8','-topk8','-nocrypt','-in','peer.key','-outform','DER','-out','key.der')
    (temp/'authorization.txt').write_bytes((token+'\r\n').encode('ascii'))
    env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    command=[str(binary),str(temp/'peer.der'),str(temp/'key.der'),'0','5000','rsa',str(temp/'authorization.txt')]
    process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
    lines=queue.Queue()
    reader=threading.Thread(target=lambda:lines.put(process.stdout.readline()),daemon=True);reader.start()
    try:
        endpoint=lines.get(timeout=15).strip();assert endpoint.startswith('https://127.0.0.1:') and endpoint.endswith('/rpc'),endpoint
        port=int(endpoint.split(':')[2].split('/')[0])
        def context(version=ssl.TLSVersion.TLSv1_3):
            c=ssl.create_default_context(cafile=str(temp/'ca.pem'));c.minimum_version=c.maximum_version=version;c.set_alpn_protocols(['http/1.1'])
            if version==ssl.TLSVersion.TLSv1_2:c.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
            return c
        def connect(version=ssl.TLSVersion.TLSv1_3):
            s=context(version).wrap_socket(socket.create_connection(('127.0.0.1',port),timeout=5),server_hostname='rpc.test');s.settimeout(5);return s
        def exchange(label,body,headers,status,expected=None,version=ssl.TLSVersion.TLSv1_3):
            global accepted
            c=http.client.HTTPSConnection('rpc.test',port,timeout=5,context=context(version));c.sock=connect(version)
            try:
                c.request('POST','/rpc',body,headers={'Content-Type':'application/json',**headers});r=c.getresponse();wire=r.read();assert r.status==status,(label,r.status,wire)
                if status==401:assert r.getheader('WWW-Authenticate').startswith('Basic' if args.auth_mode=='basic' else 'Bearer')
                if expected is not None:assert json.loads(wire)==expected,(label,wire)
                if status in [200,204]:accepted+=1
                cases.append({'label':label,'status':r.status,'tls_version':c.sock.version() if c.sock else version.name})
            finally:c.close()
        call=json.dumps({'jsonrpc':'2.0','id':1,'method':'echo','params':[7]})
        reply={'jsonrpc':'2.0','id':1,'result':[7]}
        exchange('missing',call,{},401);exchange('wrong',call,{'Authorization':token+'-wrong'},401)
        exchange('echo',call,{'Authorization':token},200,reply)
        exchange('tls12',call,{'Authorization':token},200,reply,ssl.TLSVersion.TLSv1_2)
        identity=json.dumps({'jsonrpc':'2.0','id':2,'method':'identity'})
        exchange('identity',identity,{'Authorization':token},200,{'jsonrpc':'2.0','id':2,'result':[42]})
        batch=json.dumps([{'jsonrpc':'2.0','id':3,'method':'identity'},{'jsonrpc':'2.0','method':'echo','params':[]}])
        exchange('batch',batch,{'Authorization':token},200,[{'jsonrpc':'2.0','id':3,'result':[42]}])
        exchange('notification',json.dumps({'jsonrpc':'2.0','method':'echo','params':[]}),{'Authorization':token},204)
        exchange('large-head',call,{'Authorization':token,'X-Pad':'x'*6000},200,reply)
        def response(f):
            line=f.readline(8192);assert line.startswith(b'HTTP/1.1 '),line;status=int(line.split()[1]);headers={}
            for _ in range(64):
                line=f.readline(8192)
                if line==b'\r\n':break
                key,value=line.split(b':',1);headers[key.lower()]=value.strip()
            else:raise AssertionError('response header count')
            n=int(headers.get(b'content-length',b'0'));assert n<=65536;body=f.read(n);assert len(body)==n;return status,headers,body
        def raw(label,authorization,expect=False,duplicate=False,pipeline=False):
            global accepted
            s=connect();f=s.makefile('rb')
            try:
                data=call.encode();fields=f'POST /rpc HTTP/1.1\r\nHost: rpc.test\r\nContent-Type: application/json\r\nContent-Length: {len(data)}\r\nAuthorization: {authorization}\r\n'
                if duplicate:fields+='authorization: duplicate\r\n'
                if expect:fields+='Expect: 100-continue\r\n'
                head=(fields+'\r\n').encode();tail=head.replace(authorization.encode(),token.encode(),1)+data if pipeline else b''
                s.sendall(head if expect else head+data+tail);status,h,body=response(f)
                if expect and authorization==token and not duplicate:
                    assert status==100,(label,status);s.sendall(data);status,h,body=response(f);assert status==200 and json.loads(body)==reply;accepted+=1
                else:
                    assert status==(400 if duplicate else 401),(label,status)
                    assert h.get(b'connection')==b'close' and not body
                    if status==401:assert b'www-authenticate' in h
                case={'label':label,'status':status}
                if pipeline:
                    try:assert f.read(1)==b'', 'denied connection processed its authorized tail'
                    except (ConnectionResetError,ssl.SSLEOFError):pass
                    case['authorized_tail_closed']=True
                cases.append(case)
            finally:f.close();s.close()
        raw('authorized-expect',token,expect=True);raw('denied-expect',token+'-wrong',expect=True)
        raw('duplicate',token,duplicate=True);raw('denied-pipeline',token+'-wrong',pipeline=True)
        stdout,stderr=process.communicate(timeout=20);assert process.returncode==0,(process.returncode,stdout,stderr)
        assert 'AddressSanitizer' not in stderr and 'runtime error:' not in stderr,stderr
        stats=json.loads(next(line[6:] for line in stdout.splitlines() if line.startswith('stats ')))
        assert stats['requests']==stats['dispatched']==accepted and stats['connections']==stats['queued']==stats['active_handshakes']==0,stats
        assert accepted==7 and len(cases)==12,(accepted,cases)
        certificates={n:sha(temp/n) for n in ['ca.pem','peer.der']}
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=10)
        reader.join(5)
        if process.stdout:process.stdout.close()
        if process.stderr:process.stderr.close()
report.write_text(json.dumps({'binary':str(binary),'binary_sha256':sha(binary),'script_sha256':sha(Path(__file__)),'platform':platform.platform(),'python_tls':ssl.OPENSSL_VERSION,'auth_mode':args.auth_mode,'public_certificates':certificates,'cases':cases,'accepted_documents':accepted,'stats':stats,'exit_code':0},indent=2)+'\n',encoding='utf-8')
print('PASS independent HTTPS authorization: Basic/Bearer fixture, identity, batch, TLS 1.3/1.2, large head, Continue gating, duplicate and pipelined refusals')
