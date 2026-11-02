"""Independent MQTT QoS2 receiver bytes, duplicate suppression and credit."""
import argparse,hashlib,json,socket,subprocess,threading
from pathlib import Path
from run_client_wire import packet,read_packet,read_exact,take_string,string,vbi
from run_receive_wire import publish

def ack(s,kind,identifier,version,unknown=False):
    expected=(kind,identifier.to_bytes(2,'big')+(b'\x92' if unknown and version==5 else b''))
    got=read_packet(s);assert got==expected,(got,expected)
def rel(identifier,reason=0,problem=False):
    body=identifier.to_bytes(2,'big')
    if problem:body+=b'\0'+vbi(3)+b'\x1f\0\0'
    elif reason:body+=bytes([reason])
    return packet(0x62,body)
def raw(s):
    result=b''
    while True:
        try:b=s.recv(65536)
        except ConnectionResetError:return result
        if not b:return result
        result+=b

def serve(listener,version,case,errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8);t,b=read_packet(s);protocol,at=take_string(b,0);assert t==0x10 and protocol==b'MQTT' and b[at:at+4]==bytes([version,2,0,0]);at+=4
            if version==5:
                props=b'\x21\0\1' if case.startswith('window_') or case=='duplicate_window' else b'\x17\0' if case=='problem' else b''
                assert b[at:at+1+len(props)]==vbi(len(props))+props;at+=1+len(props)
            identity,at=take_string(b,at);assert identity==b'qos2-receive' and at==len(b)
            cap=3 if case=='restricted_rec' else 4 if case=='restricted_unknown' else None
            props=b'\x27'+cap.to_bytes(4,'big') if cap is not None else b''
            connack=packet(0x20,b'\0\0'+(vbi(len(props))+props if version==5 else b''))
            s.sendall(connack)
            marker=True
            if case=='early_release':s.sendall(publish(version,9,qos=2)+rel(9));ack(s,0x50,9,version);ack(s,0x70,9,version)
            elif case in ('unknown_release','restricted_unknown','partial_unknown','unknown_then_new'):
                s.sendall(rel(9))
                if case=='restricted_unknown':assert raw(s)==b'';return
                if case=='partial_unknown':
                    assert read_exact(s,1)==b'\x70';s.sendall(publish(version,9,qos=2));assert read_exact(s,3+(version==5))==bytes([3 if version==5 else 2,0,9])+(b'\x92' if version==5 else b'')
                else:
                    if case=='unknown_then_new':s.sendall(publish(version,9,qos=2))
                    ack(s,0x70,9,version,True)
                if case=='unknown_release':s.sendall(rel(9));ack(s,0x70,9,version,True)
                else:ack(s,0x50,9,version);s.sendall(rel(9));ack(s,0x70,9,version)
            elif case=='ack_fairness':
                ids=[1]+list(range(10,26));s.sendall(publish(version,1,qos=1)+publish(version,9,qos=2)+b''.join(publish(version,i,qos=1) for i in ids[1:]));seen=[]
                while True:
                    t,b=read_packet(s)
                    if t==0x50:assert b==b'\0\x09';break
                    assert t==0x40 and b==ids[len(seen)].to_bytes(2,'big');seen.append(int.from_bytes(b,'big'));assert len(seen)<=2,'QoS1 ACK backlog starved QoS2 PUBREC'
                s.sendall(rel(9));comp=False
                while len(seen)<len(ids) or not comp:
                    t,b=read_packet(s)
                    if t==0x70:assert not comp and b==b'\0\x09';comp=True
                    else:assert t==0x40 and b==ids[len(seen)].to_bytes(2,'big');seen.append(int.from_bytes(b,'big'))
            elif case in ('parallel','work_budget','backpressure'):
                ids=list(range(1,31)) if case=='backpressure' else [65535,1,42]
                s.sendall(b''.join(publish(version,i,qos=2) for i in ids))
                for i in ids:ack(s,0x50,i,version)
                ids.reverse();s.sendall(b''.join(rel(i) for i in ids))
                for i in ids:ack(s,0x70,i,version)
            elif case=='duplex':
                t,b=read_packet(s);topic,at=take_string(b,0);assert t==0x34 and topic==b'outgoing' and b[at:at+2]==b'\0\x09'
                s.sendall(publish(version,9,qos=2)+packet(0x50,b'\0\x09'))
                seen={read_packet(s),read_packet(s)};assert seen=={(0x50,b'\0\x09'),(0x62,b'\0\x09')}
                s.sendall(rel(9)+packet(0x70,b'\0\x09'));ack(s,0x70,9,version)
            elif case=='partial_outgoing':
                assert read_exact(s,1)==b'\x30';s.sendall(publish(version,9,qos=2));n=0
                for i in range(4):
                    v=read_exact(s,1)[0];n+=(v&127)<<(7*i)
                    if not v&128:break
                b=read_exact(s,n);topic,at=take_string(b,0);assert topic==b'outgoing' and b[at:]==(b'\0' if version==5 else b'')+b'p'*32768,'ACK interleaved into outgoing PUBLISH'
                ack(s,0x50,9,version);s.sendall(rel(9));ack(s,0x70,9,version)
            elif case in ('partial_qos1','partial_rec'):
                first=0x40 if case=='partial_qos1' else 0x50;firstid=10 if first==0x40 else 9
                s.sendall(publish(version,firstid,qos=1 if first==0x40 else 2));assert read_exact(s,1)==bytes([first]);s.sendall(publish(version,9 if first==0x40 else 10,qos=2 if first==0x40 else 1));assert read_exact(s,3)==bytes([2,0,firstid])
                ack(s,0x50 if first==0x40 else 0x40,9 if first==0x40 else 10,version);s.sendall(rel(9));ack(s,0x70,9,version)
            else:
                wire=publish(version,0 if case=='zero_id' else 9,qos=2)
                if case=='byte':
                    for c in wire:s.sendall(bytes([c]))
                elif case=='coalesced_duplicates':s.sendall(wire*10)
                else:s.sendall(wire)
                if case in ('object_limit','byte_limit','private_limit','restricted_rec','reentrant'):assert raw(s)==b'';return
                if case=='zero_id':
                    if version==5:assert read_packet(s)==(0xe0,b'\x81')
                    assert raw(s)==b'';return
                if case=='close_partial_rec':assert read_exact(s,1)==b'\x50';return
                ack(s,0x50,9,version)
                if case in ('graceful_timeout','graceful_cancel'):assert raw(s)==b'';return
                if case=='queue_limit':s.sendall(publish(version,10,qos=2));assert raw(s)==b'';return
                if case in ('window_qos1','window_qos2','id_conflict','problem'):
                    s.sendall(rel(9,problem=True) if case=='problem' else publish(version,9 if case=='id_conflict' else 10,qos=1 if case in ('window_qos1','id_conflict') else 2))
                    assert read_packet(s)==(0xe0,b'\x93' if case.startswith('window_') else b'\x82');assert raw(s)==b'';return
                if case in ('duplicate_publish','duplicate_window'):
                    for dup in (True,False):s.sendall(publish(version,9,duplicate=dup,qos=2));ack(s,0x50,9,version)
                if case=='graceful':
                    s.settimeout(.04)
                    try:extra=s.recv(1);raise AssertionError(('final packet before PUBREL',extra))
                    except socket.timeout:pass
                    s.settimeout(8)
                wire=rel(9,0x92 if case=='negative_release' else 0)
                if case=='byte':
                    for c in wire:s.sendall(bytes([c]))
                else:s.sendall(wire)
                if case=='close_partial_comp':assert read_exact(s,1)==b'\x70';return
                if case=='coalesced_duplicates':
                    while True:
                        t,b=read_packet(s);assert b==b'\0\x09' and t in (0x50,0x70)
                        if t==0x70:break
                else:ack(s,0x70,9,version)
                if case=='duplicate_release':s.sendall(rel(9));ack(s,0x70,9,version,True)
                if case in ('reuse_id','duplicate_window'):
                    identifier=10 if case=='duplicate_window' else 9;s.sendall(publish(version,identifier,duplicate=True,qos=2));ack(s,0x50,identifier,version);s.sendall(rel(identifier));ack(s,0x70,identifier,version)
                if case=='graceful':assert read_packet(s)==(0xe0,b'');marker=False
            if marker:s.sendall(publish(version,qos=0))
            assert raw(s)==b''
    except BaseException as e:errors.append(repr(e))

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--probe',type=Path,required=True);ap.add_argument('--report',type=Path,required=True);ap.add_argument('--case');a=ap.parse_args();rows=[]
    common=[('ack_fairness',4,19),('normal',4,2),('byte',4,2),('duplicate_publish',4,2),('duplicate_release',4,2),('coalesced_duplicates',4,2),('reuse_id',4,3),('parallel',4,4),('unknown_release',4,1),('early_release',4,2),('partial_unknown',4,2),('unknown_then_new',4,2),('partial_rec',4,3),('partial_qos1',4,3),('close_partial_rec',5,1),('close_partial_comp',5,1),('partial_outgoing',4,2),('duplex',4,2),('backpressure',4,31),('work_budget',4,4),('reentrant',4,1),('graceful',0,1),('graceful_timeout',2,1),('graceful_cancel',3,1),('queue_limit',7,1),('object_limit',7,0),('byte_limit',7,0),('private_limit',7,0),('zero_id',6,0)]
    for version in (4,5):
        extra=[('window_qos1',6,1),('window_qos2',6,1),('duplicate_window',4,3),('id_conflict',6,1),('restricted_rec',13,1),('restricted_unknown',13,0),('problem',6,1),('negative_release',4,2)] if version==5 else []
        for case,close,received in common+extra:
            if a.case and case!=a.case:continue
            errors=[]
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(10);thread=threading.Thread(target=serve,args=(listener,version,case,errors));thread.start();r=subprocess.run([str(a.probe.resolve()),str(listener.getsockname()[1]),str(version),case,str(close),str(received)],capture_output=True,text=True,timeout=12);thread.join(10)
                row=dict(version=version,case=case,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,peer_errors=errors,peer_alive=thread.is_alive());rows.append(row);a.report.write_text(json.dumps(dict(complete=False,cases=rows),indent=2)+'\n')
                if r.returncode or errors or thread.is_alive():print(json.dumps(row));raise SystemExit(1)
                print('PASS',version,case,flush=True)
    a.report.write_text(json.dumps(dict(complete=True,probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),cases=rows),indent=2)+'\n')
if __name__=='__main__':main()
