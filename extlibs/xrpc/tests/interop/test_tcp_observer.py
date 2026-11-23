"""Exercise fixed-arrival TCP reply verification across arbitrary reads."""
from collections import deque
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
import multiprocessing
import os
import socket
import time
import unittest
from unittest.mock import Mock, patch

from run_tcp_worker import (SATURATION_DEPTH, SATURATION_WARMUP, SaturationPeer, process_affinity,
                            receive_shard, retire_shard, saturation_batches, saturation_sharded)


class Replies:
    def __init__(self, *chunks):
        self.chunks = deque(chunks)

    def recv(self, limit):
        chunk = self.chunks.popleft()
        assert len(chunk) <= limit
        return chunk


class ReplyStreamTest(unittest.TestCase):
    batches = [(b"request", [b"abc\n", b"defg\n"], 2),
               (b"batch", [b"[a,b]\n"], 2),
               (b"final", [b"last\n"], 1)]
    wire = b"abc\ndefg\n[a,b]\nlast\n"
    ends = (4, 9, 15, 15, 20)

    def peer(self, *chunks, offered=3):
        peer = SaturationPeer(Replies(*chunks), self.batches, 3)
        peer.sent_at = list(range(offered))
        return peer

    def test_every_two_split_positions(self):
        for first in range(1, len(self.wire)):
            for second in range(first + 1, len(self.wire)):
                with self.subTest(first=first, second=second):
                    peer = self.peer(self.wire[:first], self.wire[first:second], self.wire[second:], b"")
                    with patch("run_tcp_worker.time.perf_counter_ns", side_effect=(1, 2, 3, 4)):
                        self.assertTrue(peer.read())
                        self.assertEqual(peer.times, [1 for end in self.ends if end <= first])
                        self.assertEqual(peer.completed, sum(end <= first for end in (9, 15, 20)))
                        self.assertTrue(peer.read())
                        self.assertEqual(peer.completed, sum(end <= second for end in (9, 15, 20)))
                        self.assertTrue(peer.read())
                        self.assertFalse(peer.read())
                    self.assertEqual(peer.times, [1 if end <= first else 2 if end <= second else 3
                                                  for end in self.ends])
                    self.assertEqual(peer.completed, len(self.batches))

    def test_staggered_offer_keeps_its_phase_while_waiting_for_credit(self):
        sock = Mock()
        sock.send.side_effect = len
        sock.recv.return_value = b"abc\ndefg\n"
        peer = SaturationPeer(sock, self.batches, window=1, phase_ns=25)
        with patch("run_tcp_worker.time.perf_counter_ns", side_effect=(125, 237, 240)):
            self.assertEqual(peer.offer(100, 200, 2, 124, 1000), 125)
            sock.send.assert_not_called()
            self.assertEqual(peer.offer(100, 200, 2, 125, 1000), 225)
            self.assertIsNone(peer.offer(100, 200, 2, 228, 1000))
            self.assertEqual(peer.stalls, 1)
            self.assertTrue(peer.read())
            self.assertEqual(peer.offer(100, 200, 2, 240, 1000), 325)
        self.assertEqual(peer.sent_at, [125, 240])
        self.assertEqual(peer.credit_wait, [0, 12])
        self.assertEqual(peer.peak, 1)

    def test_changed_byte_in_any_reply(self):
        for offset in range(len(self.wire)):
            with self.subTest(offset=offset):
                peer = self.peer(self.wire[:offset] + b"!" + self.wire[offset + 1:])
                with self.assertRaisesRegex(RuntimeError, "identity or order mismatch"):
                    peer.read()

    def test_eof_before_the_last_byte(self):
        peer = self.peer(self.wire[:-1], b"")
        self.assertTrue(peer.read())
        self.assertEqual(peer.completed, 2)
        with self.assertRaisesRegex(RuntimeError, "identity or order mismatch"):
            peer.read()

    def test_reply_before_offer(self):
        peer = self.peer(self.wire[:10], offered=1)
        with self.assertRaisesRegex(RuntimeError, "identity or order mismatch"):
            peer.read()

    def test_reply_after_the_last_offer(self):
        peer = self.peer(self.wire + b"unexpected\n")
        with self.assertRaisesRegex(RuntimeError, "unexpected reply after the final offer"):
            peer.read()


class ShardShutdownTest(unittest.TestCase):
    def test_receive_reports_a_shard_failure(self):
        reader, writer = multiprocessing.Pipe()
        with reader, writer:
            writer.send(("failure", "reply identity mismatch"))
            with self.assertRaisesRegex(RuntimeError, "reply identity mismatch"):
                receive_shard(reader, "done")

    def test_receive_rejects_a_closed_shard(self):
        reader, writer = multiprocessing.Pipe()
        with reader:
            writer.close()
            with self.assertRaisesRegex(RuntimeError, "exited before ready"):
                receive_shard(reader, "ready")

    def test_retire_reaps_a_live_shard(self):
        context = multiprocessing.get_context("spawn")
        worker = context.Process(target=time.sleep, args=(60,))
        worker.start()
        pid = worker.pid
        retire_shard(worker)
        self.assertFalse(any(child.pid == pid for child in multiprocessing.active_children()))


@unittest.skipUnless(os.environ.get("XRPC_TCP_OBSERVER_NATIVE"), "native observer library not selected")
class NativeObserverTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from tcp_native_observer import NativeObserver
        cls.observer = NativeObserver(os.environ["XRPC_TCP_OBSERVER_NATIVE"])

    def exchange(self, batches, reply_chunks):
        observer, responder = socket.socketpair()
        with observer, responder, ThreadPoolExecutor(max_workers=1) as pool:
            observer.setblocking(False)
            responder.settimeout(2)
            expected_request = b"".join(wire for wire, _, _ in batches)

            def reply():
                with responder.makefile("rb") as stream:
                    self.assertEqual(stream.read(), expected_request)
                for chunk in reply_chunks:
                    responder.sendall(chunk)
                responder.shutdown(socket.SHUT_WR)

            responding = pool.submit(reply)
            window = len(batches)
            peer = SaturationPeer(observer, batches, window)
            streams = {observer: peer}
            peers = self.observer.prepare(streams, sum(count for _, _, count in batches))
            start = time.perf_counter_ns()
            try:
                self.observer.drive(peers, window, 100000, start, start + 2_000_000_000, 2_000_000_000)
            finally:
                responding.result()
            self.observer.collect(peers, streams)
            return peer

    def test_checks_records_and_batch_in_one_reply_stream(self):
        peer = self.exchange(ReplyStreamTest.batches, (b"ab", b"c\ndefg\n[", b"a,b]\nlast", b"\n"))
        self.assertEqual(len(peer.times), 5)
        self.assertEqual(peer.times, sorted(peer.times))
        self.assertEqual(peer.times[2], peer.times[3])
        self.assertGreaterEqual(peer.times[0], peer.sent_at[-1])
        self.assertEqual(peer.peak, 3)

    def test_shards_preserve_peer_order_and_common_schedule(self):
        cpus = sorted(cpu for cpu in process_affinity() if cpu < 64)[:2]
        if len(cpus) < 2:
            self.skipTest("two available CPUs required")
        calls, connections, rate = 9, 3, 4000
        settings = dict(calls=calls, connections=connections, rate=rate, window=3,
                        batch_documents=False, stagger_arrivals=True)

        def reply(responder, batches):
            with responder.makefile("rb") as stream:
                self.assertEqual(stream.read(), b"".join(wire for wire, _, _ in batches))
            responder.sendall(b"".join(line for _, lines, _ in batches for line in lines))
            responder.shutdown(socket.SHUT_WR)

        with ExitStack() as sockets, ThreadPoolExecutor(max_workers=connections) as pool:
            observers, replies = [], []
            for index in range(connections):
                observer, responder = socket.socketpair()
                observers.append(sockets.enter_context(observer))
                sockets.enter_context(responder)
                observer.setblocking(False)
                responder.settimeout(5)
                first = index * (calls + SATURATION_WARMUP) + SATURATION_WARMUP + 1
                batches = list(saturation_batches(calls, SATURATION_DEPTH, first, False))
                replies.append(pool.submit(reply, responder, batches))

            results, start, cpu_seconds = saturation_sharded(
                observers, settings, cpus, lambda at: (at + 3_000_000_000, 3_000_000_000), self.observer.path)
            for worker in replies:
                worker.result()

        self.assertEqual(len(cpu_seconds), len(cpus))
        self.assertEqual(len(results), connections)
        for index, peer in enumerate(results):
            phase = index * SATURATION_DEPTH * 1_000_000_000 // (rate * connections)
            self.assertEqual(peer.phase_ns, phase)
            self.assertGreaterEqual(peer.sent_at[0], start + phase)
            self.assertEqual(len(peer.times), calls)
            self.assertEqual(peer.times, sorted(peer.times))

    def test_rejects_wrong_reply_and_premature_eof(self):
        for reply in (b"ab!", b"ab", b"abc\nextra"):
            with self.subTest(reply=reply):
                with self.assertRaisesRegex(RuntimeError, "reply or EOF mismatch"):
                    self.exchange([(b"request", [b"abc\n"], 1)], (reply,))


if __name__ == "__main__":
    unittest.main()
