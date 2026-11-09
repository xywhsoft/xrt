"""Independent SSL TLS1.3 credential handoff oracle; runtime-only credentials."""
from pathlib import Path
import argparse,json,platform,shutil,socket,ssl,subprocess,tempfile,threading
from run_mqtt_tls import certificates,digest,frame

def context(root):
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_3;ctx.set_alpn_protocols(['mqtt']);ctx.load_cert_chain(root/'server.pem',root/'server.key');ctx.num_tickets=2
    return ctx
def peer(listener,root,mode,version,result,example=False):
    ctx=context(root);rounds=1 if mode>=3 else 2
    try:
        for i in range(rounds):
            if i and mode in [1,2]:ctx=context(root)
            raw,_=listener.accept();raw.settimeout(10)
            try:
                with ctx.wrap_socket(raw,server_side=True) as s:
                    row=dict(tls=s.version(),resumed=s.session_reused,alpn=s.selected_alpn_protocol());result.append(row)
                    assert row['alpn']=='mqtt' and row['resumed']==bool(i and mode==0),row
                    k,b=frame(s);assert k==0x10;assert not(i and mode==1),'ResumeOnly must expose no MQTT on a rejected PSK'
                    client=b'tls-resume-example' if example else b'tls-resume-interop'
                    expected=b'\0\x04MQTT'+bytes([5 if version==5 else 4])+b'\x02\0\0'+(b'\0' if version==5 else b'')+len(client).to_bytes(2,'big')+client;assert b==expected,(b,expected)
                    s.sendall(b'\x20\x03\0\0\0' if version==5 else b'\x20\x02\0\0')
                    k,b=frame(s);assert k==0x32;topic=b'example/resume' if example else b'resume/topic';assert b[:len(topic)+2]==len(topic).to_bytes(2,'big')+topic;at=len(topic)+2;identifier=b[at:at+2];at+=2
                    if version==5:assert b[at]==0;at+=1
                    assert b[at:]==(b'hello' if example else b'x\0z');s.sendall(b'\x40\x02'+identifier);k,b=frame(s);assert k==0xe0 and not b;raw=s.unwrap();raw.close();row['fresh_mqtt_and_authenticated_close']=True
            except (ssl.SSLError,EOFError,ConnectionResetError,BrokenPipeError) as e:
                if not(i and mode==1):raise
                result.append(dict(expected_refusal=True,error=str(e)))
    except BaseException as e:result.append(dict(failed=True,error=repr(e)))
    finally:listener.close()
def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--example',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--openssl',default=shutil.which('openssl'));a=p.parse_args();assert a.openssl;rows=[]
    with tempfile.TemporaryDirectory(prefix='xmqtt-resume-') as temp:
        root=Path(temp);public=certificates(root,a.openssl)
        for version in [5,311]:
            for factory in range(3):
                for mode in range(6):
                    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(2);listener.settimeout(12);port=listener.getsockname()[1];out=[];thread=threading.Thread(target=peer,args=(listener,root,mode,version,out));thread.start()
                    run=subprocess.run([str(a.client.resolve()),str(port),str(mode),str(factory),str(version),str(root/'ca.pem')],capture_output=True,text=True,timeout=35);thread.join(15);row=dict(mqtt=version,factory=factory,scenario=mode,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,peer=out);rows.append(row);print(version,factory,mode,run.returncode,flush=True)
                    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n');assert not thread.is_alive() and run.returncode==0 and not any(x.get('failed') for x in out),row
                    if mode==0:assert len(out)==2 and out[1]['resumed'],row
        examples=[]
        if a.example:
            listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(2);listener.settimeout(12);port=listener.getsockname()[1];out=[];thread=threading.Thread(target=peer,args=(listener,root,0,5,out,True));thread.start();run=subprocess.run([str(a.example.resolve()),'localhost',str(port),str(root/'ca.pem')],capture_output=True,text=True,timeout=35);thread.join(15);row=dict(exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,peer=out);examples.append(row);assert not thread.is_alive() and run.returncode==0 and len(out)==2 and out[1]['resumed'] and not any(x.get('failed') for x in out),row
    a.report.write_text(json.dumps(dict(schema=1,complete=True,scope='independent SSL1.3 resumption and literal MQTT, not broker/performance/production acceptance',platform=platform.platform(),python=platform.python_version(),ssl=ssl.OPENSSL_VERSION,client_sha256=digest(a.client),runner_sha256=digest(__file__),common_runner_sha256=digest(Path(__file__).with_name('run_mqtt_tls.py')),public_certificate_sha256=public,cases=rows,examples=examples,example_sha256=digest(a.example) if a.example else None),indent=2)+'\n');print('[PASS] independent SSL true resumption, required-resume refusal, full fallback and pre-network credential binding/expiry')
if __name__=='__main__':main()
