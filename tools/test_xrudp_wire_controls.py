"""Independent cancellation and graceful drain, with mixed streams and games."""
import argparse
from pathlib import Path
import struct
import time

from test_xrudp_wire_interop import Peer, build, decode, frame, run_case


class StopPeer(Peer):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.stop_sent=False
        self.reset_packets=set()
        self.current_number=0
        self.dropped_stop_number=0

    def receive_credit(self,stream,contiguous):
        # Native has 4096 bytes queued with FIN, but can only transmit 512.
        # Other streams continue consuming their normal sliding windows.
        return 512 if stream==1 else super().receive_credit(stream,contiguous)

    def expected_default_body(self):
        return super().expected_default_body()[:512]

    def input(self,wire):
        self.current_number=decode(wire)[3]
        super().input(wire)
        if not self.stop_sent and len(self.received)==512:
            self.stop_sent=True
            request=frame(14,struct.pack('!II',1,77))
            self.dropped_stop_number=self.send_number
            self.protect(request)  # Deliberately lose the first request.
            self.raw(self.protect(request))
            self.raw(self.protect(request))  # Same logical request, new PN.

    def control(self,kind,flags,body):
        assert kind==4 and flags==0 and len(body)==16,('unexpected control',kind)
        identifier,final,reason=struct.unpack('!IQI',body)
        assert (identifier,final,reason)==(1,512,77),'RESET must use sent high-water, not 4096 queued bytes'
        assert self.stop_sent and len(self.received)==512
        if not self.reset_packets:
            # Withhold the whole first RESET packet's ACK. Its replacement
            # must use a fresh packet number while other streams keep running.
            self.ack_omitted.add(self.current_number)
        self.reset_packets.add(self.current_number)
        self.final=final

    def validate_control(self,result):
        assert self.stop_sent and len(self.reset_packets)>=2,'RESET loss recovery was not exercised'
        assert len(self.ack_omitted)==1 and result['stop_verified']
        assert self.dropped_stop_number not in self.acked,'peer ACKed a request that was never sent'
        assert result['close_reason']==13 and result['joined']
        result.update(control='stop-reset',reset_packets=len(self.reset_packets),
                      reset_final=512,stop_request_dropped=1,reset_ack_withheld=1)


class ShutdownPeer(Peer):
    early_declaration=False
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.peer_declaration=None
        self.native_drained=set()
        self.current_number=0
        self.local_declared=False
        self.dropped=[]

    def input(self,wire):
        self.current_number=decode(wire)[3]
        super().input(wire)

    def control(self,kind,flags,body):
        assert flags==0
        if kind==15:
            value=struct.unpack('!II',body)
            assert value==(3,5000),'native final stream ordinal/drain timeout'
            assert self.peer_declaration in (None,value),'changed shutdown declaration'
            self.peer_declaration=value
        elif kind==16:
            assert not body and self.local_declared and self.peer_declaration
            if not self.native_drained: self.ack_omitted.add(self.current_number)
            self.native_drained.add(self.current_number)
        else: super().control(kind,flags,body)

    def transfer(self):
        declaration=frame(15,struct.pack('!II',3,5000))
        if self.early_declaration:
            # Declare all planned stream ordinals before their reordered OPENs.
            # The native side must still accept those OPENs and complete its
            # existing byte/message replies before its own Shutdown call.
            self.local_declared=True
            self.dropped.append(self.send_number); self.protect(declaration)
            self.reliable(declaration)
        super().transfer()
        until=time.monotonic()+3
        while self.peer_declaration is None:
            assert time.monotonic()<until,'native shutdown declaration stalled'
            self.service()
        if not self.early_declaration:
            self.local_declared=True
            self.dropped.append(self.send_number); self.protect(declaration)
        # First real admission sees DRAINED before SHUTDOWN in one packet.
        # All application data, messages and game echoes were consumed above.
        final=frame(16)+declaration
        self.dropped.append(self.send_number); self.protect(final)
        self.reliable(final)

    def validate_control(self,result):
        assert self.local_declared and self.peer_declaration==(3,5000)
        assert len(self.native_drained)>=2,'native DRAINED ACK loss was not recovered'
        assert len(self.ack_omitted)==1 and all(p not in self.acked for p in self.dropped)
        assert result['shutdown_verified'] and result['close_reason']==13 and result['joined']
        result.update(control='shutdown',native_drained_packets=len(self.native_drained),
                      shutdown_order='peer-first' if self.early_declaration else 'native-first',
                      shutdown_request_dropped=1,drained_packet_dropped=1,drained_ack_withheld=1)


class EarlyShutdownPeer(ShutdownPeer):
    early_declaration=True


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--roles',choices=('both','initiator','responder'),default='both')
    parser.add_argument('--families',choices=('both','4','6'),default='both')
    parser.add_argument('--backends',choices=('both','select','native'),default='both')
    parser.add_argument('--control',choices=('all','stop','shutdown'),default='all')
    parser.add_argument('--shutdown-order',choices=('both','native-first','peer-first'),default='both')
    args=parser.parse_args(); binary=build(args)
    roles=(False,True) if args.roles=='both' else (args.roles=='responder',)
    families=(4,6) if args.families=='both' else (int(args.families),)
    backends=(False,True) if args.backends=='both' else (args.backends=='native',)
    controls=[]
    if args.control in ('all','stop'): controls.append((StopPeer,3,False))
    if args.control in ('all','shutdown'):
        if args.shutdown_order in ('both','native-first'): controls.append((ShutdownPeer,4,True))
        if args.shutdown_order in ('both','peer-first'): controls.append((EarlyShutdownPeer,4,True))
    for backend in backends:
        for responder in roles:
            for family in families:
                for peer_type,mode,graceful in controls:
                    run_case(binary,family,responder,True,backend,peer_type,mode,graceful)
    return 0


if __name__=='__main__': raise SystemExit(main())
