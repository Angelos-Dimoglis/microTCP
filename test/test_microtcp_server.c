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

/*
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../lib/microtcp.h"

int main(int argc, char **argv) {
    printf("server running...\n");

    microtcp_sock_t socket = microtcp_socket(0, 0, 0);

    struct in_addr addr;

    if (inet_aton("127.0.0.1", &addr) == 0) {
        perror("invalid ip address");
        return EXIT_FAILURE;
    }
    socklen_t length = sizeof(struct sockaddr_in);

    const struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(2122),
        .sin_addr = addr,
    };

    const struct sockaddr_in address2 = {
        .sin_family = AF_INET,
        .sin_port = htons(2121),
        .sin_addr = addr,
    };

    if (microtcp_bind(&socket, (struct sockaddr *) &address, length) < 0) {
        perror("microtcp_bind failed");
        printf("msg: %s, errno: %d\n", strerror(errno), errno);
        return EXIT_FAILURE;
    }

    if (microtcp_accept(&socket, (struct sockaddr *) &address2, length) < 0) {
        perror("microtcp_accept failed");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 5; i++) {
        printf("%d\n", i);
        sleep(1);
    }

    char buffer[4096];
    while(1) {
        if (microtcp_recv(&socket, &buffer, sizeof(buffer), 0) < 0)
            break;

        printf("%s\n", buffer);
    }

}
