"""Independent authenticated A->B->A UDP rebinding and path isolation."""
import argparse
from pathlib import Path
import socket
import struct
import time

from test_xrudp_wire_interop import Peer, build, decode, frame, run_case


class PathPeer(Peer):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.extra_sockets=[]
        self.original=self.sock
        self.previous=None
        self.spare=None
        self.phase=0
        self.token=None
        self.proof_at=0
        self.proof_sent=False
        self.rejected=[]
        self.path_packets=0

    def response(self,token):
        return frame(11,token)+frame(12,struct.pack('!I',self.receive_epoch))

    def active_evidence(self):
        target=self.sock
        try:
            self.sock=self.previous
            number=self.send_number
            self.raw(self.protect(frame(1)))
            until=time.monotonic()+1
            while number not in self.acked:
                assert time.monotonic()<until,'old active path stopped serving during validation'
                try: wire,source=self.sock.recvfrom(2048)
                except socket.timeout: continue
                assert source==self.address
                # A previous attempt's PATH retry can still be queued on this
                # socket. Only ordinary DATA supplies the active-PING receipt.
                if decode(wire)[0]==4: Peer.input(self,wire)
        finally:
            self.sock=target

    def control(self,kind,flags,body):
        assert kind==10 and flags==0 and len(body)==16,('unexpected path control',kind)
        assert self.phase in (1,2)
        if self.token is None:
            self.token=body
            self.proof_at=time.monotonic()+.15
            # The right token from a third source must not prove this candidate.
            self.rejected.append(self.send_number)
            self.spare.sendto(self.protect(self.response(body),kind=5),self.address)
            forbidden=frame(8,struct.pack('!IQ',7,0)+b'forbidden')
            self.rejected.append(self.send_number)
            self.raw(self.protect(forbidden))
            # Even a matching response may not smuggle business into candidate
            # admission. This is valid DATA syntax, not an AEAD failure.
            self.rejected.append(self.send_number)
            self.raw(self.protect(frame(11,body)+forbidden))
            # Authenticate the current path at a newer PN, then deliver an
            # older matching candidate proof. It must not override that newer
            # evidence or admit the following business payload.
            self.rejected.append(self.send_number)
            delayed=self.protect(self.response(body),kind=5)
            self.active_evidence()
            self.raw(delayed)
            self.rejected.append(self.send_number)
            self.raw(self.protect(forbidden))
            self.proof_at=time.monotonic()+.15
        else:
            assert self.token==body,'candidate token changed during one attempt'
            if self.proof_sent: self.raw(self.protect(self.response(body),kind=5))

    def input(self,wire):
        if decode(wire)[0]!=5:
            return super().input(wire)
        assert 84<=len(wire)<=128
        parsed=self.open(wire); path=False
        for kind,flags,body in parsed:
            assert flags==0
            if kind in (10,11):
                path=True; self.control(kind,flags,body)
            elif kind==12:
                assert len(body)==4 and 1<=struct.unpack('!I',body)[0]<=self.send_epoch
            elif kind==0: assert not any(body)
            else: raise AssertionError(('forbidden content in PATH packet',kind))
        if parsed:
            assert path
            self.path_packets+=1
        # PATH uses the normal packet ledger. Delay DATA ACKs until our source
        # has supplied a probe-only proof, then acknowledge all seen controls.
        if self.proof_sent:
            self.raw(self.protect(self.ack()+frame(12,struct.pack('!I',self.receive_epoch))))

    def service(self):
        super().service()
        if self.token is not None and not self.proof_sent and time.monotonic()>=self.proof_at:
            try: unexpected=self.spare.recvfrom(2048)
            except socket.timeout: pass
            else: raise AssertionError(('response to wrong candidate source',len(unexpected[0])))
            self.raw(self.protect(self.response(self.token),kind=5))
            self.proof_sent=True
            self.raw(self.protect(self.ack()+frame(12,struct.pack('!I',self.receive_epoch))))

    def transfer(self):
        super().transfer()
        host=self.original.getsockname()[0]
        for _ in range(2):
            sock=socket.socket(self.original.family,socket.SOCK_DGRAM)
            self.extra_sockets.append(sock)
            sock.bind((host,0)); sock.settimeout(.05)
        alternate,self.spare=self.extra_sockets
        self.spare.settimeout(.01)
        assert len({s.getsockname()[1] for s in (self.original,alternate,self.spare)})==3
        try:
            for self.phase,target in enumerate((alternate,self.original),1):
                self.previous=self.original if self.phase==1 else alternate
                self.sock=target; self.token=None; self.proof_sent=False
                # An initial business packet triggers bounded validation but
                # cannot be ACKed. A new-PN retry succeeds after the proof.
                self.rejected.append(self.send_number)
                self.reliable(frame(1))
                assert self.proof_sent,'business ACK before candidate validation'
                self.before_migration_marker()
                marker=b'migrate1' if self.phase==1 else b'migrate2'
                self.raw(self.protect(frame(8,struct.pack('!IQ',7,0)+marker)+
                                      frame(12,struct.pack('!I',self.receive_epoch))))
                until=time.monotonic()+3
                while marker not in self.datagrams.get(7,[]):
                    assert time.monotonic()<until,'business echo on migrated socket stalled'
                    self.service()
                self.after_migration_marker(marker)
            assert self.datagrams=={7:[b'',b'migrate1',b'migrate2'],8:[b'ordered'],9:[b'latest']}
            assert all(p not in self.acked for p in self.rejected),'unadmitted candidate packet was falsely ACKed'
        finally:
            self.sock=self.original

    def before_migration_marker(self):
        pass

    def after_migration_marker(self,marker):
        pass

    def expected_games(self):
        return 5

    def validate_control(self,result):
        assert result['path_verified'] and result['path_changes']==2 and result['joined']
        assert result['close_reason']==13 and self.path_packets>=2 and len(self.rejected)==12
        result.update(control='path-rebinding',migrations=2,path_packets=self.path_packets,
                      rejected_candidate_packets=len(self.rejected),business_markers=2)

    def cleanup(self):
        # Keep all old UDP ports alive until the native endpoint and Worker
        # have joined; late old-route packets must not manufacture ICMP errors.
        for sock in self.extra_sockets: sock.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--roles',choices=('both','initiator','responder'),default='both')
    parser.add_argument('--families',choices=('both','4','6'),default='both')
    parser.add_argument('--backends',choices=('both','select','native'),default='both')
    args=parser.parse_args(); binary=build(args)
    roles=(False,True) if args.roles=='both' else (args.roles=='responder',)
    families=(4,6) if args.families=='both' else (int(args.families),)
    backends=(False,True) if args.backends=='both' else (args.backends=='native',)
    for backend in backends:
        for responder in roles:
            for family in families:
                run_case(binary,family,responder,True,backend,PathPeer,5)
    return 0


if __name__=='__main__': raise SystemExit(main())
