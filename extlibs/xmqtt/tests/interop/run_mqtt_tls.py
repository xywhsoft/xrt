"""Independent Python SSL byte peer for native MQTT TLS1.2/1.3 and mTLS.

All service sockets and keys are temporary. Only public certificate hashes enter
the report; no broker, network service or third-party Python package is needed.
"""
from pathlib import Path
import argparse,hashlib,json,platform,shutil,socket,ssl,subprocess,tempfile,threading,time

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def certificates(root,openssl):
    def run(*args):subprocess.run([openssl,*args],cwd=root,check=True,capture_output=True,timeout=60)
    for name in ['ca','wrong']:
        run('req','-x509','-newkey','rsa:2048','-nodes','-sha256','-days','2','-subj','/CN=mqtt-fixture-'+name,'-keyout',name+'.key','-out',name+'.pem','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign')
    for name,eku in [('server','serverAuth'),('client','clientAuth'),('badclient','clientAuth')]:
        run('req','-newkey','rsa:2048','-nodes','-sha256','-subj','/CN=localhost' if name=='server' else '/CN=mqtt-client','-keyout',name+'.key','-out',name+'.csr')
        (root/(name+'.ext')).write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage='+eku+'\n'+('subjectAltName=DNS:localhost,IP:127.0.0.1\n' if name=='server' else ''))
        ca='wrong' if name=='badclient' else 'ca'
        run('x509','-req','-in',name+'.csr','-CA',ca+'.pem','-CAkey',ca+'.key','-CAcreateserial','-days','2','-sha256','-extfile',name+'.ext','-out',name+'.pem')
    return {p.name:digest(p) for p in root.glob('*.pem')}
def exact(s,n):
    out=bytearray()
    while len(out)<n:
        b=s.recv(n-len(out))
        if not b:raise EOFError('truncated MQTT frame')
        out.extend(b)
    return bytes(out)
def frame(s):
    first=exact(s,1)[0];n=shift=0
    for _ in range(4):
        b=exact(s,1)[0];n|=(b&127)<<shift;shift+=7
        if not b&128:break
    else:raise AssertionError('oversized remaining length')
    assert n<65536
    return first,exact(s,n)
def peer(listener,context,version,failure,mtls,result,example=False):
    try:
        raw,_=listener.accept();raw.settimeout(10)
        with context.wrap_socket(raw,server_side=True) as s:
            result.update(tls=s.version(),alpn=s.selected_alpn_protocol(),client_certificate=bool(s.getpeercert(binary_form=True)))
            assert result['alpn']=='mqtt'
            if mtls:assert result['client_certificate']
            kind,b=frame(s);assert kind==0x10
            client_id=b'xrt-tls-example' if example else b'tls-interop'
            expected=b'\x00\x04MQTT'+bytes([5 if version==5 else 4])+b'\x02\x00\x00'+(b'\x00' if version==5 else b'')+len(client_id).to_bytes(2,'big')+client_id
            assert b==expected,(b,expected)
            assert not failure,'refused handshake must never expose MQTT CONNECT'
            ack=b'\x20\x03\x00\x00\x00' if version==5 else b'\x20\x02\x00\x00'
            for byte in ack:s.sendall(bytes([byte]))
            for qos in ([1] if example else range(3)):
                topic=b'example/xrt' if example else b'tls/topic'
                k,b=frame(s);assert k==0x30+(qos<<1);assert b[:len(topic)+2]==len(topic).to_bytes(2,'big')+topic;at=len(topic)+2
                identifier=b[at:at+2] if qos else None
                if qos:at+=2
                if version==5:assert b[at]==0;at+=1
                assert b[at:]==(b'hello' if example else b'x\0z')
                if qos:s.sendall(bytes([0x40 if qos==1 else 0x50,2])+identifier)
                if qos==2:
                    k,b=frame(s);assert k==0x62 and b==identifier;s.sendall(b'\x70\x02'+identifier)
            k,b=frame(s);assert k==0xe0 and not b
            raw=s.unwrap();raw.close();result['mqtt_qos1_and_authenticated_close' if example else 'mqtt_qos012_and_authenticated_close']=True
    except (ssl.SSLError,EOFError,ConnectionResetError,BrokenPipeError) as e:
        result['peer_error']=str(e)
        if not failure:result['failed']=True
    except BaseException as e:result.update(failed=True,peer_error=repr(e))
    finally:listener.close()
def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--example',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--openssl',default=shutil.which('openssl'));args=p.parse_args();assert args.openssl
    rows=[];examples=[]
    with tempfile.TemporaryDirectory(prefix='xmqtt-tls-') as temp:
        root=Path(temp);public=certificates(root,args.openssl)
        for tls in [12,13]:
            for version in [5,311]:
                for factory in range(3):
                    for scenario in ['plain','mtls','wrong-name','wrong-trust','missing-client','wrong-client']:
                        failure=scenario not in ['plain','mtls'];mtls=scenario in ['mtls','missing-client','wrong-client']
                        ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_2 if tls==12 else ssl.TLSVersion.TLSv1_3;ctx.set_alpn_protocols(['mqtt']);ctx.load_cert_chain(root/'server.pem',root/'server.key')
                        if mtls:ctx.verify_mode=ssl.CERT_REQUIRED;ctx.load_verify_locations(root/'ca.pem')
                        sni=[];ctx.set_servername_callback(lambda _sock,name,_ctx:sni.append(name))
                        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(12);port=listener.getsockname()[1];result={}
                        thread=threading.Thread(target=peer,args=(listener,ctx,version,failure,mtls,result));thread.start()
                        cert='client' if scenario=='mtls' else 'badclient' if scenario=='wrong-client' else None
                        cmd=[str(args.client.resolve()),str(port),str(version),str(tls),str(factory),str(int(failure)),str(root/('wrong.pem' if scenario=='wrong-trust' else 'ca.pem')),str(root/(cert+'.pem')) if cert else '-',str(root/(cert+'.key')) if cert else '-', 'wrong.fixture' if scenario=='wrong-name' else 'localhost']
                        run=subprocess.run(cmd,capture_output=True,text=True,timeout=25);thread.join(15)
                        row=dict(tls=tls,mqtt=version,factory=factory,scenario=scenario,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,sni=sni,peer=result)
                        rows.append(row);print(tls,version,factory,scenario,run.returncode,flush=True)
                        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n')
                        assert not thread.is_alive() and run.returncode==0 and not result.get('failed'),row
                        assert sni==['localhost'],row
                        if not failure:assert result.get('mqtt_qos012_and_authenticated_close'),row
        if args.example:
            for tls in [12,13]:
                for mtls in [False,True]:
                    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_2 if tls==12 else ssl.TLSVersion.TLSv1_3;ctx.set_alpn_protocols(['mqtt']);ctx.load_cert_chain(root/'server.pem',root/'server.key')
                    if mtls:ctx.verify_mode=ssl.CERT_REQUIRED;ctx.load_verify_locations(root/'ca.pem')
                    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(12);port=listener.getsockname()[1];result={}
                    thread=threading.Thread(target=peer,args=(listener,ctx,5,False,mtls,result,True));thread.start()
                    cmd=[str(args.example.resolve()),'localhost',str(port),str(root/'ca.pem')]+([str(root/'client.pem'),str(root/'client.key')] if mtls else [])
                    run=subprocess.run(cmd,capture_output=True,text=True,timeout=25);thread.join(15);row=dict(tls=tls,mtls=mtls,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,peer=result);examples.append(row);print('example',tls,mtls,run.returncode,flush=True)
                    assert not thread.is_alive() and run.returncode==0 and not result.get('failed') and result.get('mqtt_qos1_and_authenticated_close'),row
    report=dict(schema=1,complete=True,scope='independent SSL literal-byte interoperability; not broker/performance/production acceptance',platform=platform.platform(),python=platform.python_version(),ssl=ssl.OPENSSL_VERSION,openssl=subprocess.check_output([args.openssl,'version'],text=True).strip(),client_sha256=digest(args.client),runner_sha256=digest(__file__),public_certificate_sha256=public,cases=rows,examples=examples,example_sha256=digest(args.example) if args.example else None)
    args.report.write_text(json.dumps(report,indent=2)+'\n');print('[PASS] independent TLS1.2/1.3 MQTT5/311, three factories, verified server and mutual-auth refusals')
if __name__=='__main__':main()
