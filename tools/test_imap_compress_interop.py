"""Run the public IMAP client against an independent Python/zlib server."""
from __future__ import annotations

import argparse
import base64
import datetime
import hashlib
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import zlib

import build as xrt_build
from test_mail_tls_interop import certificates, upgrade
from xrt_manifest import expand_manifest_paths

ROOT = Path(__file__).resolve().parents[1]
PROBE = 'tools/probes/imap_compress_client.c'
CASES = ('stored', 'fixed', 'dynamic', 'full-flush', 'window-9', 'coalesced', 'hrr',
         'fragmented', 'reject-no', 'reject-bad', 'invalid', 'wrapped', 'truncated', 'line-limit',
         'close-truncated', 'close-stall')
MODES = ('plain', 'tls', 'starttls')
LAYOUTS = ('modular', 'single')


def payload() -> bytes:
    data = bytearray()
    value = 0x7351A2B9
    for _ in range(40000):
        value = (value ^ (value << 13)) & 0xFFFFFFFF
        value ^= value >> 17
        value = (value ^ (value << 5)) & 0xFFFFFFFF
        data.append(32 + value % 95)
    return bytes(data)


PAYLOAD = payload()


class Reader:
    def __init__(self, peer: socket.socket):
        self.peer = peer
        self.pending = bytearray()
        self.decoder = None
        self.encoded_bytes = 0
        self.decoded_bytes = 0

    def fill(self):
        block = self.peer.recv(8192)
        if not block:
            raise AssertionError('client closed before expected command or literal')
        if self.decoder is not None:
            self.encoded_bytes += len(block)
            block = self.decoder.decompress(block, 65537)
            if self.decoder.eof or self.decoder.unused_data or self.decoder.unconsumed_tail:
                raise AssertionError('client ended, restarted or exceeded fixture DEFLATE stream bound')
            self.decoded_bytes += len(block)
        self.pending.extend(block)
        if len(self.pending) > 65536:
            raise AssertionError('decoded client input exceeded fixture bound')

    def line(self) -> bytes:
        while b'\r\n' not in self.pending:
            if len(self.pending) > 8192:
                raise AssertionError('client command exceeded fixture line bound')
            self.fill()
        index = self.pending.index(b'\r\n')
        value = bytes(self.pending[:index])
        del self.pending[:index + 2]
        return value

    def exact(self, size: int) -> bytes:
        while len(self.pending) < size:
            self.fill()
        value = bytes(self.pending[:size])
        del self.pending[:size]
        return value

    def command(self, expected: bytes) -> bytes:
        tag, separator, command = self.line().partition(b' ')
        if not separator or not tag or command != expected:
            raise AssertionError(f'expected {expected!r}, got {tag!r} {command!r}')
        return tag

    def closed(self):
        if self.pending:
            raise AssertionError('unexpected queued client input after failure')
        try:
            extra = self.peer.recv(1)
        except (ssl.SSLEOFError, ConnectionResetError):
            return
        if extra:
            raise AssertionError('client sent another command after failure')


class Writer:
    def __init__(self, peer: socket.socket, scenario: str):
        self.peer = peer
        self.scenario = scenario
        self.encoder = None
        self.first_block_type = None
        self.encoded_bytes = 0
        self.last_write_bytes = 0

    def raw(self, data: bytes):
        self.last_write_bytes = 0
        if self.scenario == 'fragmented':
            for value in data:
                self.peer.sendall(bytes((value,)))
                self.last_write_bytes += 1
        else:
            self.peer.sendall(data)
            self.last_write_bytes = len(data)

    def encode(self, data: bytes) -> bytes:
        flush = zlib.Z_FULL_FLUSH if self.scenario == 'full-flush' else zlib.Z_SYNC_FLUSH
        block = self.encoder.compress(data) + self.encoder.flush(flush)
        if self.first_block_type is None and block:
            self.first_block_type = (block[0] >> 1) & 3
        self.encoded_bytes += len(block)
        return block

    def send(self, data: bytes):
        self.raw(self.encode(data) if self.encoder is not None else data)

    def start(self):
        self.encoder = zlib.compressobj(
            level=0 if self.scenario == 'stored' else 9,
            wbits=-9 if self.scenario == 'window-9' else -15,
            strategy=zlib.Z_FIXED if self.scenario == 'fixed' else zlib.Z_DEFAULT_STRATEGY)


def conversation(peer: socket.socket, scenario: str, greeting: bool, record: dict,
                 client_done: threading.Event):
    reader, writer = Reader(peer), Writer(peer, scenario)
    if greeting:
        writer.send(b'* OK localhost ready\r\n')
    tag = reader.command(b'CAPABILITY')
    writer.send(b'* CAPABILITY IMAP4rev1 AUTH=PLAIN cOmPrEsS=dEfLaTe\r\n' + tag + b' OK capability complete\r\n')
    tag = reader.command(b'AUTHENTICATE PLAIN')
    writer.send(b'+ \r\n')
    if base64.b64decode(reader.line(), validate=True) != b'\0demo\0secret':
        raise AssertionError('fixture AUTH PLAIN bytes mismatch')
    writer.send(tag + b' OK authenticated\r\n')
    tag = reader.command(b'COMPRESS DEFLATE')
    if reader.pending:
        raise AssertionError('client pipelined a command before COMPRESS confirmation')
    rejected = scenario.startswith('reject-')
    if rejected:
        writer.send(tag + (b' NO [COMPRESSIONACTIVE] compression refused\r\n' if scenario == 'reject-no'
                           else b' BAD compression refused\r\n'))
    else:
        writer.start()
        reader.decoder = zlib.decompressobj(-15)
        prefix = writer.encode(b'* OK compressed prefix ready\r\n') if scenario == 'coalesced' else b''
        writer.raw(tag + b' OK compression active\r\n' + prefix)
    tag = reader.command(b'NOOP')
    fault = scenario in ('invalid', 'wrapped', 'truncated', 'line-limit')
    if fault:
        if scenario == 'invalid':
            writer.raw(b'\x07')
        elif scenario == 'wrapped':
            wrapped = zlib.compressobj(wbits=15)
            writer.raw(wrapped.compress(tag + b' OK illegal wrapper\r\n') + wrapped.flush(zlib.Z_SYNC_FLUSH))
        elif scenario == 'line-limit':
            writer.send(b'* OK ' + b'x' * 131072 + b'\r\n')
        else:
            writer.send(b'* OK partial response\r\n' + tag + b' OK incomplete')
            peer.close()
            record['server_initiated_truncated_close'] = True
            return
        reader.closed()
        record['peer_closed_without_another_command'] = True
    else:
        extra = b'* OK ' + PAYLOAD[:800] + b'\r\n' if scenario == 'dynamic' else b''
        writer.send(extra + tag + b' OK noop complete\r\n')
        if scenario in ('stored', 'fixed', 'dynamic'):
            expected = {'stored': 0, 'fixed': 1, 'dynamic': 2}[scenario]
            if writer.first_block_type != expected:
                raise AssertionError(f'fixture did not generate intended DEFLATE block type {expected}')
        tag = reader.command(b'EXAMINE "INBOX"')
        writer.send(b'* 2 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 42] valid\r\n' +
                    tag + b' OK [READ-ONLY] examined\r\n')
        tag = reader.command(b'APPEND "INBOX" {40000}')
        writer.send(b'+ send literal\r\n')
        if reader.exact(40002) != PAYLOAD + b'\r\n':
            raise AssertionError('independent zlib decoder found different literal bytes')
        writer.send(tag + b' OK appended\r\n')
        for _ in range(3):
            tag = reader.command(b'NOOP')
            writer.send(tag + b' OK continued stream\r\n')
        tag = reader.command(b'LOGOUT')
        reply = b'* BYE signing off\r\n' + tag + b' OK logout complete\r\n'
        final_block = writer.encode(reply) if writer.encoder is not None else reply
        try:
            writer.raw(final_block)
        except (ConnectionResetError, BrokenPipeError):
            # Plain TCP may close once the complete tagged reply is decoded,
            # before the final empty sync-flush block is delivered. The client
            # independently asserts the exact final OK text; all earlier reset
            # failures and TLS shutdown failures remain fatal.
            remaining = len(final_block) - writer.last_write_bytes
            if (isinstance(peer, ssl.SSLSocket) or scenario != 'fragmented' or
                    remaining > 5 or not final_block.endswith(b'\0\0\xff\xff')):
                raise
            record['plain_close_during_empty_flush_tail'] = remaining
        record['literal_bytes_verified'] = 40000
        if scenario == 'close-truncated':
            peer.close()
            record['server_initiated_truncated_close_after_tagged_ok'] = True
        elif scenario == 'close-stall':
            # Keep the TLS peer open without replying to close_notify until the
            # public client reports its configured close timeout.
            if not client_done.wait(timeout=20):
                raise AssertionError('client did not bound authenticated close wait')
            record['server_withheld_close_notify_until_client_returned'] = True
        elif isinstance(peer, ssl.SSLSocket):
            peer.unwrap().close()
            record['authenticated_server_shutdown'] = True
        else:
            reader.closed()
    record.update(client_encoded_bytes=reader.encoded_bytes, client_decoded_bytes=reader.decoded_bytes,
                  server_encoded_bytes=writer.encoded_bytes, first_server_block_type=writer.first_block_type)


def build_clients(directory: Path, compiler_name: str, sanitize: bool, layouts: list[str]):
    compiler = xrt_build._compiler(compiler_name)
    platform = 'windows' if os.name == 'nt' else 'linux'
    overlays = expand_manifest_paths([ROOT / 'extlibs/ximap/config/modules.json'])
    sources, _, _, _, defines, links, _, _, includes, headers = xrt_build._load_modules(
        'imap_client_example,imap_append', overlays, platform=platform)
    sanitizer = ['-fno-omit-frame-pointer', '-fsanitize=address,undefined'] if sanitize else []
    flags = ['-std=c11', '-D_GNU_SOURCE=', '-O1', '-g', *sanitizer]
    for name in includes:
        flags.extend(['-I', str(ROOT / name)])
    result = {}
    for layout in layouts:
        output = directory / layout / ('imap_compress_client' + ('.exe' if os.name == 'nt' else ''))
        output.parent.mkdir(parents=True, exist_ok=True)
        objects = xrt_build._compile_objects(compiler, 'native', sources, defines, output.parent / 'obj',
                                            False, flags, headers, platform) if layout == 'modular' else []
        xrt_build._compile_program(compiler, 'native', PROBE, objects, defines if layout == 'modular' else [],
                                   links, output, flags + (['-DXRT_TEST_IMAP_SINGLE'] if layout == 'single' else []),
                                   sanitizer, platform)
        result[layout] = output
    return result


def run_case(client: Path, scenario: str, mode: str, ca: Path, leaf: Path, key: Path):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(25)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(leaf), str(key))
    handshake = {'client_hellos': 0, 'hello_retry_requests': 0}
    if scenario == 'hrr':
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
        context.set_ecdh_curve('prime256v1')
        def message(_sock, direction, _version, content_type, message_type, data):
            if int(content_type) == 22 and int(message_type) == 1 and direction == 'read':
                handshake['client_hellos'] += 1
            if (int(content_type) == 22 and int(message_type) == 2 and direction == 'write' and
                    data[6:38].hex() == 'cf21ad74e59a6111be1d8c021e65b891c2a211167abb8c5e079e09e2c8a8339c'):
                handshake['hello_retry_requests'] += 1
        context._msg_callback = message
    sni = []
    context.set_servername_callback(lambda _sock, name, _context: sni.append(name))
    errors, record = [], {'scenario': scenario, 'mode': mode}
    client_done = threading.Event()

    def serve():
        try:
            with listener.accept()[0] as raw:
                raw.settimeout(20)
                raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                if mode == 'plain':
                    conversation(raw, scenario, True, record, client_done)
                else:
                    with (context.wrap_socket(raw, server_side=True) if mode == 'tls' else
                          upgrade(raw, context, 'imap')) as peer:
                        conversation(peer, scenario, mode == 'tls', record, client_done)
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        completed = subprocess.run([str(client), 'localhost', str(listener.getsockname()[1]), str(ca), mode, scenario],
                                   cwd=ROOT, capture_output=True, text=True, timeout=30)
    finally:
        client_done.set()
        worker.join(timeout=25)
        listener.close()
    expected_sni = [] if mode == 'plain' else ['localhost'] * (2 if scenario == 'hrr' else 1)
    if worker.is_alive() or errors or completed.returncode != 0 or sni != expected_sni:
        raise AssertionError(f'{client.parent.name} {mode}/{scenario}: exit={completed.returncode}, '
                             f'out={completed.stdout!r}, err={completed.stderr!r}, server={errors!r}, SNI={sni!r}')
    expected = f'[PASS] independent IMAP {mode}/{scenario}'
    if expected not in completed.stdout:
        raise AssertionError('client did not report completed public API assertions')
    if scenario == 'hrr' and handshake != {'client_hellos': 2, 'hello_retry_requests': 1}:
        raise AssertionError(f'fixture did not complete a real HelloRetryRequest: {handshake}')
    if scenario == 'hrr':
        record['verified_handshake_messages'] = handshake
    record['layout'] = client.parent.name
    record['client_exit'] = completed.returncode
    print(f'[PASS] zlib {client.parent.name} {mode}/{scenario}', flush=True)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--case', action='append', choices=CASES)
    parser.add_argument('--mode', action='append', choices=MODES)
    parser.add_argument('--layout', action='append', choices=LAYOUTS)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if args.sanitize and os.name == 'nt':
        parser.error('sanitizer execution requires a POSIX host')
    selected = [(mode, scenario) for mode in args.mode or MODES for scenario in args.case or CASES
                if mode != 'plain' or scenario not in ('hrr', 'close-truncated', 'close-stall')]
    if not selected:
        parser.error('selected modes and cases have no applicable IMAP interoperability scenarios')
    platform = 'windows' if os.name == 'nt' else 'linux'
    output = (args.output_dir or ROOT / f'out/imap-compress-interop/{platform}/{Path(args.compiler).stem}' /
              ('sanitizer' if args.sanitize else 'normal')).resolve()
    output.mkdir(parents=True, exist_ok=True)
    execution = output / 'execution.json'
    execution.unlink(missing_ok=True)
    if args.sanitize:
        os.environ['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
        os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    clients = build_clients(output, args.compiler, args.sanitize, args.layout or list(LAYOUTS))
    results = []
    with tempfile.TemporaryDirectory(prefix='imap-zlib-interop-') as temporary:
        ca, leaf, _ip_leaf, key = certificates(Path(temporary))
        for client in clients.values():
            for mode, scenario in selected:
                results.append(run_case(client, scenario, mode, ca, leaf, key))
    record = {'schema': 1, 'actual_runtime_root': str(ROOT), 'platform': platform, 'sanitize': args.sanitize,
              'compiler': args.compiler, 'zlib_runtime_version': zlib.ZLIB_RUNTIME_VERSION,
              'python_ssl_version': ssl.OPENSSL_VERSION, 'finished_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'cases': results, 'binary_sha256': {str(p.relative_to(ROOT) if p.is_relative_to(ROOT) else p): hashlib.sha256(p.read_bytes()).hexdigest()
                                                for p in clients.values()}}
    execution.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(f'[verified] independent zlib interoperability: {len(results)} public API cases', flush=True)


if __name__ == '__main__':
    main()
