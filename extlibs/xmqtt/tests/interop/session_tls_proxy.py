"""Bounded test-only TLS terminator for the independent MQTT broker proxy.

Each SSL connection has one IO owner for both directions. Relay threads never
concurrently call into the same OpenSSL SSL object. No broker/product TLS code
is imported. Only complete MQTT frames, or one explicitly truncated prefix,
reach the broker. TLS protects the client-to-proxy hop; backend is local TCP.
"""
from collections import deque
import hashlib
import select
import socket
import ssl

from run_session_broker import Proxy, varint


LIMIT = 2 * 1024 * 1024


def extract(data):
    if len(data) < 2:
        return None
    size = 0
    for i in range(4):
        if len(data) <= i + 1:
            return None
        b = data[i + 1]
        size |= (b & 127) << (7 * i)
        if not b & 128:
            if size + i + 2 > LIMIT:
                raise ValueError('TLS proxy MQTT frame cap')
            return i + 2, size
    raise ValueError('TLS proxy overlong Remaining Length')


def metadata(raw, body, generation, direction, total=None):
    kind = raw[0] >> 4
    row = dict(generation=generation, direction=direction, type=kind,
               flags=raw[0] & 15, bytes=total or len(raw), forwarded=True)
    if kind == 2:
        if len(body) < 2:
            raise ValueError('TLS proxy CONNACK bounds')
        row.update(session_present=bool(body[0] & 1), reason=body[1])
    elif kind == 3:
        if len(body) < 2:
            raise ValueError('TLS proxy PUBLISH bounds')
        n = int.from_bytes(body[:2], 'big')
        at = 2 + n
        if at > len(body):
            raise ValueError('TLS proxy topic bounds')
        row['topic_bytes'] = n
        if n:
            row['topic'] = body[2:at].decode('utf-8')
        if (raw[0] >> 1) & 3:
            if at + 2 > len(body):
                raise ValueError('TLS proxy identifier bounds')
            row['identifier'] = int.from_bytes(body[at:at + 2], 'big')
            at += 2
        return row, at
    elif kind in [4, 5, 6, 7, 8, 9, 10, 11]:
        if len(body) < 2:
            raise ValueError('TLS proxy control identifier bounds')
        row['identifier'] = int.from_bytes(body[:2], 'big')
        if kind in [4, 5, 6, 7]:
            row['reason'] = body[2] if len(body) >= 3 else 0
    return row, None


class TLSProxy(Proxy):
    def __init__(self, backend, version, mode, host, context_factory, tls,
                 mutual=False, resume=0, prefix=False):
        self.context_factory = context_factory
        self.context = context_factory()
        self.contexts = [self.context]
        self.tls, self.mutual, self.resume, self.prefix = tls, mutual, resume, prefix
        self.sni = []
        self.observations = dict(handshakes=[], closes=[], prefix=prefix,
                                 frame_cap=LIMIT, input_cap_per_side=LIMIT,
                                 queue_cap_per_side=LIMIT, queue_frames=32,
                                 ssl_io_owner='one bridge thread per connection')
        super().__init__(backend, version, mode, host)

    def accept(self):
        generation = 0
        while not self.stop.is_set():
            try:
                raw, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self.connections.append(raw)
            raw.settimeout(20)
            if self.prefix and not generation:
                raw.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            try:
                if not raw.recv(1, socket.MSG_PEEK):
                    raw.close()  # Native TCP readiness is not a TLS generation.
                    continue
                generation += 1
                if generation > 2:
                    raise ValueError('TLS connection cap')
                context = self.context_factory() if generation == 2 and self.resume == 2 else self.context
                if context not in self.contexts:
                    self.contexts.append(context)
                context.set_servername_callback(lambda _s, name, _c: self.sni.append(name))
                front = context.wrap_socket(raw, server_side=True, do_handshake_on_connect=False)
                self.connections.append(front)
                front.do_handshake()
                cert = front.getpeercert(binary_form=True)
                info = dict(generation=generation, tls=front.version(),
                            cipher=front.cipher()[0], alpn=front.selected_alpn_protocol(),
                            resumed=front.session_reused, sni=self.sni[-1],
                            client_certificate_sha256=hashlib.sha256(cert).hexdigest() if cert else None)
                self.observations['handshakes'].append(info)
                assert info['tls'] == ('TLSv1.2' if self.tls == 12 else 'TLSv1.3')
                assert info['cipher'] == ('ECDHE-RSA-AES128-GCM-SHA256' if self.tls == 12 else 'TLS_AES_128_GCM_SHA256')
                assert info['alpn'] == 'mqtt' and info['sni'] == 'localhost'
                assert bool(cert) == self.mutual
                assert info['resumed'] == (generation == 2 and self.resume == 1)
                back = socket.create_connection(('127.0.0.1', self.backend), timeout=10)
                self.connections.append(back)
                front.setblocking(False)
                back.setblocking(False)
                import threading
                thread = threading.Thread(target=self.bridge, args=(front, back, generation), daemon=True)
                self.threads.append(thread)
                thread.start()
            except Exception as exc:
                if not self.stop.is_set():
                    self.error('TLS accept: ' + repr(exc))
                raw.close()
                break

    def record(self, row):
        with self.cv:
            if len(self.rows) >= 200:
                raise ValueError('TLS proxy trace cap')
            row['order'] = len(self.rows)
            self.rows.append(row)
            if not row['forwarded']:
                self.dropped = True
            self.cv.notify_all()

    def describe(self, raw, body, generation, direction, total=None):
        row, at = metadata(raw, body, generation, direction, total)
        if row['type'] == 3 and self.version == 5:
            n, start = varint(body, at)
            if start + n > len(body):
                raise ValueError('TLS proxy property bounds')
            row['property_hex'] = body[start:start + n].hex()
        return row

    def bridge(self, front, back, generation):
        sockets = [front, back]
        inputs = [bytearray(), bytearray()]
        outputs = [deque(), deque()]
        queued = [0, 0]
        eof = [False, False]
        discard, discarded, want_write = False, 0, False
        try:
            while not self.stop.is_set():
                if (eof[0] and not outputs[1]) or (eof[1] and not outputs[0]):
                    break
                reads = [s for i, s in enumerate(sockets) if not eof[i] and len(inputs[i]) < LIMIT and queued[1 - i] < LIMIT]
                writes = [s for i, s in enumerate(sockets) if outputs[i] or (i == 0 and want_write)]
                pending = front in reads and front.pending()
                readable, writable, _ = select.select(reads, writes, [], 0 if pending else .05)
                if pending and front not in readable:
                    readable.append(front)
                for target in writable:
                    at = sockets.index(target)
                    if not outputs[at]:
                        continue
                    raw, row, offset = outputs[at][0]
                    try:
                        n = target.send(memoryview(raw)[offset:])
                    except (ssl.SSLWantReadError, ssl.SSLWantWriteError, BlockingIOError):
                        continue
                    if not n:
                        raise EOFError('TLS proxy zero send')
                    offset += n
                    queued[at] -= n
                    if offset == len(raw):
                        outputs[at].popleft()
                        self.record(row)
                    else:
                        outputs[at][0] = raw, row, offset
                for source in readable:
                    at = sockets.index(source)
                    try:
                        data = source.recv(min(65536, LIMIT - len(inputs[at])))
                        want_write = False
                    except ssl.SSLWantWriteError:
                        want_write = True
                        continue
                    except (ssl.SSLWantReadError, BlockingIOError):
                        continue
                    if not data:
                        if inputs[at]:
                            raise ValueError('TLS proxy truncated MQTT frame')
                        eof[at] = True
                        continue
                    if at == 0 and discard:
                        discarded += len(data)
                        if discarded > LIMIT:
                            raise ValueError('TLS prefix discarded-byte cap')
                        continue
                    inputs[at].extend(data)
                    while inputs[at]:
                        header = extract(inputs[at])
                        if not header:
                            break
                        h, n = header
                        total = h + n
                        direction = 'cs' if at == 0 else 'sc'
                        prefix = self.prefix and generation == 1 and at == 0 and inputs[at][0] >> 4 == 3 and not self.dropped
                        take = 64 if prefix else total
                        if len(inputs[at]) < take:
                            break
                        raw = bytes(inputs[at][:take])
                        body = raw[h:]
                        del inputs[at][:take]
                        row = self.describe(raw, body, generation, direction, total)
                        target = 1 - at
                        if prefix:
                            assert total > take
                            row.update(forwarded=False, fault='authenticated_publish_prefix', prefix_bytes=take)
                            discard = True
                            discarded += len(inputs[at])
                            inputs[at].clear()
                        drop_direction, drop_type = [('sc', 4), ('sc', 5), ('sc', 7), ('cs', 5), ('cs', 7)][self.mode]
                        if not self.prefix and generation == 1 and direction == drop_direction and row['type'] == drop_type and not self.dropped:
                            row['forwarded'] = False
                            self.record(row)
                            continue
                        if queued[target] + len(raw) > LIMIT or len(outputs[target]) >= 32:
                            raise ValueError('TLS proxy output queue cap')
                        outputs[target].append((raw, row, 0))
                        queued[target] += len(raw)
                        if prefix:
                            break
        except (ConnectionResetError, BrokenPipeError):
            pass
        except Exception as exc:
            if not self.stop.is_set():
                self.error('TLS bridge: ' + repr(exc))
        finally:
            close = dict(generation=generation, discarded=discarded, notify=False)
            if not self.stop.is_set():
                try:
                    front.settimeout(1)
                    plain = front.unwrap()
                    plain.close()
                    close['notify'] = True
                except (OSError, ValueError) as exc:
                    close['close_error'] = type(exc).__name__
            self.observations['closes'].append(close)
            for sock in sockets:
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                sock.close()

    def verify(self):
        super().verify()
        assert len(self.observations['handshakes']) == 2
        if self.prefix:
            cut = [r for r in self.rows if not r['forwarded']]
            assert len(cut) == 1 and cut[0]['fault'] == 'authenticated_publish_prefix'
            assert cut[0]['prefix_bytes'] == 64 < cut[0]['bytes']

    def close(self):
        try:
            super().close()
        finally:
            for context in self.contexts:
                context.set_servername_callback(None)
            self.contexts.clear()
            self.context = self.context_factory = None
