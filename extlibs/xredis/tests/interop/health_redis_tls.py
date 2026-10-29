"""Independent native-loopback TLS startup probe, using ephemeral fixture keys."""
import socket
import ssl
import sys


def main():
    if len(sys.argv) != 5:
        return 2
    context = ssl.create_default_context(cafile=sys.argv[2])
    context.load_cert_chain(sys.argv[3], sys.argv[4])
    with socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=1) as raw:
        with context.wrap_socket(raw, server_hostname="localhost") as connection:
            connection.sendall(b"*1\r\n$4\r\nPING\r\n")
            response = connection.recv(4096)
            if not response.startswith((b"+PONG\r\n", b"-NOAUTH")):
                raise RuntimeError("fixture Redis did not answer PING")
    return 0


if __name__ == "__main__":
    sys.exit(main())
