#!/usr/bin/env python3
"""
P1 — Process 1 (Python, Windows)

Reads data from stdin and sends it over UDP to P2 (Kali).
Uses a stop-and-wait ACK: each packet is retransmitted until the matching
ACK arrives or MAX_RETRY is exhausted.

Dependencies: Python standard library only.
"""

import socket
import sys

P2_HOST = "KALI_HOST"  # placeholder: IPv4 address of the Kali host running P2
P2_PORT = 9001
CHUNK_SIZE = 508
TIMEOUT = 0.5  # seconds to wait for an ACK
MAX_RETRY = 5  # maximum send attempts per packet


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)  # recv() raises socket.timeout after TIMEOUT seconds

    sys.stderr.write(f"[P1] Sending UDP data to {P2_HOST}:{P2_PORT}\n")
    sys.stderr.flush()

    stdin = sys.stdin.buffer if hasattr(sys.stdin, "buffer") else sys.stdin

    seq = 0  # packet sequence number

    while True:
        chunk = stdin.read(CHUNK_SIZE)
        if not chunk:
            break

        # Prepend a 4-byte big-endian sequence number
        packet = seq.to_bytes(4, "big") + chunk

        for attempt in range(MAX_RETRY):
            sock.sendto(packet, (P2_HOST, P2_PORT))
            try:
                ack, _ = sock.recvfrom(8)  # wait for ACK
                ack_seq = int.from_bytes(ack[:4], "big")
                if ack_seq == seq:
                    break  # matching ACK received
            except socket.timeout:
                sys.stderr.write(
                    f"[P1] Timeout on packet {seq}, attempt {attempt + 1}/{MAX_RETRY}\n"
                )
                sys.stderr.flush()
        else:
            sys.stderr.write(
                f"[P1] Packet {seq} not delivered after {MAX_RETRY} attempts!\n"
            )
            sys.stderr.flush()

        seq += 1

    # End-of-stream: empty payload with the next sequence number
    end_packet = seq.to_bytes(4, "big") + b""
    for _ in range(MAX_RETRY):
        sock.sendto(end_packet, (P2_HOST, P2_PORT))
        try:
            ack, _ = sock.recvfrom(8)
            ack_seq = int.from_bytes(ack[:4], "big")
            if ack_seq == seq:
                break
        except socket.timeout:
            pass

    sys.stderr.write("[P1] Send complete.\n")
    sys.stderr.flush()
    sock.close()


if __name__ == "__main__":
    main()
