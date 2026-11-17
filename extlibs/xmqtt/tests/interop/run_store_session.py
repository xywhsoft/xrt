"""Literal Python MQTT peer for actual Client attachment to recovered State.
No native codec, state, Session, image or broker code is imported by this peer.
"""
from pathlib import Path
import argparse,hashlib,json,platform,socket,subprocess,threading
from run_mqtt_tls import frame

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def send(s,b,fragment):
    if fragment:
        for octet in b:s.sendall(bytes([octet]))
    else:s.sendall(b)
def wire(first,body):assert len(body)<128;return bytes([first,len(body)])+body
def publish(version,identifier,duplicate=False,incoming=False):
    properties=b'\x26\0\0\0\x04meta\x09\0\x03a\0b\x03\0\x09type/utf8\x01\x01'
    if incoming:properties+=b'\x0b\x03'
    return wire(0x30+(4 if incoming else 2)+(8 if duplicate else 0),b'\0\x0bstore/topic'+identifier.to_bytes(2,'big')+(bytes([len(properties)])+properties if version==5 else b'')+b'x\0z')
def peer(listener,version,fragment,result,done):
    try:
        s,_=listener.accept()
        with s:
            s.settimeout(15);k,b=frame(s);expected=b'\0\x04MQTT'+bytes([5 if version==5 else 4,0])+b'\0\0'+(b'\x05\x11\0\0\0\x3c' if version==5 else b'')+b'\0\x0cdelta-client'
            assert (k,b)==(0x10,expected),(k,b,expected)
            send(s,b'\x20\x03\x01\0\0' if version==5 else b'\x20\x02\x01\0',fragment)
            actual=[]
            # The native Client owns scheduling. Verify every exact obligation
            # once; passive SessionNext's PUBREL priority is tested separately.
            seen=set()
            for _ in range(3):
                k,b=frame(s);identifier=int.from_bytes(b[:2] if k==0x62 else b[13:15],'big');assert identifier in {7,8,9} and identifier not in seen,(k,b);seen.add(identifier);actual.append([k,b.hex()])
                if identifier==8:assert (k,b)==(0x62,b'\0\x08');send(s,b'\x70\x02\0\x08',fragment)
                else:assert wire(k,b)==publish(version,identifier,identifier==7),(k,b);send(s,b'\x40\x02'+identifier.to_bytes(2,'big'),fragment)
            # Accepted source deliveries are still in the durable Inbox. Replay
            # of PUBLISH18 must be deduplicated, with normal protocol handshakes.
            send(s,publish(version,18,True,True),fragment);assert frame(s)==(0x50,b'\0\x12')
            for identifier in [17,18]:
                send(s,b'\x62\x02'+identifier.to_bytes(2,'big'),fragment);assert frame(s)==(0x70,identifier.to_bytes(2,'big'))
            send(s,b'\x62\x02\0\x11',fragment);assert frame(s)==(0x70,b'\0\x11'+(b'\x92' if version==5 else b''))
            result.update(exact_outgoing=actual,accepted_receive_dedup=True,unknown_second_pubrel=True);done.set()
            assert frame(s)==(0xe0,b'');assert s.recv(1)==b'';result['physical_eof']=True
    except BaseException as error:result.update(failed=True,error=repr(error));done.set()
    finally:listener.close()
def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',required=True,type=Path);p.add_argument('--report',required=True,type=Path);a=p.parse_args();rows=[]
    a.report.parent.mkdir(parents=True,exist_ok=True)
    for version in [5,311]:
        for fragment in [False,True]:
            listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15);port=listener.getsockname()[1];result={};done=threading.Event();thread=threading.Thread(target=peer,args=(listener,version,fragment,result,done));thread.start();run=None;stdout=stderr='';failure=None
            try:
                run=subprocess.Popen([str(a.probe.resolve()),str(port),str(version),str(int(fragment))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                assert done.wait(25),'bounded independent peer';stdout,stderr=run.communicate('done\n',timeout=20)
            except BaseException as error:failure=repr(error)
            finally:
                if run:
                    if run.poll() is None:run.kill();stdout,stderr=run.communicate(timeout=10)
                    run.wait(timeout=10)
                    for stream in [run.stdin,run.stdout,run.stderr]:
                        if stream and not stream.closed:stream.close()
                listener.close();thread.join(20)
            row=dict(version=version,fragmented=fragment,exit_code=run.returncode if run else None,stdout=stdout,stderr=stderr,peer=result,failure=failure,child_reaped=bool(run and run.poll() is not None),peer_reaped=not thread.is_alive());rows.append(row);a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n');print(version,fragment,row['exit_code'],flush=True);assert row['child_reaped'] and row['peer_reaped'] and not failure and not row['exit_code'] and not result.get('failed') and result.get('physical_eof'),row
    a.report.write_text(json.dumps(dict(complete=True,scope='Passive committed import attached to actual Client/Engine; no live durable ACK/lease/commit driver inferred',platform=platform.platform(),runner_sha256=digest(__file__),common_sha256=digest(Path(__file__).with_name('run_mqtt_tls.py')),probe_sha256=digest(a.probe),cases=rows),indent=2)+'\n')
if __name__=='__main__':main()
