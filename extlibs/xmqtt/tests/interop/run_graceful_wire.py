"""Independent final DISCONNECT ordering, immutable properties and local outcomes."""
import argparse,hashlib,json,socket,struct,subprocess,threading,time
from pathlib import Path
from run_client_wire import packet,read_packet,take_string,string,vbi

def final(version,case):
    reason=4 if case in ('reason4','cap3') else 0;props=b''
    if case=='snapshot':props=b'\x1f'+string(b'before')+b'\x26'+string(b'k')+string(b'v')
    if case=='expiry_zero':props=b'\x11'+b'\0'*4
    if case=='abort_prefix':props=b'\x1f'+string(b'p'*8192)
    if case=='timeout_prefix':props=(b'\x26'+string(b'n')+string(b'p'*60000))*20
    return packet(0xe0,(bytes([reason])+vbi(len(props))+props) if props else bytes([reason]) if reason else b'')
def raw(s):
    chunks=[]
    while True:
        try:b=s.recv(65536)
        except ConnectionResetError:break
        if not b:break
        chunks.append(b)
    return b''.join(chunks)
def serve(listener,version,case,errors,seen):
    accepted=False
    try:
        with listener.accept()[0] as s:
            accepted=True;s.settimeout(8)
            if case=='before_ready':raw(s);return
            t,b=read_packet(s);assert t==0x10
            protocol,at=take_string(b,0);assert protocol==b'MQTT' and b[at]==version and b[at+1]==2
            at+=4
            if version==5:
                props=b'\x11'+(60).to_bytes(4,'big') if case=='expiry_zero' else b'';assert b[at:at+1+len(props)]==vbi(len(props))+props;at+=1+len(props)
            identity,at=take_string(b,at);assert identity==b'grace-probe' and at==len(b)
            caps=b'\x27'+(2 if case in ('cap','cap2') else 3).to_bytes(4,'big') if case in ('cap','cap2','cap3') else b''
            connack=packet(0x20,b'\0\0'+(vbi(len(caps))+caps if version==5 else b''))
            incoming=packet(0x32,string(b't')+b'\0\x0b'+(b'\0' if version==5 else b'')+b'i') if case=='receive_ack' else b''
            s.sendall(connack+incoming)
            if case=='receive_ack':assert read_packet(s)==(0x40,b'\0\x0b')
            if case in ('objects','outgoing','cap','expiry_bad','timeout_zero'):assert raw(s)==b'';return
            if case in ('timeout_ack','cancel_ack','abort_ack','requests'):
                b=raw(s);expected=packet(0x32,string(b't')+b'\0\1'+(b'\0' if version==5 else b''));assert expected.startswith(b),(case,len(b));return
            if case in ('drain0','drain1','partial_pub'):
                t,b=read_packet(s);assert t==(0x30 if case=='drain0' else 0x32);topic,at=take_string(b,0);assert topic==b't'
                if case!='drain0':identifier=b[at:at+2];at+=2
                if version==5:assert b[at]==0;at+=1
                assert b[at:]==(b'x'*32768 if case=='partial_pub' else b'')
                if case!='drain0':s.sendall(packet(0x40,identifier))
            if case in ('abort_prefix','timeout_prefix'):
                b=raw(s);assert final(version,case).startswith(b);assert len(b)<len(final(version,case));seen['prefix']=len(b);return
            if case=='abort_notify':assert raw(s)==b'';return
            if case=='slow':time.sleep(.2)
            t,b=read_packet(s);expected=final(version,case);assert packet(t,b)==expected,(case,t,len(b))
            seen['final_hex']=expected[:64].hex();assert s.recv(1)==b'','packet after DISCONNECT or missing client EOF'
    except TimeoutError as e:
        if case!='before_ready' or accepted:errors.append(repr(e))
    except BaseException as e:errors.append(repr(e))
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--probe',required=True);ap.add_argument('--report',required=True);args=ap.parse_args();rows=[]
    common=['normal','timeout_zero','byte','receive_ack','drain0','drain1','partial_pub','timeout_ack','cancel_ack','abort_ack','requests','objects','outgoing','duplicate','before_ready','abort_notify','slow']
    for version in (4,5):
        for case in common+(['reason4','snapshot','expiry_zero','expiry_bad','cap','cap2','cap3','abort_prefix','timeout_prefix'] if version==5 else []):
            errors=[];seen={}
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(.5 if case=='before_ready' else 10);thread=threading.Thread(target=serve,args=(listener,version,case,errors,seen));thread.start()
                command=[args.probe,str(listener.getsockname()[1]),str(version),case];r=subprocess.run(command,capture_output=True,text=True,timeout=12);thread.join(10)
                row=dict(version=version,case=case,command=command,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,peer_errors=errors,seen=seen);rows.append(row)
                if r.returncode or errors or thread.is_alive():print(json.dumps(row));raise SystemExit(1)
                print('PASS',version,case,flush=True)
    Path(args.report).write_text(json.dumps(dict(complete=True,probe_sha256=hashlib.sha256(Path(args.probe).read_bytes()).hexdigest(),cases=rows),indent=2)+'\n')
if __name__=='__main__':main()
