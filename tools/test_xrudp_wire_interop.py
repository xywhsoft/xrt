"""Independent XRUDP001 Noise/wire peer versus the native endpoint (test-only).

Packet, frame, IK, HKDF, replay history and flow control are implemented here;
no product codecs, private headers or bindings are called by this peer. The
cryptography package is an explicit development tool, never a runtime library.
The supplied SDK provides an immutable, committed XRT core while other tasks
may have uncommitted core changes. All traffic is bounded IPv4/IPv6 loopback.
"""
from pathlib import Path
import argparse
import hashlib
import hmac
import json
import os
import queue
import socket
import struct
import subprocess
import sys
import threading
import time

from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

ROOT=Path(__file__).resolve().parents[1]
MAGIC=b'XRUDP001'
SCOPE=bytes([17])*16
CONTEXT=bytes([22])*32


def digest(data): return hashlib.sha256(data).digest()
def public(key): return key.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw)
def nonce(number): return bytes(4)+struct.pack('<Q',number)
def hkdf(chain,material):
    extract=hmac.digest(chain,material,'sha256')
    first=hmac.digest(extract,b'\x01','sha256')
    return first,hmac.digest(extract,first+b'\x02','sha256')
def rekey(key): return ChaCha20Poly1305(key).encrypt(nonce(2**64-1),bytes(32),b'')[:32]
def header(kind,epoch,cid,number,size):
    assert len(cid)==16 and any(cid) and 0<number<2**64
    return struct.pack('!8sBBHI16sQ8s',MAGIC,kind,0,size,epoch,cid,number,bytes(8))
def packet(kind,cid,number,body): return header(kind,0,cid,number,len(body))+body
def decode(data):
    assert 48<=len(data)<=1200,'wire size'
    magic,kind,flags,size,epoch,cid,pn,reserved=struct.unpack('!8sBBHI16sQ8s',data[:48])
    assert magic==MAGIC and kind in range(1,6) and not flags and not any(reserved)
    assert size==len(data)-48 and any(cid) and pn
    assert (epoch!=0)==(kind in (4,5))
    assert kind not in (4,5) or pn<2**64-1
    return kind,epoch,cid,pn,data[48:]
def frame(kind,body=b'',flags=0): return struct.pack('!BBH',kind,flags,len(body))+body
def frames(data):
    result=[]; offset=0
    while offset<len(data):
        assert len(result)<64 and len(data)-offset>=4
        kind,flags,size=struct.unpack_from('!BBH',data,offset); offset+=4
        assert kind<=16 and offset+size<=len(data)
        body=data[offset:offset+size]; offset+=size
        assert flags==0 or (kind in (3,8) and flags==1)
        result.append((kind,flags,body))
    return result


class Noise:
    """Fixed IK roles, from the public fixed-profile specification."""
    def __init__(self,prologue,remote,responder=False):
        name=b'Noise_IK_25519_ChaChaPoly_SHA256'
        self.hash=name.ljust(32,b'\0') if len(name)<=32 else digest(name)
        self.chain=self.hash; self.mix_hash(prologue)
        self.static=X25519PrivateKey.from_private_bytes(bytes(range(91,123) if responder else range(33,65)))
        self.mix_hash(public(self.static) if responder else remote)
        self.ephemeral=X25519PrivateKey.generate(); self.remote=remote
        self.key=None; self.number=0
    def mix_hash(self,value): self.hash=digest(self.hash+value)
    def mix_key(self,private,remote):
        self.chain,self.key=hkdf(self.chain,private.exchange(X25519PublicKey.from_public_bytes(remote)))
        self.number=0
    def encrypt(self,value):
        out=ChaCha20Poly1305(self.key).encrypt(nonce(self.number),value,self.hash)
        self.number+=1; self.mix_hash(out); return out
    def decrypt(self,value):
        out=ChaCha20Poly1305(self.key).decrypt(nonce(self.number),value,self.hash)
        self.number+=1; self.mix_hash(value); return out
    def first(self,parameters):
        out=public(self.ephemeral); self.mix_hash(out); self.mix_key(self.ephemeral,self.remote)
        out+=self.encrypt(public(self.static)); self.mix_key(self.static,self.remote)
        return out+self.encrypt(parameters)
    def second(self,value):
        assert len(value)==80
        ephemeral=value[:32]; self.mix_hash(ephemeral)
        self.mix_key(self.ephemeral,ephemeral); self.mix_key(self.static,ephemeral)
        parameters=self.decrypt(value[32:]); assert len(parameters)==32
        self.send,self.receive=hkdf(self.chain,b''); return struct.unpack('!QIIIIII',parameters)
    def read_first(self,value):
        assert len(value)==128
        self.remote_ephemeral=value[:32]; self.mix_hash(self.remote_ephemeral)
        self.mix_key(self.static,self.remote_ephemeral)
        remote=self.decrypt(value[32:80]); assert remote==self.remote,'independent responder pin'
        self.mix_key(self.static,remote)
        parameters=self.decrypt(value[80:]); assert len(parameters)==32
        return struct.unpack('!QIIIIII',parameters)
    def write_second(self,parameters):
        out=public(self.ephemeral); self.mix_hash(out)
        self.mix_key(self.ephemeral,self.remote_ephemeral); self.mix_key(self.ephemeral,self.remote)
        out+=self.encrypt(parameters)
        self.receive,self.send=hkdf(self.chain,b''); return out


class Peer:
    def __init__(self,sock,address,responder=False,typed=False):
        self.sock,self.address=sock,address; self.cid=os.urandom(16); self.server=None
        self.responder=responder; self.typed=typed
        self.send_number=1; self.send_epoch=1; self.receive_epoch=1
        self.send_key=None; self.receive_keys={}; self.seen=set(); self.acked=set(); self.ack_omitted=set()
        self.received={}; self.final=None; self.credit=0; self.stream_credit=0
        self.invalid_injected=0; self.replay_injected=0; self.duplicates=0; self.rotations=0
        self.retransmits=0; self.muted_acks=0; self.mute_until=0
        self.extra_receive={i:{} for i in range(2,6)}; self.extra_final={}; self.extra_credit={}
        self.remote_opens={}; self.datagrams={}; self.sent_high={i:0 for i in range(1,6)}
    def parameters(self): return struct.pack('!QIIIIII',1024,512,3 if self.typed else 1,10000,25000,1200,8192 if self.typed else 0)
    def negotiated(self,value):
        self.credit,self.stream_credit=value[:2]; self.initial_credit,self.initial_stream_credit=value[:2]
        self.extra_credit={i:self.stream_credit for i in range(2,6)}
    def raw(self,data): self.sock.sendto(data,self.address)
    def receive(self):
        data,source=self.sock.recvfrom(2048); assert source==self.address
        return data
    def exchange(self,out,expected):
        end=time.monotonic()+4; next_send=0
        while time.monotonic()<end:
            if time.monotonic()>=next_send: self.raw(out); next_send=time.monotonic()+.25
            try:
                while True:
                    data=self.receive(); fields=decode(data)
                    if fields[0]==expected: return fields,data
            except socket.timeout: pass
        raise AssertionError('handshake flight deadline')
    def handshake(self):
        if self.responder: return self.accept()
        remote=public(X25519PrivateKey.from_private_bytes(bytes(range(91,123))))
        hello=self.cid+remote+os.urandom(16)+CONTEXT
        first=packet(1,SCOPE,1,hello)
        # Malformed unauthenticated input must not poison the listening peer.
        self.raw(b'XRUDP001'+bytes(9))
        (kind,epoch,cid,pn,body),_=self.exchange(first,2)
        assert cid==self.cid and pn==1 and len(body)==88
        self.server=body[:16]; cookie=body[48:]
        assert body[16:48]==digest(first) and self.server==digest(b'XRUDP001/server-id'+digest(first)+cookie)[:16]
        noise=Noise(b'XRUDP001/IK/prologue'+SCOPE+hello+self.server,remote)
        parameters=self.parameters()
        flight=packet(3,self.server,1,hello+cookie+noise.first(parameters))
        assert len(flight)==312
        (_,_,cid,pn,body),_=self.exchange(flight,3); assert cid==self.cid and pn==2
        self.negotiated(noise.second(body))
        self.send_key=noise.send; self.receive_keys[1]=noise.receive
        proof=self.protect(frame(1)+frame(12,struct.pack('!I',1)))
        assert self.send_number==2
        (_,epoch,cid,pn,_),data=self.exchange(proof,4)
        assert cid==self.cid and epoch==1 and pn==1
        payload=self.open(data)
        assert payload==[(1,0,b''),(12,0,struct.pack('!I',1)),(2,0,struct.pack('!IHHQQ',0,1,0,1,1))]
    def accept(self):
        end=time.monotonic()+4; first=None
        while time.monotonic()<end:
            try:
                wire=self.receive(); kind,_,scope,pn,hello=decode(wire)
                if kind==1: first=wire; break
            except socket.timeout: pass
        assert first is not None and scope==SCOPE and pn==1 and len(hello)==96
        remote=public(X25519PrivateKey.from_private_bytes(bytes(range(33,65))))
        assert any(hello[:16]) and hello[:16]!=SCOPE and any(hello[48:64]) and hello[64:]==CONTEXT
        assert hello[16:48]==public(X25519PrivateKey.from_private_bytes(bytes(range(91,123))))
        issued=struct.pack('!Q',int(time.monotonic()*1000000)); secret=os.urandom(32)
        family=4 if self.sock.family==socket.AF_INET else 6
        ip=socket.inet_pton(self.sock.family,self.address[0]).ljust(16,b'\0')
        canonical=struct.pack('!BHI',family,self.address[1],self.address[3] if family==6 else 0)+ip
        mac_input=b'XRUDP001/address-cookie'+SCOPE+digest(first)+canonical+issued
        cookie=issued+hmac.digest(secret,mac_input,'sha256')
        self.cid=digest(b'XRUDP001/server-id'+digest(first)+cookie)[:16]; self.server=hello[:16]
        assert any(self.cid) and self.cid not in (self.server,SCOPE)
        retry=packet(2,self.server,1,self.cid+digest(first)+cookie); self.raw(retry)
        flight=None
        while time.monotonic()<end:
            try:
                wire=self.receive(); kind,_,cid,pn,body=decode(wire)
                if kind==1:
                    assert wire==first; self.raw(retry)
                elif kind==3:
                    assert cid==self.cid and pn==1 and len(body)==264 and body[:96]==hello
                    token=body[96:136]; assert token[:8]==issued
                    assert hmac.compare_digest(token[8:],hmac.digest(secret,mac_input,'sha256'))
                    assert 0<=int(time.monotonic()*1000000)-struct.unpack('!Q',issued)[0]<=30000000
                    flight=body[136:]; break
            except socket.timeout: pass
        assert flight is not None,'native first flight deadline'
        noise=Noise(b'XRUDP001/IK/prologue'+SCOPE+hello+self.cid,remote,responder=True)
        self.negotiated(noise.read_first(flight))
        second=noise.write_second(self.parameters())
        self.send_key=noise.send; self.receive_keys[1]=noise.receive
        (_,epoch,cid,pn,_),wire=self.exchange(packet(3,self.server,2,second),4)
        assert epoch==1 and cid==self.cid and pn==1
        assert self.open(wire)==[(1,0,b''),(12,0,struct.pack('!I',1))]
        self.raw(self.protect(frame(1)+frame(12,struct.pack('!I',1))+frame(2,struct.pack('!IHHQQ',0,1,0,1,1))))
    def protect(self,payload,kind=4):
        if self.send_number==8:
            self.send_key=rekey(self.send_key); self.send_epoch+=1
        assert kind in (4,5)
        if kind==5: assert 84<=len(payload)+64<=128
        head=header(kind,self.send_epoch,self.server,self.send_number,len(payload)+16)
        wire=head+ChaCha20Poly1305(self.send_key).encrypt(nonce(self.send_number),payload,head)
        self.send_number+=1; return wire
    def open(self,wire):
        kind,epoch,cid,number,body=decode(wire); assert kind in (4,5) and cid==self.cid
        key=self.receive_keys.get(epoch)
        if key is None and epoch<self.receive_epoch: return []
        if key is None:
            assert epoch==self.receive_epoch+1,'skipped unauthenticated key epoch'
            key=rekey(self.receive_keys[self.receive_epoch])
        payload=ChaCha20Poly1305(key).decrypt(nonce(number),body,wire[:48])
        parsed=frames(payload)
        if epoch not in self.receive_keys:
            self.receive_keys[epoch]=key; self.receive_epoch=epoch; self.rotations+=1
            self.receive_keys={e:k for e,k in self.receive_keys.items() if e>=epoch-1}
        if number in self.seen: self.duplicates+=1; return []
        self.seen.add(number); return parsed
    def ack(self):
        ranges=[]
        for pn in sorted(self.seen-self.ack_omitted,reverse=True):
            if ranges and pn==ranges[-1][0]-1: ranges[-1][0]=pn
            else: ranges.append([pn,pn])
        ranges=ranges[:32]
        body=struct.pack('!IHH',0,len(ranges),0)+b''.join(struct.pack('!QQ',*r) for r in ranges)
        return frame(2,body)
    def input(self,wire):
        before=self.receive_epoch; parsed=self.open(wire); eliciting=False; consumed=set()
        for kind,flags,body in parsed:
            if kind!=13: continue
            assert self.typed and len(body)==16
            stream,credit,mode,priority,weight=struct.unpack('!IQBBH',body)
            assert stream in ((3,5) if self.responder else (2,4)) and mode==(1 if stream>=4 else 0) and priority<=3 and weight
            value=(credit,mode,priority,weight)
            assert stream not in self.remote_opens or self.remote_opens[stream]==value,'OPEN changed on retransmission'
            self.remote_opens[stream]=value; self.extra_credit[stream]=max(self.extra_credit[stream],credit)
        for kind,flags,body in parsed:
            eliciting|=kind not in (0,2)
            if kind==2:
                delay,count,reserved=struct.unpack_from('!IHH',body); assert not reserved and 0<count<=32 and len(body)==8+16*count
                previous=2**64
                for index in range(count):
                    first,last=struct.unpack_from('!QQ',body,8+16*index)
                    assert 0<first<=last<previous-1 and last<self.send_number
                    previous=first; assert last-first<10000
                    if time.monotonic()>=self.mute_until: self.acked.update(range(first,last+1))
                    else: self.muted_acks+=1
            elif kind==3:
                stream,offset=struct.unpack_from('!IQ',body)
                assert stream==1 or (self.typed and 2<=stream<6)
                received=self.received if stream==1 else self.extra_receive[stream]
                for i,value in enumerate(body[12:],offset):
                    assert i<8192 and received.get(i,value)==value
                    if i not in received: received[i]=value; consumed.add(stream)
                if flags:
                    final=offset+len(body)-12
                    if stream==1: assert self.final in (None,final); self.final=final
                    else: assert self.extra_final.get(stream,final)==final; self.extra_final[stream]=final
            elif kind==5:
                credit=struct.unpack('!Q',body)[0]
                assert credit<=self.initial_credit+sum(self.sent_high.values()),'connection credit consumed twice'
                self.credit=max(self.credit,credit)
            elif kind==6:
                stream,credit=struct.unpack('!IQ',body); assert stream in self.sent_high
                assert credit<=self.initial_stream_credit+self.sent_high[stream],'stream credit consumed twice'
                if stream==1: self.stream_credit=max(self.stream_credit,credit)
                else: self.extra_credit[stream]=max(self.extra_credit[stream],credit)
            elif kind==7: assert self.typed and 3<=struct.unpack('!I',body)[0]<=2**31
            elif kind==8:
                assert self.typed and len(body)>=12
                channel,sequence=struct.unpack_from('!IQ',body)
                assert channel in (7,8,9) and ((channel==7 and not flags and not sequence) or (channel!=7 and flags==1 and sequence==1))
                self.datagrams.setdefault(channel,[]).append(body[12:])
            elif kind==13: pass
            elif kind==12: assert len(body)==4 and 1<=struct.unpack('!I',body)[0]<=self.send_epoch
            elif kind==0: assert not any(body)
            elif kind==1: assert not body
            else: self.control(kind,flags,body)
        reply=b''
        if eliciting: reply+=self.ack()
        if self.receive_epoch!=before: reply+=frame(12,struct.pack('!I',self.receive_epoch))
        if consumed:
            total=0
            for stream in range(1,6 if self.typed else 2):
                received=self.received if stream==1 else self.extra_receive[stream]; contiguous=0
                while contiguous in received: contiguous+=1
                total+=contiguous
                if stream in consumed: reply+=frame(6,struct.pack('!IQ',stream,self.receive_credit(stream,contiguous)))
            reply+=frame(5,struct.pack('!Q',total+1024))
        if reply: self.raw(self.protect(reply))
    def control(self,kind,flags,body):
        raise AssertionError(('unexpected frame',kind))
    def receive_credit(self,stream,contiguous):
        return contiguous+512
    def expected_default_body(self):
        return bytes((i*7+19)&255 for i in range(4096))
    def validate_control(self,result):
        pass
    def expected_games(self):
        return 3 if self.typed else 0
    def cleanup(self):
        pass
    def service(self):
        try:
            data=self.receive()
            if decode(data)[0] in (4,5): self.input(data)
        except socket.timeout: pass
    def reliable(self,payload,poison=False):
        for kind,_,body in frames(payload):
            if kind==3:
                stream,offset=struct.unpack_from('!IQ',body)
                self.sent_high[stream]=max(self.sent_high[stream],offset+len(body)-12)
        end=time.monotonic()+5; next_send=0; copies=[]
        while not any(pn in self.acked for pn in copies):
            now=time.monotonic(); assert now<end,'independent stop/wait deadline'
            if now>=next_send:
                data=self.protect(payload); copies.append(self.send_number-1)
                if len(copies)>1: self.retransmits+=1
                if poison and not self.invalid_injected:
                    corrupt=bytearray(data); corrupt[-1]^=1; self.raw(corrupt); self.invalid_injected+=1
                self.raw(data)
                if poison and not self.replay_injected: self.raw(data); self.replay_injected+=1
                next_send=now+.25
            self.service()
    def transfer(self):
        if self.typed: return self.transfer_typed()
        body=bytes((i*11+3)&255 for i in range(3072)); offset=0
        self.mute_until=time.monotonic()+.28
        while offset<len(body):
            end=time.monotonic()+4
            while min(self.credit,self.stream_credit)<=offset:
                assert time.monotonic()<end,'peer flow credit stalled'; self.service()
            size=min(400,len(body)-offset,min(self.credit,self.stream_credit)-offset)
            self.reliable(frame(3,struct.pack('!IQ',1,offset)+body[offset:offset+size]),poison=offset==0)
            offset+=size
        self.reliable(frame(3,struct.pack('!IQ',1,offset),1))
        end=time.monotonic()+5
        while self.final!=4096 or len(self.received)!=4096:
            assert time.monotonic()<end,'server FIN/bytes stalled'; self.service()
        assert bytes(self.received[i] for i in range(4096))==bytes((i*7+19)&255 for i in range(4096))
        assert self.rotations>0 and self.send_epoch==2 and self.retransmits>0
        self.raw(self.protect(self.ack()))
    @staticmethod
    def typed_body(stream,native):
        factor,base=(7,19) if native else (11,3)
        if stream<4:
            size=(1536 if native else 2304)+stream
            return bytes((i*factor+base+stream)&255 for i in range(size))
        return b''.join(struct.pack('!I',size)+bytes((i*factor+base+stream+n)&255 for i in range(size)) for n,size in enumerate((0,513,4097)))
    def transfer_typed(self):
        outgoing={1:bytes((i*11+3)&255 for i in range(3072))}
        outgoing.update({i:self.typed_body(i,False) for i in range(2,6)})
        own=(2,4) if self.responder else (3,5)
        offsets={i:0 for i in outgoing}; offsets[own[0]]=1
        # Legal packet-wide preflight: data may precede its OPEN, and the two
        # continuous ordinals need not be serialized in frame order.
        early=frame(3,struct.pack('!IQ',own[0],0)+outgoing[own[0]][:1])
        self.reliable(early+b''.join(frame(13,struct.pack('!IQBBH',i,512,int(i>=4),1,1)) for i in reversed(own)))
        end=time.monotonic()+4
        while len(self.remote_opens)!=2:
            assert time.monotonic()<end,'native OPEN stalled'; self.service()
        for channel,sequence,body in ((7,0,b''),(7,1,b'wrong-mode'),(8,5,b'ordered'),(8,4,b'old'),
            (9,11,b'old'),(9,12,b'latest'),(9,10,b'stale'),(99,0,b'unknown')):
            # Canonical game output: one record + key confirmation, never
            # replayed on ACK loss. The app defers reads until reliable FIN.
            game=frame(8,struct.pack('!IQ',channel,sequence)+body,int(sequence!=0))
            self.raw(self.protect(game+frame(12,struct.pack('!I',self.receive_epoch))))
        self.mute_until=time.monotonic()+.28
        end=time.monotonic()+8
        while any(offsets[i]<len(body) for i,body in outgoing.items()):
            progressed=False
            for stream,body in outgoing.items():
                offset=offsets[stream]
                credit=self.stream_credit if stream==1 else self.extra_credit[stream]
                room=min(self.credit-sum(offsets.values()),credit-offset,len(body)-offset,400)
                if room<=0: continue
                self.reliable(frame(3,struct.pack('!IQ',stream,offset)+body[offset:offset+room]),poison=stream==1 and offset==0)
                offsets[stream]+=room; progressed=True
            if progressed: end=time.monotonic()+8
            else: assert time.monotonic()<end,'mixed small-window credit deadlock'; self.service()
        for stream,offset in offsets.items(): self.reliable(frame(3,struct.pack('!IQ',stream,offset),1))
        expected={1:self.expected_default_body()}
        expected.update({i:self.typed_body(i,True) for i in range(2,6)})
        end=time.monotonic()+5
        while not (self.final==len(expected[1]) and len(self.received)==len(expected[1]) and len(self.datagrams)==3 and
            all(self.extra_final.get(i)==len(expected[i]) and len(self.extra_receive[i])==len(expected[i]) for i in range(2,6))):
            assert time.monotonic()<end,'mixed receive/FIN/game stalled'; self.service()
        for stream,body in expected.items():
            received=self.received if stream==1 else self.extra_receive[stream]
            assert bytes(received[i] for i in range(len(body)))==body,('mixed output bytes',stream)
        assert self.datagrams=={7:[b''],8:[b'ordered'],9:[b'latest']},'game duplicate/stale/latest policy'
        assert self.rotations>0 and self.send_epoch==2 and self.retransmits>0
        self.raw(self.protect(self.ack()))


def build(args):
    sdk=args.sdk.resolve(); core=sdk/'lib/xrt.h'
    source_commit=subprocess.check_output(['git','rev-parse','--verify','HEAD^{commit}'],cwd=ROOT,text=True).strip()
    committed=subprocess.check_output(['git','show',source_commit+':single/xrt.h'],cwd=ROOT)
    assert core.read_bytes()==committed,'SDK must use the current committed core; never export dirty core'
    output=ROOT/'out/xrudp-wire-interop'; output.mkdir(parents=True,exist_ok=True)
    binary=output/('server.exe' if os.name=='nt' else 'server')
    assert (sdk/'lib/xrt_decl.h').read_bytes()==subprocess.check_output(['git','show',source_commit+':single/xrt_decl.h'],cwd=ROOT)
    command=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1','-DXRUDP_MODULE_ALL=1',
        '-I'+str(sdk/'lib'),'-I'+str(ROOT/'extlibs/xrudp/include'),'-I'+str(sdk/'lib/xrtshim')]
    if args.sanitize: command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    command+=[str(ROOT/'extlibs/xrudp/tests/test_wire_interop_server.c')]
    command+=list(map(str,sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))))
    command+=['-o',str(binary)]+(['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-lm','-ldl'])
    paths=[core,sdk/'lib/xrt_decl.h',ROOT/'extlibs/xrudp/tests/test_wire_interop_server.c']
    for directory,suffix in ((ROOT/'extlibs/xrudp/src','*.c'),(ROOT/'extlibs/xrudp/src','*.h'),
            (ROOT/'extlibs/xrudp/include','*.h'),(sdk/'lib/xrtshim','*.h')):
        paths+=sorted(directory.rglob(suffix))
    for path in paths:
        if path.is_relative_to(ROOT/'extlibs/xrudp/src') or path.is_relative_to(ROOT/'extlibs/xrudp/include'):
            upstream=subprocess.check_output(['git','show',source_commit+':'+path.relative_to(ROOT).as_posix()],cwd=ROOT)
            assert path.read_bytes().replace(b'\r\n',b'\n')==upstream.replace(b'\r\n',b'\n'),'commit runtime changes before an acceptance build'
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    version=subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0]
    key=hashlib.sha256(json.dumps([command,version,hashes],sort_keys=True).encode()).hexdigest()
    receipt=binary.with_name(binary.name+'.build.json')
    if args.reuse:
        stored=json.loads(receipt.read_text())
        assert stored['input_key']==key and stored['binary_sha256']==hashlib.sha256(binary.read_bytes()).hexdigest(),'native inputs changed; rebuild without --reuse'
        return binary
    subprocess.run(command,cwd=ROOT,check=True)
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==value for p,value in hashes.items()),'native input changed during build'
    receipt.write_text(json.dumps({'input_key':key,'compiler':version,'source_hashes':hashes,'source_commit':source_commit,
        'command':command,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n',encoding='utf-8',newline='\n')
    return binary


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--reuse',action='store_true',help='require matching source/compiler/flags/binary provenance')
    parser.add_argument('--roles',choices=('both','initiator','responder'),default='both',help='independent Python peer roles')
    parser.add_argument('--suite',choices=('basic','mixed','both'),default='both')
    parser.add_argument('--backends',choices=('select','native','both'),default='both')
    args=parser.parse_args(); binary=build(args)
    roles=(False,True) if args.roles=='both' else (args.roles=='responder',)
    suites=(False,True) if args.suite=='both' else (args.suite=='mixed',)
    backends=(False,True) if args.backends=='both' else (args.backends=='native',)
    for native_backend in backends:
        for typed in suites:
            for responder in roles:
                for family in (4,6): run_case(binary,family,responder,typed,native_backend)
    return 0


def run_case(binary,family,responder,typed,native_backend,peer_type=Peer,fixture_mode=None,graceful=False):
    host='127.0.0.1' if family==4 else '::1'
    with socket.socket(socket.AF_INET if family==4 else socket.AF_INET6,socket.SOCK_DGRAM) as sock:
        sock.bind((host,0)); sock.settimeout(.05)
        argv=[str(binary),str(family),str(sock.getsockname()[1]) if responder else '0',str(int(typed) if fixture_mode is None else fixture_mode),str(int(native_backend))]
        child=subprocess.Popen(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
        lines=queue.Queue()
        def read_lines():
            for line in child.stdout: lines.put(line)
        reader=threading.Thread(target=read_lines,daemon=True); reader.start()
        peer=None
        try:
            ready=json.loads(lines.get(timeout=5)); assert ready['ready']
            address=(host,ready['port']) if family==4 else (host,ready['port'],0,0)
            peer=peer_type(sock,address,responder,typed); peer.handshake(); peer.transfer()
            assert json.loads(lines.get(timeout=4))['app_done']
            if graceful:
                until=time.monotonic()+6
                while child.poll() is None:
                    assert time.monotonic()<until,'native graceful close deadline'
                    try: peer.service()
                    except (ConnectionResetError,ConnectionRefusedError):
                        # A final ACK can race the already-closed UDP socket.
                        # Only the actual child exit and terminal proof below
                        # may establish success, never ICMP alone.
                        child.wait(timeout=2)
            else: peer.raw(peer.protect(frame(9,struct.pack('!I',0))))
            code=child.wait(timeout=6); reader.join(1); error=child.stderr.read()
            assert code==0,(code,error)
            result=json.loads(lines.get(timeout=1)); assert result['read']==3072 and result['confirmed']==1 and result['closes']==1
            assert result['streams']==(5 if typed else 1) and result['messages']==(6 if typed else 0) and result['games']==peer.expected_games()
            peer.validate_control(result)
            result.update(family=family,typed=typed,backend=('iocp' if os.name=='nt' else 'epoll') if native_backend else 'select',
                python_role='responder' if responder else 'initiator',independent_noise=True,native_default_bytes=len(peer.received),
                rekeys=peer.rotations,client_rekey=peer.send_epoch,retransmits=peer.retransmits,
                bad_tags=peer.invalid_injected,replayed_packets=peer.replay_injected)
            print(json.dumps(result),flush=True)
        finally:
            failed=sys.exc_info()[0] is not None
            if child.poll() is None:
                child.terminate(); child.wait(timeout=5)
            reader.join(1)
            if failed:
                diagnostic=child.stderr.read()
                pending=[]
                while not lines.empty() and len(pending)<4: pending.append(lines.get_nowait().strip())
                if diagnostic or pending:
                    print('native failure diagnostic: '+(diagnostic+'\n'+'\n'.join(pending))[:8192],file=sys.stderr,flush=True)
            child.stdout.close(); child.stderr.close()
            if peer is not None: peer.cleanup()


if __name__=='__main__': raise SystemExit(main())
