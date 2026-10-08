#!/usr/bin/env python3
"""
P3 — Process 3 (Python, Windows)

Listens for a TCP connection from P2 (Kali) on port 9002.
Counts received bytes and prints the total on stderr.
Forwards each chunk to P4 (Kali) over XML-RPC.

Dependencies: Python standard library only.
"""

import socket
import sys
import xmlrpc.client

P3_LISTEN_PORT = 9002
P4_HOST = "KALI_HOST"  # placeholder: IPv4 address of the Kali host running P4
P4_XMLRPC_PORT = 9003
CHUNK_SIZE = 60000


def main():
    # XML-RPC client for P4
    p4 = xmlrpc.client.ServerProxy(f"http://{P4_HOST}:{P4_XMLRPC_PORT}/")

    # TCP server — waits for P2
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(("0.0.0.0", P3_LISTEN_PORT))
    server_sock.listen(1)

    sys.stderr.write(f"[P3] Listening for TCP on port {P3_LISTEN_PORT}\n")
    sys.stderr.flush()

    conn, addr = server_sock.accept()
    sys.stderr.write(f"[P3] Connected from P2: {addr}\n")
    sys.stderr.flush()

    total_bytes = 0

    try:
        while True:
            chunk = conn.recv(CHUNK_SIZE)
            if not chunk:
                # P2 closed the connection — end of data
                break

            total_bytes += len(chunk)

            # Forward the chunk to P4 over XML-RPC
            # xmlrpc.client.Binary carries binary payloads safely
            p4.ChunkReceiver.receive_chunk(xmlrpc.client.Binary(chunk), False)

        # End-of-stream signal to P4
        p4.ChunkReceiver.receive_chunk(xmlrpc.client.Binary(b""), True)

        sys.stderr.write(f"[P3] Bytes received: {total_bytes}\n")
        sys.stderr.flush()

    except Exception as e:
        sys.stderr.write(f"[P3] Error: {e}\n")
        sys.stderr.flush()
    finally:
        conn.close()
        server_sock.close()


if __name__ == "__main__":
    main()
