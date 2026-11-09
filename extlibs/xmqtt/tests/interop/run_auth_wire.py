"""Independent stdlib byte peer, not an external broker/algorithm comparator."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, string, vbi


CLIENT = b"\x15" + string(b"external") + b"\x16" + string(b"a\0\xffz")
SERVER = b"\x15" + string(b"external") + b"\x16" + string(b"p\0\xfff")


def auth(reason, props=SERVER):
    return packet(0xf0, bytes([reason]) + vbi(len(props)) + props)


def connack(reason=0, props=SERVER):
    return packet(0x20, bytes([0, reason]) + vbi(len(props)) + props)


def expect_auth(s, reason):
    kind, body = read_packet(s)
    assert kind == 0xf0 and body == bytes([reason]) + vbi(len(CLIENT)) + CLIENT, (kind, body)


def closed(s, reason=None):
    try:
        kind, body = read_packet(s)
    except (EOFError, ConnectionResetError):
        return
    assert kind == 0xe0, (kind, body)
    if reason is not None:
        assert body == bytes([reason]), body
    try:
        assert not s.recv(1), "output after DISCONNECT"
    except ConnectionResetError:
        pass


def serve(listener, case, errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(6)
            kind, body = read_packet(s)
            props = CLIENT + (b"\x17\0" if case.startswith("problem_") else b"")
            keep = b"\0\x01" if case == "initial_keepalive" else b"\0\0"
            assert kind == 0x10 and body == string(b"MQTT") + b"\x05\x02" + keep + vbi(len(props)) + props + string(b"auth-probe"), body
            if case == "initial_eof":
                return
            if case == "initial_reject":
                s.sendall(connack(0x87, b""))
                closed(s)
                return
            bad = {"initial_bad_method": auth(0x18, SERVER.replace(b"external", b"changed!")),
                   "initial_missing_method": auth(0x18, b""), "initial_success": auth(0),
                   "initial_server_start": auth(0x19), "initial_early_pong": b"\xd0\0",
                   "problem_reason": auth(0x18, SERVER + b"\x1f" + string(b"forbidden"))}
            if case in bad:
                s.sendall(bad[case]);closed(s);return
            s.sendall(auth(0x18))
            if case == "initial_timeout":
                closed(s);return
            if case == "initial_keepalive":
                # Before successful CONNACK, server control output is AUTH only.
                # Defer PINGRESP while the application handles its challenge.
                assert read_packet(s) == (0xc0, b"")
                assert read_packet(s) == (0xc0, b"")
            expect_auth(s, 0x18)
            wire = connack(props=SERVER + (b"\x13\0\x01" if case == "verify_keepalive" else b""))
            if case == "initial_keepalive":
                wire += b"\xd0\0\xd0\0"
            if case == "coalesced":
                wire += packet(0x30, string(b"early") + b"\0seen")
            if case == "tiny":
                for byte in wire:
                    s.sendall(bytes([byte]))
            else:
                s.sendall(wire)
            if case == "local_reject_initial":
                closed(s);return
            if case == "verify_keepalive":
                assert read_packet(s) == (0xc0, b"")
                s.sendall(b"\xd0\0")
            expect_auth(s, 0x19)
            bad = {"reauth_bad_method": auth(0x18, SERVER.replace(b"external", b"changed!")),
                   "reauth_missing_method": auth(0, b""), "reauth_short_success": b"\xf0\0",
                   "reauth_server_start": auth(0x19), "reauth_disconnect": b"\xe0\x01\x87"}
            if case in bad:
                s.sendall(bad[case]);closed(s);return
            s.sendall(auth(0x18))
            if case == "reauth_timeout":
                closed(s);return
            expect_auth(s, 0x18)
            s.sendall(auth(0))
            if case == "local_reject_reauth":
                closed(s, 0x80);return
            kind, body = read_packet(s)
            assert kind == 0x30 and body == string(b"probe") + b"\0out", (kind, body)
            s.sendall(packet(kind, body));closed(s)
    except BaseException as error:
        errors.append(repr(error))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    cases = ["normal", "tiny", "coalesced", "initial_keepalive", "verify_keepalive", "problem_clean", "problem_reason",
             "initial_bad_method", "initial_missing_method", "initial_success", "initial_server_start", "initial_early_pong",
             "initial_reject", "initial_eof", "initial_timeout", "local_reject_initial",
             "reauth_bad_method", "reauth_missing_method", "reauth_short_success", "reauth_server_start",
             "reauth_disconnect", "reauth_timeout", "local_reject_reauth"]
    rows = []
    for case in cases:
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0));listener.listen(1);listener.settimeout(8)
            errors = [];thread = threading.Thread(target=serve, args=(listener, case, errors))
            thread.start()
            result = subprocess.run([str(args.probe.resolve()), str(listener.getsockname()[1]), case], capture_output=True, text=True, timeout=12)
            thread.join(8)
            row = dict(case=case, exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr, peer_errors=errors, peer_finished=not thread.is_alive())
            rows.append(row)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(dict(probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), cases=rows), indent=2)+"\n")
            assert result.returncode == 0 and not errors and not thread.is_alive(), row
            print("PASS auth", case, flush=True)
    print("Independent MQTT5 auth byte peer:", len(rows), "cases passed")


if __name__ == "__main__":
    main()
