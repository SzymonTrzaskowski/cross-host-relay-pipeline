/*
 * P2 — Process 2 (C, Kali Linux)
 *
 * Receives UDP packets from P1 (Windows) and replies with ACKs.
 * Counts payload bytes and prints the total on stderr.
 * Forwards the reassembled buffer over TCP to P3 (Windows).
 *
 * Build: gcc -o P2 P2.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define UDP_PORT     9001
#define P3_HOST      "WINDOWS_HOST"  /* placeholder: IPv4 of the Windows host running P3 */
#define P3_TCP_PORT  9002
#define CHUNK_SIZE   508
#define INIT_BUF     (128 * 1024 * 1024)  /* 128 MB initial receive buffer */

int main(void) {
    /* UDP socket */
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) { perror("[P2] UDP socket"); return 1; }

    /* Enlarge the UDP receive buffer to 64 MB */
    int rcvbuf = 64 * 1024 * 1024;
    setsockopt(udp_sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family      = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port        = htons(UDP_PORT);

    if (bind(udp_sock, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("[P2] UDP bind"); return 1;
    }
    fprintf(stderr, "[P2] Listening for UDP on port %d\n", UDP_PORT);
    fflush(stderr);

    /* Dynamic buffer for the full payload */
    size_t buf_cap = INIT_BUF;
    size_t buf_len = 0;
    unsigned char *buffer = malloc(buf_cap);
    if (!buffer) { fprintf(stderr, "[P2] out of memory\n"); return 1; }

    /* Packet buffer: 4-byte seq + payload */
    unsigned char pkt[4 + CHUNK_SIZE + 1];

    uint32_t expected_seq = 0;

    while (1) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        ssize_t n = recvfrom(udp_sock, pkt, sizeof(pkt), 0,
                             (struct sockaddr *)&sender, &sender_len);
        if (n < 0) { perror("[P2] recvfrom"); break; }

        /* Sequence number is the first 4 bytes (big-endian) */
        uint32_t seq = ((uint32_t)pkt[0] << 24) |
                       ((uint32_t)pkt[1] << 16) |
                       ((uint32_t)pkt[2] <<  8) |
                       ((uint32_t)pkt[3]);

        /* ACK every packet, including duplicates */
        unsigned char ack[4];
        ack[0] = pkt[0]; ack[1] = pkt[1];
        ack[2] = pkt[2]; ack[3] = pkt[3];
        sendto(udp_sock, ack, 4, 0,
               (struct sockaddr *)&sender, sender_len);

        /* Drop duplicates / out-of-order packets */
        if (seq != expected_seq) continue;
        expected_seq++;

        ssize_t data_len = n - 4;

        /* Empty payload marks end of stream from P1 */
        if (data_len == 0) break;

        /* Grow the buffer if needed */
        if (buf_len + data_len > buf_cap) {
            buf_cap *= 2;
            unsigned char *tmp = realloc(buffer, buf_cap);
            if (!tmp) { fprintf(stderr, "[P2] realloc failed\n"); break; }
            buffer = tmp;
        }

        memcpy(buffer + buf_len, pkt + 4, data_len);
        buf_len += data_len;
    }
    close(udp_sock);

    fprintf(stderr, "[P2] Total bytes received: %zu\n", buf_len);
    fflush(stderr);

    /* TCP connection to P3 */
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) { perror("[P2] TCP socket"); return 1; }

    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port   = htons(P3_TCP_PORT);
    inet_pton(AF_INET, P3_HOST, &tcp_addr.sin_addr);

    fprintf(stderr, "[P2] Connecting TCP to P3 %s:%d\n", P3_HOST, P3_TCP_PORT);
    fflush(stderr);

    int retries = 30;
    while (retries-- > 0) {
        if (connect(tcp_sock, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) == 0) break;
        fprintf(stderr, "[P2] Waiting for P3...\n");
        fflush(stderr);
        close(tcp_sock);
        tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        sleep(1);
    }

    fprintf(stderr, "[P2] Connected to P3 over TCP\n");
    fflush(stderr);

    /* Send the entire buffer to P3 */
    size_t sent = 0;
    while (sent < buf_len) {
        ssize_t s = send(tcp_sock, buffer + sent, buf_len - sent, 0);
        if (s <= 0) { perror("[P2] TCP send"); break; }
        sent += s;
    }

    close(tcp_sock);
    free(buffer);
    return 0;
}
