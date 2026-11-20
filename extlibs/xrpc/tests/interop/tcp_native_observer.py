"""Binding for the optional native TCP saturation observer."""
import ctypes
from pathlib import Path
import time


class Group(ctypes.Structure):
    _fields_ = [
        ("wire", ctypes.c_char_p), ("reply", ctypes.c_char_p),
        ("wire_size", ctypes.c_size_t), ("reply_size", ctypes.c_size_t),
        ("ends", ctypes.c_size_t * 8), ("calls", ctypes.c_size_t),
    ]


class Peer(ctypes.Structure):
    _fields_ = [
        ("socket", ctypes.c_size_t), ("groups", ctypes.POINTER(Group)),
        ("count", ctypes.c_size_t), ("calls", ctypes.c_size_t), ("phase", ctypes.c_uint64),
        ("sent", ctypes.POINTER(ctypes.c_uint64)), ("credit_wait", ctypes.POINTER(ctypes.c_uint64)),
        ("received", ctypes.POINTER(ctypes.c_uint64)), ("peak", ctypes.c_size_t), ("stalls", ctypes.c_size_t),
    ]


class NativeObserver:
    def __init__(self, path):
        self.library = ctypes.CDLL(str(Path(path).resolve(strict=True)))
        now = self.library.xrpcTcpObserverNow
        now.argtypes, now.restype = [], ctypes.c_uint64
        before = time.perf_counter_ns()
        sample = now()
        after = time.perf_counter_ns()
        if not before - 1000 <= sample <= after + 1000:
            raise RuntimeError("native observer uses a different monotonic clock")
        self.observe = self.library.xrpcTcpObserve
        self.observe.argtypes = [ctypes.POINTER(Peer), ctypes.c_size_t, ctypes.c_size_t,
                                 ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64]
        self.observe.restype = ctypes.c_int

    def prepare(self, streams, calls):
        peers = []
        for stream in streams.values():
            groups = (Group * len(stream.batches))()
            for index, (wire, reply, ends) in enumerate(stream.batches):
                groups[index] = Group(wire, reply, len(wire), len(reply), (ctypes.c_size_t * 8)(*ends), len(ends))
            sent = (ctypes.c_uint64 * len(groups))()
            credit = (ctypes.c_uint64 * len(groups))()
            received = (ctypes.c_uint64 * calls)()
            peers.append(Peer(stream.sock.fileno(), groups, len(groups), calls, stream.phase_ns,
                              sent, credit, received, 0, 0))
        return (Peer * len(peers))(*peers)

    def drive(self, peers, window, rate, start, end, deadline):
        status = self.observe(peers, len(peers), window, rate, start, end, deadline)
        if status:
            detail = {1: "invalid plan", 2: "deadline", 3: "socket I/O", 4: "reply or EOF mismatch"}
            raise RuntimeError(f"native observer failed: {detail.get(status, status)}")

    @staticmethod
    def collect(peers, streams):
        for peer, stream in zip(peers, streams.values()):
            stream.sent_at = peer.sent[:peer.count]
            stream.credit_wait = peer.credit_wait[:peer.count]
            stream.times = peer.received[:peer.calls]
            stream.peak = peer.peak
            stream.stalls = peer.stalls
