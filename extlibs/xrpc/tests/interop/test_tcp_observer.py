"""Exercise fixed-arrival TCP reply verification across arbitrary reads."""
from collections import deque
import unittest
from unittest.mock import patch

from run_tcp_worker import SaturationPeer


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


if __name__ == "__main__":
    unittest.main()
