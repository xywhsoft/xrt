"""Independent bounded two-phase sender, window and retained metadata fixtures."""
import argparse,hashlib,json,socket,subprocess,threading,time
from pathlib import Path
from run_client_wire import packet,read_packet,read_exact,take_string,string,vbi

def ack(kind,identifier,version,reason=0,properties=False):
    props=b'\x1f'+string(b'received' if kind==0x50 else b'completed')+b'\x26'+string(b'k')+string(b'v') if properties else b''
    return packet(kind,identifier+(bytes([reason])+vbi(len(props))+props if props else bytes([reason]) if reason else b''))
def raw(s):
    b=b''
    while True:
        try:x=s.recv(65536)
        except ConnectionResetError:break
        if not x:return b
        b+=x
    return b
def publish(s,version,expected=7):
    t,b=read_packet(s);assert t==0x34,(t,len(b));topic,at=take_string(b,0);assert topic==b'qos/two';identifier=b[at:at+2];at+=2;assert int.from_bytes(identifier,'big')==expected
    if version==5:assert b[at]==0;at+=1
    assert b[at:]==b'';return identifier
def release(s,identifier):assert read_packet(s)==(0x62,identifier)
def serve(listener,version,case,errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8);t,b=read_packet(s);assert t==0x10;protocol,at=take_string(b,0);assert protocol==b'MQTT' and b[at:at+4]==bytes([version,2,0,0]);at+=4
            if version==5:
                pi=b'\x17\0' if case=='problem' else b'';assert b[at:at+1+len(pi)]==vbi(len(pi))+pi;at+=1+len(pi)
            identity,at=take_string(b,at);assert identity==b'qos2-probe' and at==len(b)
            props=b'\x21\0\1' if case in ('window','negative_window') else b'';s.sendall(packet(0x20,b'\0\0'+(vbi(len(props))+props if version==5 else b'')))
            identifier=b'\0\7'
            if case=='partial_publish':
                assert read_exact(s,1)==b'\x34';s.sendall(ack(0x50,identifier,version));raw(s);return
            identifier=publish(s,version)
            if case=='timeout_rec':raw(s);return
            if case in ('wrong_id','wrong_kind','problem'):
                s.sendall(ack(0x40 if case=='wrong_kind' else 0x50,b'\0\x55' if case=='wrong_id' else identifier,version,properties=case=='problem'));raw(s);return
            if case=='premature_comp':s.sendall(ack(0x70,identifier,version))
            s.sendall(ack(0x50,identifier,version,0x87 if case in ('reject_rec','negative_window') else 0,case=='props'))
            if case=='reject_rec' or case=='metadata_rec':assert raw(s)==b'';return
            if case=='negative_window':
                second=publish(s,version,8);s.sendall(ack(0x50,second,version));release(s,second);s.sendall(ack(0x70,second,version));assert raw(s)==b'';return
            if case=='window':
                seen=set()
                for _ in range(2):
                    t,b=read_packet(s)
                    if t==0x62:assert b==identifier and 'rel' not in seen;seen.add('rel')
                    else:
                        assert t==0x30,(t,b);topic,at=take_string(b,0);assert topic==b'qos/two' and b[at:]==b'\0' and 'zero' not in seen;seen.add('zero')
                assert seen=={'rel','zero'}
                s.settimeout(.04)
                try:extra=s.recv(1);raise AssertionError(('PUBLISH before PUBCOMP',extra))
                except socket.timeout:pass
                s.settimeout(8);s.sendall(ack(0x70,identifier,version));second=publish(s,version,8);s.sendall(ack(0x50,second,version));release(s,second);s.sendall(ack(0x70,second,version));assert raw(s)==b'';return
            release(s,identifier)
            if case in ('timeout_comp','cancel_comp','close_comp'):
                if case!='timeout_comp':s.sendall(packet(0x30,string(b'm')+(b'\0' if version==5 else b'')+b'x'))
                assert raw(s)==b'';return
            if case=='changed_negative':s.sendall(ack(0x50,identifier,version,0x87));assert raw(s)==b'';return
            if case=='duplicate_rec':s.sendall(ack(0x50,identifier,version));release(s,identifier)
            if case=='partial_repeat':
                s.sendall(ack(0x50,identifier,version));assert read_exact(s,1)==b'\x62';s.sendall(ack(0x70,identifier,version));assert read_exact(s,3)==b'\x02'+identifier
            if case=='graceful':
                s.settimeout(.04)
                try:extra=s.recv(1);raise AssertionError(('DISCONNECT before PUBCOMP',extra))
                except socket.timeout:pass
                s.settimeout(8)
            s.sendall(ack(0x70,identifier,version,0x92 if case=='reject_comp' else 0,case=='props'))
            if case=='late_comp':
                s.sendall(ack(0x70,identifier,version));identifier=publish(s,version);s.sendall(ack(0x70,identifier,version)+ack(0x50,identifier,version));release(s,identifier);s.sendall(ack(0x70,identifier,version))
            if case in ('graceful','partial_repeat'):assert read_packet(s)==(0xe0,b'')
            assert raw(s)==b''
    except BaseException as e:errors.append(repr(e))
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--probe',type=Path,required=True);ap.add_argument('--report',type=Path,required=True);a=ap.parse_args();rows=[]
    common=['normal','byte','duplicate_rec','partial_repeat','late_comp','premature_comp','wrong_id','wrong_kind','timeout_rec','timeout_comp','cancel_comp','close_comp','graceful','partial_publish']
    for version in (4,5):
        for case in common+(['props','reject_rec','reject_comp','changed_negative','window','negative_window','metadata_rec','metadata_comp','problem'] if version==5 else []):
            errors=[]
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(10);thread=threading.Thread(target=serve,args=(listener,version,case,errors));thread.start();r=subprocess.run([str(a.probe.resolve()),str(listener.getsockname()[1]),str(version),case],text=True,capture_output=True,timeout=12);thread.join(10)
                row=dict(version=version,case=case,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,peer_errors=errors,peer_alive=thread.is_alive());rows.append(row);a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n')
                if r.returncode or errors or thread.is_alive():print(json.dumps(row));raise SystemExit(1)
                print('PASS',version,case,flush=True)
    a.report.write_text(json.dumps(dict(complete=True,probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),cases=rows),indent=2)+'\n')
if __name__=='__main__':main()
