# Cross-host data pipeline (P1–P4)

Lab project that moves a binary stream across **two hosts** using three transports:

```
stdin  →  P1 (Python / Windows)  --UDP+ACK-->  P2 (C / Kali)
                                                   |
                                                   | TCP
                                                   v
stdout ←  P4 (Java / Kali)  <--XML-RPC--  P3 (Python / Windows)
```

Typical use: pipe a file (or a media stream) into P1 on Windows and reconstruct it on Kali from P4’s stdout.

This is a **course laboratory**, not production software. There is no authentication, integrity check, or encryption. Use it only on an isolated lab network.

## Hosts and placeholders

Replace the placeholders **before** you run anything. They are string constants in the source, not environment variables.

| Placeholder     | Meaning                         | File(s)      |
|-----------------|---------------------------------|--------------|
| `KALI_HOST`     | IPv4 of the Kali machine        | `P1.py`, `P3.py` |
| `WINDOWS_HOST`  | IPv4 of the Windows machine     | `P2.c`       |

P2 and P4 bind on the Kali host (`INADDR_ANY` / all interfaces). P3 binds TCP on Windows (`0.0.0.0:9002`).

Example: if Kali is `192.0.2.10` and Windows is `192.0.2.20`, set:

- `P1.py`: `P2_HOST = "192.0.2.10"`
- `P3.py`: `P4_HOST = "192.0.2.10"`
- `P2.c`:  `#define P3_HOST "192.0.2.20"`

## Ports

| Process | Role              | Port | Protocol |
|---------|-------------------|------|----------|
| P2      | UDP listener      | 9001 | UDP      |
| P3      | TCP listener      | 9002 | TCP      |
| P4      | XML-RPC listener  | 9003 | HTTP     |

Open these ports between the two hosts (or disable the host firewall for the lab).

## UDP packet format (P1 ↔ P2)

Each datagram:

| Offset | Size    | Content                          |
|--------|---------|----------------------------------|
| 0      | 4 bytes | Sequence number, big-endian      |
| 4      | 0–508 B | Payload (`CHUNK_SIZE` is 508)    |

- P1 waits up to **0.5 s** for an ACK that repeats the same 4-byte sequence number, then retries up to **5** times.
- P2 ACKs every datagram (including duplicates) but appends payload only when `seq == expected_seq`.
- End of stream: a datagram with the next sequence number and **empty** payload.

P2 then sends the reassembled buffer over a single TCP connection to P3. P3 forwards each TCP read as `ChunkReceiver.receive_chunk(data, isLast)` to P4. `isLast=true` with empty data ends the transfer; P4 writes chunks to **stdout** and shuts down.

## Requirements

**Windows (P1, P3)**

- Python 3 (stdlib only: `socket`, `xmlrpc.client`)

**Kali / Linux (P2, P4)**

- `gcc`
- JDK 8+ (`javac`, `java`)
- Apache XML-RPC 3.x JARs on the classpath (see below)

### P4 libraries

Create a `lib/` directory next to `P4.java` and put the Apache XML-RPC JARs there, for example:

- `xmlrpc-client`
- `xmlrpc-server`
- `xmlrpc-common`
- `ws-commons-util`

Classpath used in the commands below: `.:lib/*`

Do not commit third-party JARs unless your licence review allows it; download them yourself.

## Build

On Kali:

```bash
gcc -o P2 P2.c
javac -cp ".:lib/*" P4.java
```

P1 and P3 need no build step.

## Run order

Start **listeners first**, then the sender.

1. **Kali — P4**

   ```bash
   java -cp ".:lib/*" P4 > output.bin
   ```

   Play instead of saving:

   ```bash
   java -cp ".:lib/*" P4 | vlc -
   ```

2. **Windows — P3**

   ```text
   python P3.py
   ```

3. **Kali — P2**

   ```bash
   ./P2
   ```

4. **Windows — P1** (stdin is the payload)

   ```text
   python P1.py < input.bin
   ```

P2 retries the TCP connect to P3 for about 30 seconds. If P3 is not listening, start P3 and run P2 again.

Logs go to **stderr**. Byte counts from P2, P3, and P4 should match the size of `input.bin` (and `output.bin`).

## Repository layout

```text
P1.py      UDP sender + ACK client (Windows)
P2.c       UDP ACK server + TCP client (Kali)
P3.py      TCP server + XML-RPC client (Windows)
P4.java    XML-RPC server, writes stdout (Kali)
README.md
```

## Lab limitations (intentional)

- Placeholders must be edited in source; there is no config file.
- UDP ACKs are not authenticated; a third party on the path can inject or replay packets.
- XML-RPC is plain HTTP with no TLS and no access control.
- Listeners bind on all interfaces.
- P2 buffers the **entire** stream in RAM before opening TCP (initial 128 MB, then doubles).

Treat this as a protocol/IPC exercise on a trusted LAN, not as a template for a service exposed to the internet.
