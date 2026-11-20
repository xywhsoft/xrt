"""Exercise fixed-arrival TCP reply verification across arbitrary reads."""
from collections import deque
import multiprocessing
import time
import unittest
from unittest.mock import Mock, patch

from run_tcp_worker import SaturationPeer, receive_shard, retire_shard


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


if __name__ == "__main__":
    unittest.main()
