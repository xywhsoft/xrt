"""Independent authenticated control mutations and unauthenticated UDP fuzz.

The peer encodes and protects its own packets using the published profile.
Only the native fixture uses product APIs. No account or production traffic.
"""
import argparse
import json
import os
from pathlib import Path
import queue
import random
import socket
import struct
import subprocess
import threading
import time

from test_xrudp_wire_interop import Peer, build, frame


def stream(identifier, offset, body=b'', fin=False):
    return frame(3, struct.pack('!IQ', identifier, offset)+body, int(fin))


def opened(identifier, credit=512, kind=0, priority=1, weight=1):
    return frame(13, struct.pack('!IQBBH', identifier, credit, kind, priority, weight))


def malformed_cases(responder):
    own = 2 if responder else 3
    ack = lambda *ranges: frame(2, struct.pack('!IHH', 0, len(ranges), 0)+
                                b''.join(struct.pack('!QQ', *r) for r in ranges))
    cases = {
        'empty-payload': b'',
        'truncated-tlv-header': b'\x01\x00\x00',
        'truncated-tlv-body': b'\x03\x00\x00\x10'+bytes(12),
        'unknown-frame': frame(17),
        'unknown-frame-255': frame(255),
        'nonzero-padding': frame(0, b'\x01'),
        'ping-body': frame(1, b'\x00'),
        'ping-flags': frame(1, flags=1),
        'ack-empty-ranges': ack(),
        'ack-zero-packet': ack((0, 1)),
        'ack-reversed-range': ack((2, 1)),
        'ack-adjacent-ranges': ack((3, 3), (2, 2)),
        'ack-overlapping-ranges': ack((2, 3), (1, 2)),
        'ack-unsent-packet': ack((1000000, 1000000)),
        'ack-reserved': frame(2, struct.pack('!IHHQQ', 0, 1, 1, 1, 1)),
        'stream-zero-id': stream(0, 0, b'x'),
        'stream-offset-overflow': stream(1, 2**64-1, b'x'),
        'stream-credit-overrun': stream(1, 1024, b'x'),
        'stream-conflicting-final': stream(1, 0, fin=True),
        'reset-below-high-water': frame(4, struct.pack('!IQI', 1, 0, 7)),
        'reset-credit-overrun': frame(4, struct.pack('!IQI', 1, 1025, 7)),
        'conflicting-stop-reasons': frame(14, struct.pack('!II', 1, 7))+frame(14, struct.pack('!II', 1, 8)),
        'max-streams-overflow': frame(7, struct.pack('!I', 2**31+1)),
        'datagram-zero-channel': frame(8, struct.pack('!IQ', 0, 0)),
        'datagram-sequence-without-flag': frame(8, struct.pack('!IQ', 7, 1)),
        'datagram-flag-without-sequence': frame(8, struct.pack('!IQ', 7, 0), 1),
        'key-confirm-zero': frame(12, struct.pack('!I', 0)),
        'key-confirm-future': frame(12, struct.pack('!I', 1000000)),
        'open-zero-id': opened(0),
        'open-wrong-origin': opened(own ^ 1),
        'open-default-id': opened(1),
        'open-credit-overflow': opened(own, 2**64-1),
        'open-conflicting-metadata': opened(own, 513),
        'open-invalid-kind': opened(own, kind=2),
        'open-invalid-priority': opened(own, priority=4),
        'open-zero-weight': opened(own, weight=0),
        'open-outside-credit': opened(own+4),
        'message-length-overflow': opened(own+2, kind=1)+stream(own+2, 0, struct.pack('!I', 8193)),
        'message-truncated-final': opened(own+2, kind=1)+stream(own+2, 0, struct.pack('!I', 4)+b'x', True),
        'shutdown-zero-ordinal': frame(15, struct.pack('!II', 0, 1000)),
        'shutdown-low-timeout': frame(15, struct.pack('!II', 2, 999)),
        'shutdown-high-timeout': frame(15, struct.pack('!II', 2, 600001)),
        'shutdown-excludes-open': frame(15, struct.pack('!II', 1, 1000)),
        'unrequested-drained': frame(16),
        'conflicting-path-challenges': frame(10, bytes(16))+frame(10, bytes([1])*16),
        'frame-count-overflow': frame(1)*65,
    }
    # Every fixed-width control also gets a structurally truncated body. The
    # hand-written encoder deliberately does not call a product validator.
    for kind, size in ((3,12),(4,16),(5,8),(6,12),(7,4),(8,12),(9,4),
                       (10,16),(11,16),(12,4),(13,16),(14,8),(15,8)):
        cases[f'control-{kind}-short'] = frame(kind, bytes(size-1))
    return cases


def prefix(responder):
    own = 2 if responder else 3
    return stream(1, 0, b'atomic-prefix')+opened(own)+frame(8, struct.pack('!IQ', 7, 0)+b'atomic-game')


def wait_ack(peer, number):
    end = time.monotonic()+3
    while number not in peer.acked:
        assert time.monotonic()<end, ('valid packet not acknowledged after rejected input', number)
        peer.service()


def unauthenticated(peer):
    rng = random.Random(0x5852554450)
    mutations = []
    # Alter each header byte, then ciphertext and tag bytes, without recomputing
    # AEAD. Malformed headers, unknown IDs and future epochs remain untrusted.
    for index in range(76):
        mutations.append(lambda wire, index=index: wire[:index]+bytes([wire[index]^0x80])+wire[index+1:])
    for size in range(76):
        mutations.append(lambda wire, size=size: wire[:size])
    for size in (1, 2, 8, 1201):
        mutations.append(lambda wire, size=size: wire+bytes(size))
    for _ in range(64):
        value = rng.randbytes(rng.randrange(1, 1202))
        mutations.append(lambda wire, value=value: value)
    for first in range(0, len(mutations), 16):
        valid = peer.protect(frame(1)+frame(12, struct.pack('!I', peer.receive_epoch)))
        number = peer.send_number-1
        assert len(valid)==76
        for mutate in mutations[first:first+16]:
            peer.raw(mutate(valid))
        # The original same-PN packet must still work: failed authentication
        # must not advance replay state, retire a key, or close the connection.
        peer.raw(valid)
        wait_ack(peer, number)
        peer.raw(valid)  # Authenticated duplicate remains harmless.
    return len(mutations)


def deferred(peer, responder, missing_open):
    own = 2 if responder else 3
    # A future OPEN within the negotiated grant is a legitimate reordered
    # packet, as is STREAM arriving before its OPEN. Neither can commit the
    # valid default-stream prefix or be ACKed as received business content.
    content = stream(1, 0, b'not-admitted')
    content += stream(own, 0, b'x') if missing_open else opened(own+2)
    deferred_number = peer.send_number
    peer.raw(peer.protect(content))
    fence = peer.send_number
    peer.raw(peer.protect(frame(1)))
    wait_ack(peer, fence)
    assert deferred_number not in peer.acked, 'deferred packet was falsely acknowledged'


def run_case(binary, family, responder, backend, name, payload=None):
    host = '127.0.0.1' if family==4 else '::1'
    reason = 5 if payload is not None else 13  # Public XRUDP_MALFORMED / CLOSED.
    with socket.socket(socket.AF_INET if family==4 else socket.AF_INET6, socket.SOCK_DGRAM) as sock:
        sock.bind((host, 0)); sock.settimeout(.05)
        argv = [str(binary), str(family), str(sock.getsockname()[1]) if responder else '0',
                '2', str(int(backend)), str(reason)]
        child = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1',
                 'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
        lines = queue.Queue()
        def read_lines():
            for line in child.stdout:
                lines.put(line)
        reader = threading.Thread(target=read_lines, daemon=True); reader.start()
        try:
            ready = json.loads(lines.get(timeout=5)); assert ready['ready']
            address = (host, ready['port']) if family==4 else (host, ready['port'], 0, 0)
            peer = Peer(sock, address, responder, typed=True); peer.handshake()
            assert json.loads(lines.get(timeout=3))['app_done'], 'native application confirmation fence'
            mutants = 0
            if payload is not None:
                peer.raw(peer.protect(payload))
            else:
                if name=='unauthenticated': mutants = unauthenticated(peer)
                else: deferred(peer, responder, name=='stream-before-open')
                peer.reliable(frame(1))  # Independent valid service after the rejection.
                peer.raw(peer.protect(frame(9, struct.pack('!I', 0))))
            code = child.wait(timeout=6); reader.join(1); error = child.stderr.read()
            assert code==0, (name, code, error)
            result = json.loads(lines.get(timeout=1))
            assert result['close_reason']==reason and result['joined']
            assert result['confirmed']==1 and result['closes']==1
            assert result['read']==result['opened']==result['games']==0, ('partial packet delivery', result)
            if mutants: assert result['invalid']>0
            result.update(case=name, family=family, python_role='responder' if responder else 'initiator',
                          backend=('iocp' if os.name=='nt' else 'epoll') if backend else 'select',
                          unauthenticated_mutations=mutants)
            print(json.dumps(result), flush=True)
        finally:
            if child.poll() is None:
                child.terminate(); child.wait(timeout=5)
            reader.join(1); child.stdout.close(); child.stderr.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--reuse', action='store_true')
    parser.add_argument('--roles', choices=('both','initiator','responder'), default='both')
    parser.add_argument('--families', choices=('both','4','6'), default='both')
    parser.add_argument('--backends', choices=('both','select','native'), default='both')
    parser.add_argument('--suite', choices=('all','malformed','unauthenticated','deferred'), default='all')
    parser.add_argument('--case', help='one malformed-case name; includes whole-packet prefix/suffix variants')
    args = parser.parse_args()
    if args.case and args.case not in malformed_cases(False): parser.error('unknown malformed case')
    binary = build(args)
    roles = (False, True) if args.roles=='both' else (args.roles=='responder',)
    families = (4, 6) if args.families=='both' else (int(args.families),)
    backends = (False, True) if args.backends=='both' else (args.backends=='native',)
    for backend in backends:
        for responder in roles:
            for family in families:
                if args.suite in ('all','unauthenticated') and not args.case:
                    run_case(binary, family, responder, backend, 'unauthenticated')
                if args.suite in ('all','deferred') and not args.case:
                    for name in ('stream-before-open', 'future-open-gap'):
                        run_case(binary, family, responder, backend, name)
                if args.suite not in ('all','malformed') and not args.case: continue
                for name, bad in malformed_cases(responder).items():
                    if args.case and args.case!=name: continue
                    if not bad:
                        run_case(binary, family, responder, backend, name, bad)
                    else:
                        for suffix in (False, True):
                            payload = bad+prefix(responder) if suffix else prefix(responder)+bad
                            run_case(binary, family, responder, backend, name+('-valid-suffix' if suffix else '-valid-prefix'), payload)
    return 0


if __name__=='__main__': raise SystemExit(main())
