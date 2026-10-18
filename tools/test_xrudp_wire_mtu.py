"""Independent exact padded MTU proofs and large-probe blackhole isolation.

No ordinary business is queued by the native fixture until its public MTU
snapshot proves growth or bounded growth failure. A blocked 1024B probe must
never enter the receiver ACK history, yet all five streams remain usable at
the independently admitted 768B ceiling. This is not a full MTU gate: a path
shrinking after large business was admitted is a separate recovery case.
"""
import argparse
from pathlib import Path
import struct
import time

from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from test_xrudp_wire_interop import Peer,build,decode,frames,nonce,rekey,run_case


class MtuPeer(Peer):
    ceiling=1200

    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.probes={}; self.dropped={}; self.probe_acks=[]

    def input(self,wire):
        kind,epoch,cid,number,body=decode(wire)
        assert kind==4 and cid==self.cid
        key=self.receive_keys.get(epoch)
        if key is None and epoch<self.receive_epoch:
            # A delayed packet older than the retained receive epochs follows
            # the ordinary peer's discard path; it supplies no MTU evidence.
            return super().input(wire)
        if key is None:
            assert epoch==self.receive_epoch+1,(epoch,self.receive_epoch)
            key=rekey(self.receive_keys[self.receive_epoch])
        # Independent inspection does not commit replay/key/ACK state. The
        # oversized oracle branch drops before the normal receiver sees it.
        parsed=frames(ChaCha20Poly1305(key).decrypt(nonce(number),body,wire[:48]))
        probe=[k for k,_,_ in parsed]==[1,12,0]
        if probe:
            assert not parsed[0][1] and not parsed[0][2]
            assert not parsed[1][1] and struct.unpack('!I',parsed[1][2])[0]<=self.send_epoch
            assert not parsed[2][1] and not any(parsed[2][2])
            assert len(wire) in (512,768,1024,1200)
            assert number not in self.probes and number not in self.dropped,'MTU probe reused a packet number'
            if len(wire)>self.ceiling:
                self.dropped[number]=len(wire)
                return
            self.probes[number]=len(wire)
        else:
            assert len(wire)<=self.ceiling,'business exceeded independently proved MTU'
        super().input(wire)
        if probe:
            assert number in self.seen and number not in self.ack_omitted
            self.probe_acks.append(number)

    def transfer(self):
        until=time.monotonic()+12
        while list(self.probes.values())!=([512,768] if self.ceiling==768 else [512,768,1024,1200]) or (
            self.ceiling==768 and len(self.dropped)<3):
            assert time.monotonic()<until,('bounded MTU search did not reach its proof/failure fence',
                self.sock.family,self.responder,list(self.probes.values()),list(self.dropped.values()),
                self.receive_epoch,self.send_number)
            self.service()
        super().transfer()
        assert not self.seen.intersection(self.dropped),'blackholed probe was admitted into ACK history'

    def validate_control(self,result):
        assert result['mtu_verified'] and result['mtu_effective']==self.ceiling and result['joined']
        assert list(self.probes)==self.probe_acks
        if self.ceiling==768:
            assert list(self.dropped.values())==[1024]*3
            assert result['mtu_probes']==5 and result['mtu_acknowledged']==2 and result['mtu_failures']==3
        else:
            assert not self.dropped
            assert result['mtu_probes']==result['mtu_acknowledged']==4 and result['mtu_failures']==0
        result.update(control='mtu-growth' if self.ceiling==1200 else 'mtu-growth-blackhole',
                      probe_sizes=list(self.probes.values()),dropped_probe_sizes=list(self.dropped.values()),
                      authenticated_probe_acks=len(self.probe_acks),unadmitted_probe_acks=0)


class LimitedMtuPeer(MtuPeer):
    ceiling=768


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--roles',choices=('both','initiator','responder'),default='both')
    parser.add_argument('--families',choices=('both','4','6'),default='both')
    parser.add_argument('--backends',choices=('both','select','native'),default='both')
    parser.add_argument('--case',choices=('all','growth','limited'),default='all')
    args=parser.parse_args(); binary=build(args)
    roles=(False,True) if args.roles=='both' else (args.roles=='responder',)
    families=(4,6) if args.families=='both' else (int(args.families),)
    backends=(False,True) if args.backends=='both' else (args.backends=='native',)
    cases=[]
    if args.case in ('all','growth'): cases.append((MtuPeer,6))
    if args.case in ('all','limited'): cases.append((LimitedMtuPeer,7))
    for backend in backends:
        for responder in roles:
            for family in families:
                for peer_type,mode in cases:
                    run_case(binary,family,responder,True,backend,peer_type,mode)
    return 0


if __name__=='__main__': raise SystemExit(main())
