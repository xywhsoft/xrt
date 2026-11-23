"""An independent Python peer drives the APP-owned line framing sample."""
import json
from pathlib import Path
import subprocess
import sys


def verify_record_limit(executable):
    request = b'{"jsonrpc":"2.0","id":91,"method":"echo","params":[42]}'
    complete = request.ljust(4096, b" ") + b"\n"
    expected = {"jsonrpc": "2.0", "id": 91, "result": [42]}
    command = [str(Path(executable).resolve()), "--serve"]
    accepted = subprocess.run(command, input=complete, capture_output=True, timeout=10)
    assert accepted.returncode == 0 and json.loads(accepted.stdout) == expected, accepted

    oversized = request.ljust(4097, b" ") + b"\n"
    rejected = subprocess.run(command, input=complete + oversized + complete,
                              capture_output=True, timeout=10)
    assert rejected.returncode == 2 and json.loads(rejected.stdout) == expected, rejected

    partial = subprocess.run(command, input=complete[:-1], capture_output=True, timeout=10)
    assert partial.returncode == 2 and not partial.stdout, partial


def verify_byte_records(executable):
    command = [str(Path(executable).resolve()), "--serve"]
    good = br'{"jsonrpc":"2.0","id":17,"method":"echo","params":["a\u0000b"]}' + b"\n"
    malformed = (b'{"jsonrpc":"2.0","method":"echo\x00","id":13}\n', b"\x1a\n")
    result = subprocess.run(command, input=b"".join(malformed) + good, capture_output=True, timeout=10)
    assert result.returncode == 0, result
    parse_error = {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Parse error"}}
    assert [json.loads(line) for line in result.stdout.splitlines()] == [
        parse_error, parse_error, {"jsonrpc": "2.0", "id": 17, "result": ["a\x00b"]},
    ], result
    assert result.stdout.count(b"\n") == 3 and b"\r" not in result.stdout, result

    request = good[:-1]
    exact = request.ljust(4095, b" ") + b"\r\n"
    oversized = request.ljust(4096, b" ") + b"\r\n"
    result = subprocess.run(command, input=exact + oversized + good, capture_output=True, timeout=10)
    assert result.returncode == 2 and json.loads(result.stdout) == {
        "jsonrpc": "2.0", "id": 17, "result": ["a\x00b"],
    }, result


def main():
    executable, report = sys.argv[1:]
    call = {"jsonrpc": "2.0", "method": "echo", "id": "app-1", "params": [42]}
    escaped = r'{"jsonrpc":"2.0","method":"\u0065\u0063\u0068\u006f","id":"app-2","params":[43]}'
    notification = {"jsonrpc": "2.0", "method": "echo"}
    batch = [
        {"jsonrpc": "2.0", "method": "echo", "id": 7, "params": [1, 2]},
        notification,
        {"jsonrpc": "2.0", "method": "missing", "id": 9},
    ]
    data = "\n".join([json.dumps(call), escaped, json.dumps(notification),
                      json.dumps(batch), "[", ""])
    result = subprocess.run([str(Path(executable).resolve()), "--serve"],
                            input=data.encode(), capture_output=True, timeout=15, check=True)
    response, escaped_reply, grouped, malformed = [json.loads(line) for line in result.stdout.splitlines()]
    assert response == {"jsonrpc": "2.0", "id": "app-1", "result": [42]}
    assert escaped_reply == {"jsonrpc": "2.0", "id": "app-2", "result": [43]}
    assert len(grouped) == 2 and grouped[0]["id"] == 7 and grouped[0]["result"] == [1, 2]
    assert grouped[1]["id"] == 9 and grouped[1]["error"]["code"] == -32601
    assert malformed["id"] is None and malformed["error"]["code"] == -32700
    verify_record_limit(executable)
    verify_byte_records(executable)
    out = Path(report)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"cases": 10, "replies": 10, "escaped_method": True, "ok": True, "max_record_bytes": 4096,
                               "oversized_record_rejected": True, "unterminated_record_rejected": True,
                               "byte_records_verified": True}) + "\n",
                   encoding="utf-8")
    print("[pass] APP stdio call, notification, batch, parse error and record limits")


if __name__ == "__main__":
    main()
