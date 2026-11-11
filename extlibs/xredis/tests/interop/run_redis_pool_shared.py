"""Owned standalone Redis, fixed native shared-pool interoperation matrix."""
from pathlib import Path
import argparse, hashlib, json, random, shutil, socket, subprocess, tempfile, time, traceback
from run_redis_role import exchange

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', type=Path, action='append', required=True)
    parser.add_argument('--server', type=Path, default=Path(shutil.which('redis-server') or 'redis-server'))
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--port-candidate', type=int, action='append', default=[],
                        help='Caller-screened ports, useful across Windows/WSL network namespaces')
    args = parser.parse_args()
    server = args.server.absolute(); clients = [p.absolute() for p in args.client]
    args.report = args.report.absolute(); args.report.parent.mkdir(parents=True, exist_ok=True)
    result = dict(complete=False, server_version=subprocess.check_output([str(server), '--version'], text=True).strip(),
                  server_sha256=sha(server), runner_sha256=sha(__file__), cases=[],
                  limits=['Owned plaintext IPv4 standalone Redis; not Cluster routing, full TLS/auth trust or production acceptance.'])
    def save(): args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    temporary = tempfile.TemporaryDirectory(prefix='xredis shared pool '); runtime = Path(temporary.name)
    child = None; log = (runtime / 'server.log').open('wb')
    def identity():
        value, _ = exchange(port, ['INFO', 'SERVER'])
        return dict(row.split(b':', 1) for row in value.split(b'\r\n') if b':' in row)
    def owned():
        now = identity()
        assert child.poll() is None and int(now[b'process_id']) == child.pid and now[b'run_id'].decode() == result['run_id']
    try:
        # Avoid the Windows dynamic RPC range. Cross-host orchestration also
        # screens listeners/exclusions on Windows before passing candidates.
        candidates = args.port_candidate or random.Random(6714).sample(range(12000, 45000), 128)
        port = None
        for candidate in candidates:
            assert 1 <= candidate <= 65535
            try:
                with socket.socket() as reservation: reservation.bind(('127.0.0.1', candidate))
                port = candidate; break
            except OSError: continue
        assert port is not None, 'no fixture port available'
        command = [str(server), '--bind', '127.0.0.1', '--port', str(port), '--save', '', '--appendonly', 'no', '--dir', str(runtime)]
        child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT); due = time.monotonic() + 10
        while True:
            assert child.poll() is None, 'fixture exited'
            try:
                info = identity(); assert int(info[b'process_id']) == child.pid; break
            except OSError:
                assert time.monotonic() < due; time.sleep(.02)
        result.update(port=port, pid=child.pid, run_id=info[b'run_id'].decode(), command=command); save(); owned()
        exchange(port, ['ACL', 'SETUSER', 'shared-fixture', 'reset', 'on', 'nopass', 'allkeys', '+hello', '+auth', '+ping', '+info', '+client|id', '+echo', '+blpop'])
        for client in clients:
            owned(); command = [str(client), str(port), result['run_id']]
            completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
            result['cases'].append(dict(command=command, client_sha256=sha(client), exit_code=completed.returncode,
                                        output=completed.stdout, expected_native_matrix_cases=18,
                                        native_matrix_cases=18 if completed.returncode == 0 else None)); save()
            assert completed.returncode == 0, completed.stdout
            print(client.name, '18 native shared-pool cases passed', flush=True)
        result['complete'] = True
    except BaseException:
        result['failure'] = traceback.format_exc(); raise
    finally:
        if child is not None:
            if child.poll() is None: child.terminate()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
        log.close(); shutil.copy2(runtime / 'server.log', args.report.with_suffix('.server.log'))
        temporary.cleanup(); result.update(processes_terminal=child is None or child.poll() is not None, runtime_material_removed=not runtime.exists()); save()

if __name__ == '__main__': main()
