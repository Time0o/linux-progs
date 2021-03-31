#if !defined ENC && !defined DEC
    #error "either ENC or DEC must be defined"
#endif

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "proto.h"
#include "socket.h"
#include "util.h"


/* constants */
enum { LISTEN_BACKLOG = 128 };

/* program name */
char *progname;


static char ord(char c) {
    if (c == ' ')
        return 'Z' - 'A' + 1;
    else
        return c - 'A';
}


static char chr(char c) {
    if (c == 'Z' - 'A' + 1)
        return ' ';
    else
        return c + 'A';
}


static void code(char *text, char *key, long text_length) {
    char t, k;
#ifdef DEC
    int tmp;
#endif
    long i;

    for (i = 0; i < text_length; ++i) {
        t = ord(text[i]);
        k = ord(key[i]);

#if defined ENC
        text[i] = chr((t + k) % ('Z' - 'A' + 2));
#elif defined DEC
        tmp = t - k;
        if (tmp < 0)
            tmp += 'Z' - 'A' + 2;

        text[i] = chr(tmp);
#endif
    }
}


int main(int argc, char **argv) {
    int port, sock_fd, client_sock_fd, handshake;
    struct sockaddr_in client_addr;
    unsigned client_addr_size;

    /* store program name */
    progname = basename(argv[0]);

    /* parse command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s PORT\n", progname);
        exit(EXIT_FAILURE);
    }

    if ((port = strtol_safe(argv[1])) == -1) {
        errprintf("failed to parse port argument");
        exit(EXIT_FAILURE);
    }

    /* create socket */
    if ((sock_fd = create_socket(port, SOCKET_BIND)) == -1) {
        errprintf("failed to create socket");
        exit(EXIT_FAILURE);
    }

    /* listen for clients */
    if (listen(sock_fd, LISTEN_BACKLOG) == -1) {
        errprintf("listen failed\n");
        exit(EXIT_FAILURE);
    }

    /* handle client requests */
    client_addr_size = sizeof(client_addr);
    for (;;) {
        client_sock_fd = accept(sock_fd,
                                (struct sockaddr *) &client_addr,
                                &client_addr_size);

        if (client_sock_fd == -1) {
            errprintf("accepting client failed");

        } else {
            /* fork of child process */
            pid_t child;
            switch (child = fork()) {
            case -1:
                errprintf("fork failed");
                break;
            case 0:
                {
                char buf[BUF_SIZE], *text, *key;
                enum proto proto;
                long text_length, key_length;

                /* receive protocol opcode */
                if (read(client_sock_fd, buf, sizeof(proto)) != sizeof(proto)) {
                    errprintf("failed to read opcode (%s)", strerror(errno));
                    _Exit(EXIT_FAILURE);
                }

                memcpy(&proto, buf, sizeof(proto));

#if defined ENC
                if (proto != PROTO_ENC)
#elif defined DEC
                if (proto != PROTO_DEC)
#endif
                {
                    handshake = 0;
                } else {
                    handshake = 1;
                }

                if (write(client_sock_fd, &handshake, sizeof(handshake))
                    != sizeof(handshake)) {

                    errprintf("failed to send handshake");
                    _Exit(EXIT_FAILURE);
                }

                if (!handshake) {
                    errprintf("invalid protocol");
                    _Exit(EXIT_FAILURE);
                }

                /* receive text */
                if (receive_block(client_sock_fd, &text, &text_length) == -1)
                    _Exit(EXIT_FAILURE);

                /* receive key */
                if (receive_block(client_sock_fd, &key, &key_length) == -1) {
                    free(text);
                    _Exit(EXIT_FAILURE);
                }

                if (key_length < text_length) {
                    errprintf("key too short (%lld/%lld)",
                              key_length,
                              text_length);

                    _Exit(EXIT_FAILURE);
                }

                /* (en/de)code text */
                code(text, key, text_length);

                /* send result */
                if (send_block(client_sock_fd, text, text_length) == -1)
                    _Exit(EXIT_FAILURE);
                }
                break;
            default:
                break;
            }
        }
    }

    exit(EXIT_SUCCESS);
}
