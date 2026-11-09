"""Opt-in real broker qualification. No service install, product dependency or key log."""
from pathlib import Path
import argparse,getpass,hashlib,json,os,platform,secrets,shutil,socket,ssl,subprocess,tempfile,time
from run_mqtt_tls import certificates,digest,frame

def free_ports():
    with socket.socket() as a,socket.socket() as b:
        a.bind(('127.0.0.1',0));b.bind(('127.0.0.1',0));return a.getsockname()[1],b.getsockname()[1]
def client_path(p,windows):
    return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip() if windows and os.name!='nt' else str(p.resolve())
def seed_session(port,version,tls,root,auth,password):
    def field(v):return len(v).to_bytes(2,'big')+v
    steps=[True,False] if version==5 else [True,False,False]
    for i,clean in enumerate(steps):
        raw=socket.create_connection(('127.0.0.1',port),timeout=10)
        if tls:
            ctx=ssl.create_default_context(cafile=root/'ca.pem');raw=ctx.wrap_socket(raw,server_hostname='localhost')
        with raw as s:
            flags=(2 if clean else 0)|(0xc0 if auth else 0);body=b'\0\x04MQTT'+bytes([5 if version==5 else 4,flags])+b'\0\0'
            if version==5:body+=b'\x05\x11\0\0\0\x1e'
            body+=field(b'xrt-session-guard')
            if auth:body+=field(b'xrt-test')+field(password.encode())
            assert len(body)<128;s.sendall(b'\x10'+bytes([len(body)])+body);kind,b=frame(s);expected=int(i==len(steps)-1)
            assert kind==0x20 and len(b)>=2 and b[0]==expected and b[1]==0,'independent retained-session seed'
            s.sendall(b'\xe0\0')
            if tls:plain=s.unwrap();plain.close()
    return True
def main():
    p=argparse.ArgumentParser();p.add_argument('--broker',type=Path,required=True);p.add_argument('--passwd',type=Path,required=True);p.add_argument('--client',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--openssl',default=shutil.which('openssl'));p.add_argument('--runtime-root',type=Path);p.add_argument('--expected-version',default='2.1.2');a=p.parse_args()
    assert a.openssl and a.broker.is_file() and a.passwd.is_file() and a.client.is_file();a.report.parent.mkdir(parents=True,exist_ok=True);runtime=a.runtime_root or a.report.parent;runtime.mkdir(parents=True,exist_ok=True)
    version=subprocess.run([str(a.broker.resolve()),'-h'],capture_output=True,text=True,timeout=15);assert 'mosquitto version '+a.expected_version in version.stdout.splitlines(),'broker version';windows_client=a.client.suffix.lower()=='.exe';rows=[];broker_runs=[]
    with tempfile.TemporaryDirectory(prefix='xmqtt-broker-',dir=runtime) as tmp:
        root=Path(tmp);public=certificates(root,a.openssl);credential=root/'client-auth.txt';passwordfile=root/'broker-passwords';user='xrt-test';password=secrets.token_hex(24);credential.write_text(user+'\n'+password+'\n')
        # Neither a password-tool failure nor its timeout may print secret argv.
        try:generated=subprocess.run([str(a.passwd.resolve()),'-b','-c',str(passwordfile),user,password],capture_output=True,timeout=15)
        except subprocess.SubprocessError:raise RuntimeError('runtime password-file generation failed') from None
        assert not generated.returncode,'runtime password-file generation failed'
        for auth in [False,True]:
            tcp,tls=free_ports();config=root/('broker-auth.conf' if auth else 'broker-anonymous.conf');log=a.report.parent/(a.report.stem+('-auth' if auth else '-anonymous')+'-broker.log')
            text=('user '+getpass.getuser()+'\n' if hasattr(os,'geteuid') and os.geteuid()==0 else '')+'persistence false\nlog_dest stdout\nlog_type all\nset_tcp_nodelay true\nmax_queued_messages 64\nmax_inflight_messages 16\n'
            text+=('allow_anonymous false\npassword_file '+passwordfile.resolve().as_posix()+'\n' if auth else 'allow_anonymous true\n')
            text+=f'listener {tcp} 127.0.0.1\nmax_topic_alias 10\nlistener {tls} 127.0.0.1\nmax_topic_alias 10\n'+f'certfile {(root/"server.pem").resolve().as_posix()}\nkeyfile {(root/"server.key").resolve().as_posix()}\n'
            config.write_text(text);started=time.monotonic();logfile=log.open('w');broker=subprocess.Popen([str(a.broker.resolve()),'-c',str(config.resolve())],stdout=logfile,stderr=subprocess.STDOUT)
            try:
                deadline=time.monotonic()+10
                while True:
                    assert broker.poll() is None,'broker failed before listening: '+str(log)
                    try:
                        with socket.create_connection(('127.0.0.1',tcp),timeout=.2):break
                    except OSError:assert time.monotonic()<deadline,'broker listening deadline';time.sleep(.05)
                cases=[(v,t,f,0) for v in [5,311] for t in [0,1] for f in range(3)]+[(v,t,0,1) for v in [5,311] for t in [0,1]]
                if auth:cases += [(v,t,f,2) for v in [5,311] for t in [0,1] for f in range(3)]
                cases += [(v,t,f,3) for v in [5,311] for t in [0,1] for f in range(3)]
                for version,t,factory,scenario in cases:
                    seeded=seed_session(tls if t else tcp,version,bool(t),root,auth,password) if scenario==3 else False
                    command=[str(a.client.resolve()),str(tls if t else tcp),str(version),str(t),str(factory),client_path(root/'ca.pem',windows_client) if t else '-',client_path(credential,windows_client) if auth else '-',str(scenario)]
                    run=subprocess.run(command,capture_output=True,text=True,timeout=90);row=dict(mqtt=version,tls=bool(t),factory=factory,scenario=scenario,authentication=auth,independent_retained_session_seed=seeded,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr);rows.append(row);print(version,t,factory,scenario,auth,run.returncode,flush=True)
                    a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n');assert not run.returncode,row
            finally:
                broker.terminate()
                try:broker.wait(10)
                except subprocess.TimeoutExpired:broker.kill();broker.wait(10)
                logfile.close();broker_runs.append(dict(pid=broker.pid,exit_code=broker.returncode,seconds=time.monotonic()-started,log_sha256=digest(log),reaped=broker.poll() is not None))
                a.report.write_text(json.dumps(dict(complete=False,cases=rows,broker_runs=broker_runs),indent=2)+'\n')
    report=dict(schema=1,complete=True,scope='Current volatile client interoperability and retained-session refusal; no session recovery/performance/native Linux deployment/production acceptance',broker_version=a.expected_version,broker_sha256=digest(a.broker),passwd_sha256=digest(a.passwd),broker_platform=platform.platform(),client_platform='Windows native client via WSL interoperability' if windows_client and os.name!='nt' else platform.platform(),client_sha256=digest(a.client),runner_sha256=digest(__file__),common_runner_sha256=digest(Path(__file__).with_name('run_mqtt_tls.py')),public_certificate_sha256=public,broker_runs=broker_runs,runtime_material_removed=not root.exists(),cases=rows)
    a.report.write_text(json.dumps(report,indent=2)+'\n');print('[PASS] real broker MQTT5/311 TCP/TLS factories, QoS, aliases, No Local, retained, Will, auth, retained-session guard and physical lifetime')
if __name__=='__main__':main()
