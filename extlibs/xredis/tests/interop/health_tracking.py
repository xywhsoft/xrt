"""Independent loopback health check, executed by native Python for Windows probes."""
import socket,ssl,sys
with socket.create_connection(('127.0.0.1',int(sys.argv[1])),timeout=1) as raw:
 context=ssl.create_default_context(cafile=sys.argv[2]) if sys.argv[2]!='-' else None
 with (context.wrap_socket(raw,server_hostname='localhost') if context else raw) as connection:
  connection.sendall(b'*1\r\n$4\r\nPING\r\n');reply=connection.recv(4096)
  if not reply.startswith((b'+PONG\r\n',b'-NOAUTH')):raise RuntimeError('fixture did not answer health PING')
