"""Independent address validation cannot carry MTU evidence to another path.

Complete a 1200B proof, ordinary mixed business, then A->B->A validation with
the same Noise/CID/packet history. Hold each new 512B probe's ACK until native
business observes the reset in its public state. Only then prove all four sizes
again, including on return to the earlier UDP port.
"""
import argparse
from pathlib import Path
import struct
import time

from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from test_xrudp_wire_interop import build,decode,frame,frames,nonce,rekey,run_case
from test_xrudp_wire_path import PathPeer


class PathMtuPeer(PathPeer):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.mtu_proofs={i:{} for i in range(3)}
        self.mtu_numbers=set(); self.held={1:set(),2:set()}; self.released=set()

    def ack(self):
        pending=self.held.get(self.phase,set()) if self.phase not in self.released else set()
        if not pending: return super().ack()
        # Withholding only the probe while acknowledging later PNs would
        # legitimately declare it lost by packet threshold. Delay that ACK
        # suffix as well, then release every genuinely received PN together.
        omitted=self.ack_omitted
        self.ack_omitted=omitted|{pn for pn in self.seen if pn>=min(pending)}
        try: return super().ack()
        finally: self.ack_omitted=omitted

    def input(self,wire):
        kind,epoch,cid,number,body=decode(wire)
        if kind==4:
            assert cid==self.cid
            key=self.receive_keys.get(epoch)
            if key is None and epoch<self.receive_epoch: return super().input(wire)
            if key is None:
                assert epoch==self.receive_epoch+1
                key=rekey(self.receive_keys[self.receive_epoch])
            parsed=frames(ChaCha20Poly1305(key).decrypt(nonce(number),body,wire[:48]))
            if [k for k,_,_ in parsed]==[1,12,0]:
                assert not parsed[0][1] and not parsed[0][2]
                assert not parsed[1][1] and 1<=struct.unpack('!I',parsed[1][2])[0]<=self.send_epoch
                assert not parsed[2][1] and not any(parsed[2][2])
                assert number not in self.mtu_numbers,'MTU packet-number reuse across paths'
                self.mtu_numbers.add(number); self.mtu_proofs[self.phase][number]=len(wire)
                assert len(wire) in (512,768,1024,1200)
                if self.phase and self.phase not in self.released:
                    assert len(wire)==512,'native reused a larger size before new base proof'
                    self.held[self.phase].add(number); self.ack_omitted.add(number)
        super().input(wire)
        if self.phase and self.phase not in self.released:
            marker=b'migrate1' if self.phase==1 else b'migrate2'
            if marker in self.datagrams.get(7,[]):
                assert len(self.held[self.phase])==1
                self.released.add(self.phase)
                self.ack_omitted.difference_update(self.held[self.phase])
                self.raw(self.protect(self.ack()+frame(12,struct.pack('!I',self.receive_epoch))))

    def wait_proofs(self,phase):
        until=time.monotonic()+3
        while list(self.mtu_proofs[phase].values())!=[512,768,1024,1200]:
            assert time.monotonic()<until,('path size proof stalled',phase,list(self.mtu_proofs[phase].values()))
            self.service()

    def transfer(self):
        self.wait_proofs(0)
        super().transfer()

    def before_migration_marker(self):
        until=time.monotonic()+2
        while not self.held[self.phase]:
            assert time.monotonic()<until,'new path did not restart from a base probe'
            self.service()
        assert list(self.mtu_proofs[self.phase].values())==[512]
        assert self.held[self.phase]<=self.ack_omitted

    def after_migration_marker(self,marker):
        assert self.phase in self.released and marker in self.datagrams[7]
        self.wait_proofs(self.phase)

    def validate_control(self,result):
        super().validate_control(result)
        assert result['path_mtu_resets']==2 and result['mtu_verified'] and result['mtu_effective']==1200
        assert result['mtu_probes']==result['mtu_acknowledged']==12 and not result['mtu_failures'] and not result['mtu_fallbacks']
        assert self.released=={1,2} and not self.ack_omitted and len(self.mtu_numbers)==12
        assert all(len(self.held[i])==1 for i in (1,2))
        assert all(list(v.values())==[512,768,1024,1200] for v in self.mtu_proofs.values())
        result.update(control='path-mtu-reproof',size_proof_cycles=3,unique_mtu_packets=len(self.mtu_numbers),
            held_base_acknowledgements=2,proof_sizes=[list(v.values()) for v in self.mtu_proofs.values()])


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
                run_case(binary,family,responder,True,backend,PathMtuPeer,9)
    return 0


if __name__=='__main__': raise SystemExit(main())
