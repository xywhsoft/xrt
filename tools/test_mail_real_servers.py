"""Validate C mail examples against private Dovecot 2.4/Postfix instances.

Linux root and a private mount namespace are required. Extract server packages
with prepare_mail_servers.py; no packages, users, services or trust stores are
installed. Run via: unshare --mount --propagation private -- python3 this.py ...
An optional Windows Python bridge exercises Windows clients through WSL's
localhost forwarding. All messages go to the single example.test mailbox.
"""
import argparse
import atexit
import hashlib
import json
import os
from pathlib import Path
import signal
import shutil
import secrets
import smtplib
import socket
import ssl
import subprocess
import sys
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--compiler', default='gcc')
parser.add_argument('--windows-python', type=Path)
parser.add_argument('--windows-bridge', help='Windows absolute path to mail_server_windows_bridge.py')
parser.add_argument('--wsl-distribution', default='Ubuntu', help='distribution name for the Windows CA UNC path')
args = parser.parse_args()
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from mail_server_test_inputs import snapshot, sha256
source_inputs = snapshot(ROOT)
RUNTIME = args.runtime.resolve() / 'extracted'
OUT = args.output.resolve()
if sys.platform != 'linux' or os.geteuid() != 0:
    parser.error('requires Linux root in a private mount namespace')
if os.readlink('/proc/self/ns/mnt') == os.readlink('/proc/1/ns/mnt'):
    parser.error('run under unshare --mount --propagation private')
if bool(args.windows_python) != bool(args.windows_bridge):
    parser.error('--windows-python and --windows-bridge must be used together')
if len(str(OUT).encode()) > 70 or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./+-' for c in str(OUT) + str(RUNTIME)):
    parser.error('runtime/output paths must be short ASCII paths without spaces or configuration metacharacters')
runtime_manifest = json.loads((args.runtime / 'runtime.json').read_text(encoding='utf-8'))
for relative, wanted in runtime_manifest['files'].items():
    if hashlib.sha256((RUNTIME / relative).read_bytes()).hexdigest() != wanted:
        parser.error('server runtime differs from its package manifest: ' + relative)
PASSWORD = secrets.token_urlsafe(24)
records = []
OUT.mkdir(exist_ok=False)
os.chmod(OUT, 0o755)
upper = OUT / 'lib-upper'; upper.mkdir()
work = OUT / 'lib-work'; work.mkdir()
subprocess.run(['mount', '-t', 'overlay', 'overlay', '-o', f'lowerdir=/usr/lib,upperdir={upper},workdir={work}', '/usr/lib'], check=True)
Path('/usr/lib/dovecot').mkdir(exist_ok=True)
subprocess.run(['mount', '--bind', str(RUNTIME / 'usr/lib/dovecot'), '/usr/lib/dovecot'], check=True)
Path('/usr/lib/postfix').mkdir(exist_ok=True)
subprocess.run(['mount', '--bind', str(RUNTIME / 'usr/lib/postfix'), '/usr/lib/postfix'], check=True)
for library in (RUNTIME / 'usr/lib/x86_64-linux-gnu').iterdir():
    destination = Path('/usr/lib/x86_64-linux-gnu') / library.name
    if not os.path.lexists(destination):
        if library.is_symlink(): os.symlink(os.readlink(library), destination)
        elif library.is_file(): shutil.copy2(library, destination)
subprocess.run(['mount', '-t', 'tmpfs', '-o', 'mode=755', 'tmpfs', '/dev'], check=True)
for name, major, minor in [('null', 1, 3), ('zero', 1, 5), ('random', 1, 8), ('urandom', 1, 9), ('full', 1, 7)]:
    import stat
    os.mknod('/dev/' + name, stat.S_IFCHR | 0o666, os.makedev(major, minor))
    os.chmod('/dev/' + name, 0o666)
os.symlink('/proc/self/fd', '/dev/fd')
for name, fd in [('stdin', 0), ('stdout', 1), ('stderr', 2)]: os.symlink('/proc/self/fd/' + str(fd), '/dev/' + name)
logsocket = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
logsocket.bind('/dev/log'); os.chmod('/dev/log', 0o666); logsocket.settimeout(.2)
logstop = threading.Event()
def syslog_reader():
    with (OUT / 'syslog.log').open('wb') as stream:
        while not logstop.is_set():
            try: data = logsocket.recv(65536)
            except socket.timeout: continue
            stream.write(data + b'\n'); stream.flush()
logger = threading.Thread(target=syslog_reader, daemon=True); logger.start()
sys.path.insert(0, str(ROOT / 'tools'))
from test_mail_tls_interop import certificates, build_examples, example_directory, EXAMPLES

ENV = dict(os.environ)
ENV['LD_LIBRARY_PATH'] = ':'.join(str(RUNTIME / p) for p in ['usr/lib/x86_64-linux-gnu', 'usr/lib/dovecot', 'usr/lib/postfix'])
ports = {}
for name in ['pop3', 'pop3s', 'imap', 'imaps', 'smtp', 'smtps']:
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        ports[name] = s.getsockname()[1]
certdir = OUT / 'certs'; certdir.mkdir()
passwd = OUT / 'passwd'
def remove_credentials():
    for sensitive in [passwd, OUT / 'dovecot/auth-token-secret.dat', *certdir.glob('*.key')]:
        sensitive.unlink(missing_ok=True)
atexit.register(remove_credentials)
ca, leaf, _, key = certificates(certdir)
home = OUT / 'home'; home.mkdir()
os.chown(home, 65534, 65534)
mail = home / 'Maildir'
for name in ['cur', 'new', 'tmp']:
    folder = mail / name; folder.mkdir(parents=True, exist_ok=True); os.chown(folder, 65534, 65534)
os.chown(mail, 65534, 65534)
passwd.write_text(f'demo@example.test:{{PLAIN}}{PASSWORD}:65534:65534::{home}::\n')
os.chown(passwd, 0, 2); os.chmod(passwd, 0o640)
dovecot = OUT / 'dovecot'; dovecot.mkdir()
conf = OUT / 'dovecot.conf'
conf.write_text(f'''dovecot_config_version = 2.4.0
dovecot_storage_version = 2.4.0
protocols {{
  imap = yes
  pop3 = yes
  lmtp = yes
}}
listen = 127.0.0.1
base_dir = {dovecot}
state_dir = {OUT}/dovecot-state
libexec_dir = {RUNTIME}/usr/lib/dovecot
mail_plugin_dir = {RUNTIME}/usr/lib/dovecot/modules
default_internal_user = bin
default_internal_group = bin
default_login_user = daemon
import_environment {{
  LD_LIBRARY_PATH = {ENV['LD_LIBRARY_PATH']}
}}
ssl = required
ssl_server_cert_file = {leaf}
ssl_server_key_file = {key}
auth_mechanisms = plain login
auth_allow_cleartext = no
mail_driver = maildir
mail_path = ~/Maildir
namespace inbox {{
  inbox = yes
  separator = /
}}
passdb passwd-file {{
  passwd_file_path = {passwd}
}}
userdb passwd-file {{
  passwd_file_path = {passwd}
}}
service imap-login {{
  inet_listener imap {{
    port = {ports['imap']}
  }}
  inet_listener imaps {{
    port = {ports['imaps']}
    ssl = yes
  }}
}}
service pop3-login {{
  inet_listener pop3 {{
    port = {ports['pop3']}
  }}
  inet_listener pop3s {{
    port = {ports['pop3s']}
    ssl = yes
  }}
}}
service auth {{
  unix_listener auth-postfix {{
    mode = 0600
    user = nobody
  }}
}}
service lmtp {{
  unix_listener lmtp {{
    mode = 0600
    user = nobody
  }}
}}
log_path = {OUT}/dovecot.log
info_log_path = {OUT}/dovecot-info.log
''')
postfix = OUT / 'postfix'; postfix.mkdir()
queue = OUT / 'queue'; queue.mkdir()
data = OUT / 'postfix-data'; data.mkdir(); os.chown(data, 65534, 65534)
postlog = OUT / 'postfix.log'; postlog.touch(); os.chown(postlog, 65534, 65534)
for name in ['active','bounce','corrupt','defer','deferred','flush','hold','incoming','private','maildrop','public','pid','saved','trace']:
    folder = queue / name; folder.mkdir()
    if name != 'pid': os.chown(folder, 65534, 65534); os.chmod(folder, 0o700)
(postfix / 'main.cf').write_text(f'''compatibility_level = 3.10
queue_directory = {queue}
data_directory = {data}
daemon_directory = {RUNTIME}/usr/lib/postfix/sbin
command_directory = {RUNTIME}/usr/sbin
meta_directory = {RUNTIME}/etc/postfix
shlib_directory = {RUNTIME}/usr/lib/postfix
mail_owner = nobody
default_privs = daemon
setgid_group = mail
myhostname = localhost
mydomain = example.test
mydestination =
local_recipient_maps =
alias_maps =
alias_database =
inet_interfaces = loopback-only
inet_protocols = ipv4
virtual_mailbox_domains = example.test
virtual_mailbox_maps = static:1
virtual_transport = lmtp:unix:{dovecot}/lmtp
smtpd_relay_restrictions = reject_unauth_destination
smtpd_recipient_restrictions = reject_unauth_destination
smtpd_tls_security_level = encrypt
smtpd_tls_cert_file = {leaf}
smtpd_tls_key_file = {key}
smtpd_tls_auth_only = yes
smtpd_sasl_auth_enable = yes
smtpd_sasl_type = dovecot
smtpd_sasl_path = {dovecot}/auth-postfix
smtpd_sasl_security_options = noanonymous
smtpd_tls_loglevel = 1
maillog_file =
import_environment = MAIL_CONFIG MAIL_DEBUG MAIL_LOGTAG MAIL_VERBOSE LD_LIBRARY_PATH
export_environment = MAIL_CONFIG MAIL_DEBUG MAIL_LOGTAG MAIL_VERBOSE LD_LIBRARY_PATH
''')
(postfix / 'master.cf').write_text(f'''127.0.0.1:{ports['smtp']} inet n - n - - smtpd
127.0.0.1:{ports['smtps']} inet n - n - - smtpd
  -o smtpd_tls_wrappermode=yes
cleanup unix n - n - 0 cleanup
qmgr unix n - n 300 1 qmgr
rewrite unix - - n - - trivial-rewrite
proxymap unix - - n - - proxymap
proxywrite unix - - n - 1 proxymap
lmtp unix - - n - - lmtp
bounce unix - - n - 0 bounce
defer unix - - n - 0 bounce
trace unix - - n - 0 bounce
tlsmgr unix - - n 1000? 1 tlsmgr
anvil unix - - n - 1 anvil
postlog unix-dgram n - n - 1 postlogd
''')
streams = []; processes = []
client_inputs = None
verification_completed = False
ENV['MAIL_CONFIG'] = str(postfix)
try:
    for name, command in [('dovecot', [str(RUNTIME / 'usr/bin/doveconf'), '-c', str(conf), '-F', '--', str(RUNTIME / 'usr/sbin/dovecot'), '-F', '-c', str(conf)]), ('postfix', [str(RUNTIME / 'usr/lib/postfix/sbin/master'), '-s', '-c', str(postfix), '-e', '600'])]:
        stream = (OUT / (name + '.process.log')).open('wb'); streams.append(stream)
        process = subprocess.Popen(command, env=ENV, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        processes.append(process)
    for name, port in ports.items():
        for attempt in range(60):
            if any(p.poll() is not None for p in processes): raise RuntimeError('server process exited; inspect process logs: ' + repr([(p.pid, p.poll()) for p in processes]))
            try:
                with socket.create_connection(('127.0.0.1', port), timeout=.2): pass
                break
            except OSError: time.sleep(.1)
        else: raise RuntimeError('listener not ready: ' + name)
    print('[ready] actual Dovecot/Postfix', json.dumps(ports), flush=True)
    context = ssl.create_default_context(cafile=ca)
    subprocess.run(['ps', '-eo', 'pid,ppid,state,wchan:24,cmd'], stdout=(OUT / 'processes-before-smtp.log').open('w'), check=True)
    with smtplib.SMTP('localhost', ports['smtp'], timeout=10) as client:
        client.ehlo(); client.starttls(context=context); client.ehlo(); client.login('demo@example.test', PASSWORD)
        client.sendmail('sender@example.test', ['demo@example.test'], b'From: sender@example.test\r\nTo: demo@example.test\r\nSubject: prototype oracle\r\n\r\nHello from real mail services.\r\n')
    for attempt in range(100):
        messages = list((mail / 'new').iterdir()) + list((mail / 'cur').iterdir())
        if messages: break
        time.sleep(.1)
    assert len(messages) == 1 and b'Hello from real mail services.' in messages[0].read_bytes()
    print('[oracle] SMTP TLS + SASL + LMTP + Maildir succeeded', flush=True)
    def messages_now():
        return list((mail / 'new').iterdir()) + list((mail / 'cur').iterdir())

    def server_logs():
        return {name: (OUT / name).read_text(errors='replace') if (OUT / name).exists() else ''
                for name in ['syslog.log', 'dovecot-info.log']}

    def check_server_stage(kind, scenario, previous):
        for attempt in range(40):
            current = server_logs()
            delta = {name: text[len(previous[name]):] for name, text in current.items()}
            text = delta['syslog.log' if kind == 'smtp' else 'dovecot-info.log']
            if scenario == 'success':
                expected = ['sasl_method=PLAIN', 'data=1', 'status=sent'] if kind == 'smtp' else [kind + '-login:', 'Logged in:', kind + '(demo@example.test)', 'Logged out']
            elif scenario == 'wrong-password':
                expected = ['SASL PLAIN authentication failed', 'auth=0/1'] if kind == 'smtp' else [kind + '-login:', '(auth_failed)', 'TLS']
            else:
                expected = ['SSL_accept error', 'commands='] if kind == 'smtp' else [kind + '-login:', '(tls_handshake_not_finished)', 'user=<>']
            if all(marker in text for marker in expected):
                if scenario != 'success':
                    assert 'data=1' not in text and 'Logged in:' not in text and 'sasl_method=PLAIN' not in text
                return delta
            time.sleep(.1)
        raise AssertionError((kind, scenario, 'missing independent server stage evidence', delta))

    def bridge(request):
        result = subprocess.run([str(args.windows_python), '-X', 'utf8', args.windows_bridge], input=json.dumps(request), text=True, capture_output=True, timeout=900)
        if result.returncode != 0:
            raise AssertionError('Windows bridge failed: ' + result.stderr[-3000:])
        return json.loads(result.stdout)

    if args.windows_python:
        client_inputs = bridge({'operation': 'build', 'compiler': args.compiler})['source_inputs']
        assert client_inputs == source_inputs, 'Windows and Linux source/build inputs differ'
    else:
        build_examples(args.compiler)

    for scenario in ['success', 'wrong-password', 'wrong-identity']:
        for mode in ['tls', 'starttls']:
            for kind in ['smtp', 'pop3', 'imap']:
                client = example_directory(kind, args.compiler) / EXAMPLES[kind][2]
                port = ports[kind + 's' if mode == 'tls' else kind]
                previous_logs = server_logs()
                before = len(messages_now())
                subject = 'Real mail acceptance ' + mode
                body = 'Hello from real mail services.\n.line beginning with a dot\n\nlast line'
                parameters = {'smtp': ['sender@example.test', 'demo@example.test', subject, body], 'pop3': ['1'], 'imap': ['INBOX']}[kind]
                host = '127.0.0.1' if scenario == 'wrong-identity' else 'localhost'
                secret = 'wrong-' + PASSWORD if scenario == 'wrong-password' else PASSWORD
                env = dict(os.environ)
                env.update({'X' + kind.upper() + '_USER': 'demo@example.test', 'X' + kind.upper() + '_PASSWORD': secret})
                ca_path = '\\\\wsl.localhost\\' + args.wsl_distribution + str(ca).replace('/', '\\') if args.windows_python else str(ca)
                command_args = [host, str(port), ca_path, *parameters, 'stls' if kind == 'pop3' and mode == 'starttls' else mode]
                if args.windows_python:
                    completed = bridge({'operation': 'run', 'compiler': args.compiler, 'kind': kind, 'args': command_args, 'secret': secret})
                else:
                    digest = hashlib.sha256(client.read_bytes()).hexdigest()
                    result = subprocess.run([str(client), *command_args], env=env, text=True, capture_output=True, timeout=45)
                    assert hashlib.sha256(client.read_bytes()).hexdigest() == digest
                    completed = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr, 'binary': str(client), 'binary_sha256': digest}
                expected_exit = 0 if scenario == 'success' else 1
                assert completed['exit_code'] == expected_exit, (kind, mode, scenario, completed)
                if scenario == 'success':
                    if kind == 'smtp':
                        for attempt in range(100):
                            if len(messages_now()) == before + 1: break
                            time.sleep(.1)
                        assert len(messages_now()) == before + 1
                        import email
                        from email import policy
                        delivered = [email.message_from_bytes(p.read_bytes(), policy=policy.default) for p in messages_now()]
                        matched = [m for m in delivered if m['Subject'] == subject]
                        assert len(matched) == 1 and matched[0]['From'] == 'sender@example.test' and matched[0]['To'] == 'demo@example.test'
                        content = matched[0].get_content().replace('\r\n', '\n')
                        assert content.rstrip('\n') == body
                    elif kind == 'pop3':
                        assert 'Subject: prototype oracle' in completed['stdout'] and 'Hello from real mail services.' in completed['stdout']
                    else:
                        assert completed['stdout'].strip() == f'INBOX: {before} messages'
                else:
                    assert len(messages_now()) == before
                    if scenario == 'wrong-password':
                        assert 'kind=10 code=1610' in completed['stderr'], completed
                    else:
                        assert 'open failed: kind=15 code=19' in completed['stderr'], completed
                    assert not completed['stdout'], completed
                completed.update(kind=kind, mode=mode, scenario=scenario, mailbox_messages_before=before, mailbox_messages_after=len(messages_now()))
                completed['server_stage_logs'] = check_server_stage(kind, scenario, previous_logs)
                records.append(completed)
                print('[PASS]', kind, mode, scenario, flush=True)
    if args.windows_python:
        assert bridge({'operation': 'inputs'}) == client_inputs, 'Windows source/build inputs changed during execution'
    assert snapshot(ROOT) == source_inputs, 'Linux source/build inputs changed during execution'
    verification_completed = True
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try: process.wait(timeout=10)
            except subprocess.TimeoutExpired: os.killpg(process.pid, signal.SIGKILL); process.wait(timeout=5)
    for stream in streams: stream.close()
    logstop.set(); logger.join(timeout=2); logsocket.close()
    (OUT / 'process-results.json').write_text(json.dumps([(p.pid, p.returncode) for p in processes]))
    remove_credentials()
    ports_closed = True
    for port in ports.values():
        try:
            with socket.create_connection(('127.0.0.1', port), timeout=.2):
                ports_closed = False
        except ConnectionRefusedError:
            pass
    runtime_unchanged = all(sha256(RUNTIME / relative) == wanted for relative, wanted in runtime_manifest['files'].items())
    source_unchanged = snapshot(ROOT) == source_inputs
    credentials_removed = not passwd.exists() and not (OUT / 'dovecot/auth-token-secret.dat').exists() and not list(certdir.glob('*.key'))
    stopped = all(p.poll() is not None for p in processes) and ports_closed
    (OUT / 'results.json').write_text(json.dumps({'all_passed': verification_completed and len(records) == 18 and stopped and credentials_removed and runtime_unchanged and source_unchanged, 'source_inputs': source_inputs, 'source_inputs_unchanged': source_unchanged, 'windows_source_inputs': client_inputs, 'platform': 'windows' if args.windows_python else 'linux', 'compiler': args.compiler, 'server_runtime': str(args.runtime), 'runtime_packages': runtime_manifest['packages'], 'runtime_manifest_sha256': sha256(args.runtime / 'runtime.json'), 'runtime_files_unchanged': runtime_unchanged, 'ports': ports, 'cases': records, 'temporary_credentials_removed': credentials_removed, 'services_stopped': stopped, 'test_only_service': True, 'external_provider_verified': False}, indent=2) + '\n', encoding='utf-8')
    assert stopped and credentials_removed and runtime_unchanged and source_unchanged
    print('[stopped]', [(p.pid, p.returncode) for p in processes], flush=True)
