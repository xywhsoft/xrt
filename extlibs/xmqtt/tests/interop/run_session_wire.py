"""Independent literal TCP peer for the caller-driven session state module."""
from pathlib import Path
import argparse,json,platform,socket,subprocess,threading
from run_mqtt_tls import digest,exact,frame


def publish(version,qos=2,duplicate=False,incoming=False):
    topic=b'wire/rx' if incoming else b'wire/session'
    identifier=(321 if incoming else 31).to_bytes(2,'big')
    body=len(topic).to_bytes(2,'big')+topic+identifier+(b'\0' if version==5 else b'')+(b'r\0x' if incoming else b'x\0z')
    return bytes([0x30+(qos<<1)+(8 if duplicate else 0),len(body)])+body


def send(connection,data):
    for byte in data:connection.sendall(bytes([byte]))


def peer(listener,version,scenario,result):
    try:
        for round in range(2):
            connection,_=listener.accept()
            with connection as s:
                s.settimeout(12)
                k,b=frame(s)
                expected=b'\0\x04MQTT'+bytes([5 if version==5 else 4,0])+b'\0\0'+(b'\x05\x11\0\0\0\x3c' if version==5 else b'')+b'\0\x0csession-wire'
                assert k==0x10 and b==expected,'literal clean-false CONNECT'
                present=int(bool(round) and scenario!=5)
                send(s,bytes([0x20,3 if version==5 else 2,present,0])+(b'\0' if version==5 else b''))
                if round and scenario==5:
                    if version==5:assert frame(s)==(0xe0,b'')
                    assert s.recv(1)==b'','missing state emitted no replay'
                    continue
                if scenario==4:
                    send(s,publish(version,incoming=True,duplicate=bool(round)))
                    assert frame(s)==(0x50,b'\x01A')
                    if round:
                        for repeat in range(2):
                            send(s,b'\x62\x02\x01A')
                            assert frame(s)==(0x70,b'\x01A'+(b'\x92' if repeat and version==5 else b''))
                elif not round:
                    original=publish(version,qos=1 if scenario in [0,5] else 2)
                    if scenario==2:assert exact(s,7)==original[:7],'partial PUBLISH prefix'
                    else:
                        assert exact(s,len(original))==original,'first PUBLISH exact bytes'
                        if scenario in [1,3,6]:
                            send(s,b'\x50\x02\0\x1f')
                            if scenario==3:assert exact(s,1)==b'\x62','partial PUBREL prefix'
                            if scenario==6:
                                assert frame(s)==(0x62,b'\0\x1f'),'first full PUBREL'
                                send(s,b'\x50\x02\0\x1f')
                                assert exact(s,1)==b'\x62','partial repeated PUBREL after stale token rejection'
                else:
                    if scenario in [1,3,6]:assert frame(s)==(0x62,b'\0\x1f'),'PUBREC means only PUBREL replay'
                    else:
                        expected=publish(version,qos=1 if scenario==0 else 2,duplicate=True)
                        assert exact(s,len(expected))==expected,'DUP/original-id binary replay'
                        if scenario==2:send(s,b'\x50\x02\0\x1f');assert frame(s)==(0x62,b'\0\x1f')
                    send(s,b'\x40\x02\0\x1f' if scenario==0 else b'\x70\x02\0\x1f')
                if round:assert frame(s)==(0xe0,b'')
                elif scenario!=4:assert s.recv(1)==b'','no extra frame after cut'
        result['two_connections']=True
    except BaseException as error:result.update(failed=True,error=repr(error))
    finally:listener.close()


def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args();rows=[]
    a.report.parent.mkdir(parents=True,exist_ok=True)
    for version in [5,311]:
        for scenario in range(7):
            listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(2);listener.settimeout(15);port=listener.getsockname()[1];result={}
            thread=threading.Thread(target=peer,args=(listener,version,scenario,result));thread.start()
            run=subprocess.run([str(a.probe.resolve()),str(port),str(version),str(scenario)],capture_output=True,text=True,timeout=40);thread.join(20)
            row=dict(version=version,scenario=scenario,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,peer=result);rows.append(row);print(version,scenario,run.returncode,flush=True)
            a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n');assert not thread.is_alive() and not run.returncode and not result.get('failed') and result.get('two_connections'),row
    a.report.write_text(json.dumps(dict(complete=True,scope='Session state over a test-only public Core socket driver; existing Client integration/durable recovery/broker/performance acceptance not inferred',platform=platform.platform(),runner_sha256=digest(__file__),common_runner_sha256=digest(Path(__file__).with_name('run_mqtt_tls.py')),probe_sha256=digest(a.probe),cases=rows),indent=2)+'\n');print('[PASS]14 two-connection session TCP/literal replay, dedup, stale selection and missing-state cases')


if __name__=='__main__':main()
