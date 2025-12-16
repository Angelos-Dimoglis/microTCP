/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "microtcp.h"
#include "siphash.h"
#include "../utils/crc32.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define ackfin_or_neither(x) \
(~((x & 1) ^ (((x) >> 4) & 1)) == 3)

size_t microtcp_seq(microtcp_sock_t *socket) {
    siphash_key_t key = key_init();

    return siphash_3u32(
        socket->saddr->sin_addr.s_addr, socket->daddr->sin_addr.s_addr,
        socket->saddr->sin_port << 16 | socket->daddr->sin_port,
        &key, 2, 4
    );
}

microtcp_sock_t microtcp_socket(int domain, int type, int protocol) {
    if (type == SOCK_DGRAM) {
        errno = -EINVAL;
        exit(EXIT_FAILURE);
    }

    microtcp_sock_t msock = {
        .sd = socket(AF_INET, SOCK_DGRAM, 0),
        .state = INVALID,
    };

    if (msock.sd < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    return msock;
}

int microtcp_bind(
    microtcp_sock_t *socket,
    const struct sockaddr *address,
    socklen_t address_len
) {
    socket->saddr = (const struct sockaddr_in *)address;

    if (bind(socket->sd, address, address_len) == -1) {
        socket->state = INVALID;
        return -1;
    }

    socket->state = BOUND;
    return 0;
}

int microtcp_connect(
    microtcp_sock_t *socket,
    const struct sockaddr *address,
    socklen_t address_len
) {
    uint64_t seq;
    siphash_key_t key;
    struct sockaddr saddr;
    microtcp_header_t *rbuf;

    if (connect(socket->sd, address, address_len) < 0
        || socket->state != (LISTEN | BOUND)) {
        socket->state = INVALID;
        printf("invalid socket state\n");
        return -1;
    }

    socket->recvbuf = malloc(MICROTCP_RECVBUFS);
    if (!socket->recvbuf) {
        perror("malloc recvbuf:");
        exit(EXIT_FAILURE);
    }

    /* make the SYN */
    socket->daddr = (const struct sockaddr_in *)address;
    socket->seq_number = microtcp_seq(socket);
    socket->control = SYN;

    /* send the SYN */
    if (microtcp_send(socket, 0, 0, 0) < 0) {
        socket->state = INVALID;
        return -1;
    }
    printf("syn sent by the client\n");
    socket->state = SYN_SENT;

    /* make ack */
    socket->ack_number = socket->peer_seq_number + 1;
    socket->control = ACK;

    /* send ack */
    if (microtcp_send(socket, 0, 0, 0) < 0) {
        socket->state = INVALID;
        return -1;
    }

    socket->state = ESTABLISHED;
    printf("connection established\n");

    return 0;
}

int microtcp_accept(
    microtcp_sock_t *socket,
    struct sockaddr *address,
    socklen_t address_len
) {
    /* TODO: make a new socket (somewhere) and return it */

    int fd;

    if (socket->state != BOUND) {
        socket->state = INVALID;
        printf("invalid socket state\n");
        return -1;
    }

    socket->recvbuf = malloc(MICROTCP_RECVBUFS);
    if (!socket->recvbuf) {
        perror("malloc recvbuf:");
        exit(EXIT_FAILURE);
    }

    /* blocks and listens for connections */
    socket->state = LISTEN;
    printf("listening...\n");

    /* get the SYN */
    socket->daddr = malloc(sizeof *socket->daddr);
    socklen_t addr_len = sizeof *socket->daddr;
    if (recvfrom(socket->sd, socket->recvbuf, MICROTCP_RECVBUFS, 0,
                 (struct sockaddr*) socket->daddr, &addr_len) < 0) {
        perror("recvfrom failed");
        socket->state = INVALID;
        return -1;
    }

    socket->state = SYN_RCVD;

    if (connect(socket->sd, address, address_len) < 0) {
        socket->state = INVALID;
        printf("invalid socket state\n");
        return -1;
    }

    microtcp_header_t *rbuf = socket->recvbuf;

    /* make the SYN+ACK */
    socket->seq_number = microtcp_seq(socket);
    socket->ack_number = rbuf->seq_number + 1;
    socket->control = ACK | SYN;

    /* send the SYN+ACK */
    if (microtcp_send(socket, 0, 0, 0) < 0) {
        socket->state = INVALID;
        return -1;
    }

    socket->state = ESTABLISHED;
    printf("connection established\n");

    return 0;
}

int microtcp_shutdown(microtcp_sock_t *socket, int how) {
    socket->control = ACK | FIN;

    if (microtcp_send(socket, 0, 0, 0) < 0) {
        if (socket->state == CLOSING_BY_HOST &&
            microtcp_recv(socket, 0, 0, 0) < 0) {
            socket->state = INVALID;
            return -1;
        }
    }

    socket->state = CLOSED;
    return 0;
}

static inline ssize_t process_ack(microtcp_sock_t *socket) {
    ssize_t rlen;
    microtcp_header_t *rhdr;
    uint32_t checksum;

    rlen = recv(socket->sd, socket->recvbuf,
                MICROTCP_RECVBUFS, MSG_WAITALL);
    if (rlen < 0)
        return -1;

    rhdr = (microtcp_header_t *)socket->recvbuf;
    checksum = rhdr->checksum;
    rhdr->checksum = 0;

    if (checksum != crc32(socket->recvbuf, rlen)
        || (rhdr->control & ACK) != ACK) {
        errno = -EIO;
        return -1;
    }

    socket->ack_number = rhdr->ack_number;
    socket->peer_seq_number = rhdr->seq_number;

    return rlen;
}

ssize_t microtcp_send(
    microtcp_sock_t *socket,
    const void *buffer,
    size_t length,
    int flags
) {
    size_t step;
    const void *p = buffer;
    void *pload, *q;

    /* check if the FSM invariant broke */
    if (socket->state == (INVALID | LISTEN)
        && (buffer || length)) {
        exit(EXIT_FAILURE);
    };

    pload = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);
    /* TODO: log this */
    if (!pload) {
        perror("Out of memory");
        exit(EXIT_FAILURE);
    }

    while (p <= buffer+length) {
        step = (buffer+length - p < MICROTCP_MSS ? length : MICROTCP_MSS);
        socket->seq_number += step;

        microtcp_header_t header = {
            .seq_number = socket->seq_number,
            .ack_number = socket->ack_number,
            .control = socket->control,
            .data_len = step,
        };

        q = mempcpy(pload, &header, sizeof(microtcp_header_t));
        p = mempcpy(q, p, step);

        step += sizeof(microtcp_header_t);

        ((microtcp_header_t *)pload)->checksum =
            crc32((uint8_t *)pload, step);

        if (send(socket->sd, pload, step, flags) < 0)
            return -1;

        if ((socket->control & (ACK | SYN)) == (ACK | SYN) &&
            process_ack(socket) < 0)
            return -1;

        if ((socket->control & ACK) != ACK && process_ack(socket) < 0)
            return -1;

    }

    return (ssize_t)length;
}

ssize_t microtcp_recv(
    microtcp_sock_t *socket,
    void *buffer,
    size_t length,
    int flags
) {
    ssize_t rlen;
    size_t step, avail;
    void *p = buffer;
    microtcp_header_t *rhdr;
    uint32_t checksum;

    if (socket->state == CLOSED || socket->state == INVALID) {
        errno = -ENOTCONN;
        return -1;
    }

    while (1) {
        rlen = recv(socket->sd, (void *)socket->recvbuf,
                    MICROTCP_RECVBUFS, flags);
        if (rlen < 0) {
            perror("something went wrong with recv");
            return -1;
        }
        socket->packets_received++;
        rhdr = (microtcp_header_t *)socket->recvbuf;
        checksum = rhdr->checksum;
        rhdr->checksum = 0;

        /* packets with payload in a non-established connection are
                        unacceptable */
        if (rhdr->data_len != 0 && socket->state != ESTABLISHED) { 
            perror("connection not yet established");
            errno = -ENOTCONN;
            return -1;
        }

        avail = length - (p - buffer);
        step = avail < rlen ? avail : rlen;
        socket->bytes_received += step;

        if (checksum != crc32(socket->recvbuf, rlen)) {
            errno = -EIO;
            return -1;
        }

        if ((rhdr->control & FIN) == FIN && 
            socket->state != CLOSING_BY_HOST &&
            socket->state != CLOSING_BY_PEER) {
            socket->state = CLOSING_BY_PEER;
            microtcp_shutdown(socket, 0);
            return -1;
        }

        p = mempcpy(p, socket->recvbuf + sizeof(microtcp_header_t), step);

        socket->ack_number += step;
        socket->control = ACK;
        if (microtcp_send(socket, 0, 0, 0) < 0)
            return -1;
        socket->control = 0;

        if (avail == step)
            break;

        if ((flags & MSG_WAITALL) != MSG_WAITALL)
            break;
    }

    return (ssize_t)(length - avail);
}
