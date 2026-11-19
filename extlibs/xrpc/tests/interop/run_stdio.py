"""An independent Python peer drives the APP-owned line framing sample."""
import json
from pathlib import Path
import subprocess
import sys

def main():
    executable, report = sys.argv[1:]
    call = {"jsonrpc": "2.0", "method": "echo", "id": "app-1", "params": [42]}
    notification = {"jsonrpc": "2.0", "method": "echo"}
    batch = [
        {"jsonrpc": "2.0", "method": "echo", "id": 7, "params": [1, 2]},
        notification,
        {"jsonrpc": "2.0", "method": "missing", "id": 9},
    ]
    data = "\n".join([json.dumps(call), json.dumps(notification),
                      json.dumps(batch), "[", ""])
    result = subprocess.run([str(Path(executable).resolve()), "--serve"],
                            input=data.encode(), capture_output=True, timeout=15, check=True)
    response, grouped, malformed = [json.loads(line) for line in result.stdout.splitlines()]
    assert response == {"jsonrpc": "2.0", "id": "app-1", "result": [42]}
    assert len(grouped) == 2 and grouped[0]["id"] == 7 and grouped[0]["result"] == [1, 2]
    assert grouped[1]["id"] == 9 and grouped[1]["error"]["code"] == -32601
    assert malformed["id"] is None and malformed["error"]["code"] == -32700
    out = Path(report)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"cases": 4, "replies": 3, "ok": True}) + "\n", encoding="utf-8")
    print("[pass] APP stdio call, notification, batch and parse-error exchange")

if __name__ == "__main__":
    main()
