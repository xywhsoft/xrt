"""Independent native recovery after a proved 1200B path shrinks to 512B.

Real ACKed padded probes first establish the large path. The independent
receiver then discards larger business before committing replay/ACK state,
while small PING round trips continue. Reliable ranges must arrive exactly
after resegmentation; lost games cannot be replayed or split into smaller games.
"""
import argparse
from pathlib import Path
import struct
import time

from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from test_xrudp_wire_interop import Peer,build,decode,frame,frames,nonce,rekey,run_case
from test_xrudp_wire_mtu import MtuPeer


class ShrinkPeer(MtuPeer):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.shrunk=False; self.blackholed={}; self.large_games=set()
        self.dropped_bytes={}; self.recovered=set(); self.small_data_sizes=[]
        self.base_rechecks=[]; self.small_roundtrips=0

    def parameters(self):
        # Real authenticated larger receive credits let the native sender use
        # its 1200B proof; the standard 512B stream credit cannot do so.
        return struct.pack('!QIIIIII',32768,8192,3,10000,25000,1200,8192)

    def input(self,wire):
        if not self.shrunk: return super().input(wire)
        kind,epoch,cid,number,body=decode(wire)
        assert kind==4 and cid==self.cid
        key=self.receive_keys.get(epoch)
        if key is None and epoch<self.receive_epoch: return Peer.input(self,wire)
        if key is None:
            assert epoch==self.receive_epoch+1
            key=rekey(self.receive_keys[self.receive_epoch])
        parsed=frames(ChaCha20Poly1305(key).decrypt(nonce(number),body,wire[:48]))
        probe=[k for k,_,_ in parsed]==[1,12,0]
        if len(wire)>512:
            assert not probe,'growth restarted inside the post-fallback holdoff'
            assert any(k in (3,8) for k,_,_ in parsed),'oversized control packet on a shrunk path'
            assert number not in self.blackholed,'reused encrypted business packet number'
            self.blackholed[number]=len(wire)
            for k,flags,value in parsed:
                if k==3:
                    stream,offset=struct.unpack_from('!IQ',value)
                    for index,byte in enumerate(value[12:],offset):
                        identity=(stream,index)
                        assert self.dropped_bytes.get(identity,byte)==byte
                        self.dropped_bytes[identity]=byte
                elif k==8:
                    channel,sequence=struct.unpack_from('!IQ',value)
                    marker=value[12:]
                    assert not flags and channel==7 and sequence==0 and len(marker)==700
                    assert marker[0]<8 and marker[1:]==b'G'*699
                    assert marker[0] not in self.large_games,'lost game payload was retransmitted'
                    self.large_games.add(marker[0])
            assert len(self.blackholed)<=32 and len(self.dropped_bytes)<=32768
            return
        if probe:
            assert len(wire)==512 and not any(parsed[2][2])
            self.base_rechecks.append(number)
        for k,_,value in parsed:
            if k==3 and len(value)>12:
                stream,offset=struct.unpack_from('!IQ',value)
                self.small_data_sizes.append(len(wire))
                for index,byte in enumerate(value[12:],offset):
                    if (stream,index) in self.dropped_bytes:
                        assert self.dropped_bytes[stream,index]==byte
                        self.recovered.add((stream,index))
        Peer.input(self,wire)

    def transfer(self):
        until=time.monotonic()+5
        while list(self.probes.values())!=[512,768,1024,1200]:
            assert time.monotonic()<until,'initial authenticated MTU growth stalled'
            self.service()
        self.shrunk=True; until=time.monotonic()+5; next_ping=0
        while not self.base_rechecks:
            assert time.monotonic()<until,('small traffic renewed the large-packet deadline',
                self.small_roundtrips,len(self.blackholed),len(self.large_games))
            if time.monotonic()>=next_ping:
                self.reliable(frame(1)); self.small_roundtrips+=1; next_ping=time.monotonic()+.05
            self.service()
        assert self.small_roundtrips>=2 and self.blackholed and self.large_games
        # Bypass MtuPeer's initial-growth transfer wrapper: the independent
        # receiver is now deliberately limited to 512B, retaining the same CID,
        # stream offsets, Noise keys, packet-number and ACK history.
        Peer.transfer(self)
        assert not self.seen.intersection(self.blackholed),'dropped large packet was falsely ACKed'
        assert self.dropped_bytes and len(self.recovered)==len(self.dropped_bytes),'lost reliable bytes were not recovered'
        assert self.small_data_sizes and max(self.small_data_sizes)<=512

    def validate_control(self,result):
        assert result['mtu_verified'] and result['mtu_effective']==512 and result['mtu_fallbacks']==1
        assert result['mtu_probes']==result['mtu_acknowledged']==5 and not result['mtu_failures']
        assert result['stall_receives']>=2 and result['game_lost']==len(self.large_games)>0
        assert result['game_size_dropped']>0 and result['game_lost']+result['game_size_dropped']==8
        assert len(self.base_rechecks)==1 and self.small_roundtrips>=2
        result.update(control='mtu-shrink-recovery',probe_sizes=list(self.probes.values()),
            base_rechecks=len(self.base_rechecks),small_roundtrips=self.small_roundtrips,
            dropped_business_packets=len(self.blackholed),largest_dropped=max(self.blackholed.values()),
            lost_reliable_bytes=len(self.dropped_bytes),recovered_reliable_bytes=len(self.recovered),
            largest_recovered_packet=max(self.small_data_sizes),lost_game_markers=len(self.large_games),
            falsely_acked_dropped_packets=0)


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
                run_case(binary,family,responder,True,backend,ShrinkPeer,8)
    return 0


if __name__=='__main__': raise SystemExit(main())
