#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket.h"
#include "util.h"


#ifndef NDEBUG
#include <stdio.h>

static void print_long_hex(long val) {
    static char buf[sizeof(val)];

    int x, i;

    memcpy(buf, &val, sizeof(val));

    printf("0x");

    x = 1;
    if (((char *) &x)[0]) {
        for (i = sizeof(val) - 1; i >= 0; --i)
            printf("%02x", buf[i]);
    } else {
        for (i = 0; i < (int) sizeof(val); ++i)
            printf("%02x", buf[i]);
    }
}


static void print_block_preview(char *block, long block_length) {
    int i;

    for (i = 0; i < 10; ++i)
        printf("%02x", block[i]);

    printf("...");

    for (i = 9; i >= 0; --i)
        printf("%02x", block[block_length - i - 1]);
}
#endif


int create_socket(int port, enum socket_mode mode) {
    int sock_fd, conn_retries, conn_success;
    struct sockaddr_in addr;

    /* create socket */
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        errprintf("failed to create socket");
        return -1;
    }

    /* bind socket */
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    /* use localhost for now */
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (mode == SOCKET_BIND) {
        if (bind(sock_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
            errprintf("binding to socket failed");
            return -1;
        }
    } else if (mode == SOCKET_CONNECT) {
        conn_retries = 0;
        conn_success = 0;

        while (conn_retries < CONN_RETRIES) {
            if (connect(sock_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
#ifdef VERBOSE
                errprintf("could not connect to socket, retrying... (%d/%d)",
                          conn_retries + 1, CONN_RETRIES);
#endif
            } else {
                conn_success = 1;
                break;
            }

            ++conn_retries;
        }

        if (!conn_success) {
            errprintf("connecting to socket failed");
            return -1;
        }
    }

    return sock_fd;
}


int send_block(int sock_fd, char *block, long block_length) {
    ssize_t write_size;
    long block_offs = 0, chunk_size;

    /* send block size */
#ifndef NDEBUG
    printf("sending block size: ");
    print_long_hex(block_length);
    printf(" (%ld)\n", block_length);
#endif

    if (write(sock_fd, &block_length, sizeof(block_length)) == -1) {
        errprintf("failed to send block size (%s)", strerror(errno));
        return -1;
    }

    /* send block data */
#ifndef NDEBUG
    printf("sending block: ");
    print_block_preview(block, block_length);
#endif

    while (block_offs < block_length) {
        chunk_size = CHUNK_SIZE;
        if (block_offs + chunk_size > block_length)
            chunk_size = block_length - block_offs;

        if ((write_size =
             write(sock_fd, block + block_offs, chunk_size)) == -1) {

            errprintf("failed to send data (%s)", strerror(errno));
            return -1;
        }

        block_offs += write_size;
    }

#ifndef NDEBUG
    printf(" (%ld bytes)\n", block_offs);
#endif

    return 0;
}


static int receive_block_length(int sock_fd, long *block_length) {
    char buf[sizeof(*block_length)];
    ssize_t read_size;
    size_t read_offs = 0;

    while (read_offs < sizeof(buf)) {
        read_size = read(sock_fd, buf + read_offs, sizeof(buf) - read_offs);

        if (read_size == -1) {
            errprintf("failed to receive block size (%s)", strerror(errno));
            return -1;
        }

        read_offs += read_size;
    }

    memcpy(block_length, buf, sizeof(buf));

    return 0;
}


int receive_block(int sock_fd, char **block, long *block_length) {
    char buf[BUF_SIZE];
    ssize_t read_size;
    long block_offs = 0, chunk_size;

    /* receive block size */
    if (receive_block_length(sock_fd, block_length) == -1) {
        errprintf("failed to receive block size (%s)", strerror(errno));
        return -1;
    }

    memcpy(block_length, buf, sizeof(*block_length));

#ifndef NDEBUG
    printf("received block size: ");
    print_long_hex(*block_length);
    printf(" (%ld)\n", *block_length);
#endif

    /* allocate block */
    *block = malloc(*block_length);
    if (!*block) {
        errprintf("failed to allocate block");
        return -1;
    }

    /* receive block data */
    while (block_offs < *block_length) {
        chunk_size = BUF_SIZE;
        if (block_offs + chunk_size > *block_length)
            chunk_size = *block_length - block_offs;

        if ((read_size = read(sock_fd, buf, chunk_size)) == -1) {
            errprintf("failed to receive data (%s)", strerror(errno));
            free(block);
            return -1;
        }

        if (block_offs + read_size > *block_length)
            read_size = *block_length - block_offs;

        memcpy(*block + block_offs, buf, read_size);

        block_offs += read_size;
    }

#ifndef NDEBUG
    printf("received block: ");
    print_block_preview(*block, *block_length);
    printf(" (%ld bytes)\n", block_offs);
#endif

    return 0;
}
