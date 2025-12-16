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
 * You can use this file to write a test microTCP client.
 * This file is already inserted at the build system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "../lib/microtcp.h"

microtcp_sock_t socket1;

void handle_signal (int temp) {
    microtcp_shutdown(&socket1, 0);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    printf("client running...\n");

    socket1 = microtcp_socket(0, 0, 0);

    struct in_addr addr;

    if (inet_aton("127.0.0.1", &addr) == 0) {
        perror("invalid ip address");
        return EXIT_FAILURE;
    }
    socklen_t length = sizeof(struct sockaddr_in);

    const struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(2121),
        .sin_addr = addr,
    };

    const struct sockaddr_in address2 = {
        .sin_family = AF_INET,
        .sin_port = htons(2122),
        .sin_addr = addr,
    };

    if (microtcp_bind(&socket1, (struct sockaddr *) &address, length) < 0) {
        perror("microtcp_bind failed");
        printf("msg: %s, errno: %d\n", strerror(errno), errno);
        return EXIT_FAILURE;
    }

    if (microtcp_connect(&socket1, (struct sockaddr *) &address2, length) < 0) {
        perror("microtcp_connect failed");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 5; i++) {
        printf("%d\n", i);
        sleep(1);
    }

    /*
    char *buffer = "Hello CSD";
    microtcp_send(&socket1, buffer, strlen(buffer)+1, 0);
    microtcp_shutdown(&socket1, 0);
    */

    char *buffer = 0;
    size_t size = 0;
    ssize_t read;
    signal(SIGINT, handle_signal);
    while(1) {
        read = getline(&buffer, &size, stdin);

        microtcp_send(&socket1, buffer, read + 1, 0);
    }

}
