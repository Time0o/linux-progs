#if !defined ENC && !defined DEC
    #error "either ENC or DEC must be defined"
#endif

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "proto.h"
#include "socket.h"
#include "util.h"


/* program name */
char *progname;


static long read_block(char *file, char **block) {
    FILE *fp;
    int c;
    long block_length = 0, block_offs = 0, chunk_size;

    fp = fopen(file, "r");
    if (!fp) {
        errprintf("failed to open '%s'\n", file);
        goto error;
    }

    /* determine size of block */
    for (;;) {
        c = fgetc(fp);

        if (c == EOF) {
            errprintf("failed to read '%s'\n", file);
            goto error;
        }

        if (c == '\n')
            break;

        if ((c < 'A' && c != ' ') || c > 'Z') {
            errprintf("invalid character '%c' in '%s'", c, file);
            goto error;
        }

        ++block_length;
    }

    rewind(fp);

    /* allocate block */
    *block = malloc(block_length);
    if (!*block) {
        errprintf("failed to allocated block");
        goto error;
    }

    /* read block */
    while (block_offs < block_length) {
        chunk_size = CHUNK_SIZE;
        if (block_offs + chunk_size > block_length)
            chunk_size = block_length - block_offs;

        if (fread(*block + block_offs, 1, chunk_size, fp) != (size_t) chunk_size) {
            errprintf("failed to read '%s'\n", file);
            free(block);
            goto error;
        }

        block_offs += chunk_size;
    }

    fclose(fp);
    return block_length;

error:
    fclose(fp);
    return -1;
}


int main(int argc, char **argv) {
    int port, sock_fd, handshake;
    char *arg_fmt, *text = NULL, *text_modified = NULL, *key = NULL;
    long i, text_length, key_length;
    enum proto proto;
    struct timeval tv;

    /* store program name */
    progname = basename(argv[0]);

    /* parse command line arguments */
    if (argc != 4) {
#if defined ENC
        arg_fmt = "PLAINTEXT KEY PORT";
#elif defined DEC
        arg_fmt = "CIPHERTEXT KEY PORT";
#endif
        fprintf(stderr, "Usage: %s %s\n", progname, arg_fmt);
        exit(EXIT_FAILURE);
    }

    port = strtol_safe(argv[3]);
    if (port == -1) {
        errprintf("failed to parse port parameter");
        exit(EXIT_FAILURE);
    }

    /* read text and key from file */
    if ((text_length = read_block(argv[1], &text)) == -1)
        goto error;

    if ((key_length = read_block(argv[2], &key)) == -1)
        goto error;

    if (key_length < text_length) {
        errprintf("key too short (%lld/%lld)", key_length, text_length);
        goto error;
    }

    /* create socket */
    if ((sock_fd = create_socket(port, SOCKET_CONNECT)) == -1)
        goto error;

    /* send opcode */
#if defined ENC
    proto = PROTO_ENC;
#elif defined DEC
    proto = PROTO_DEC;
#endif

    if (write(sock_fd, &proto, sizeof(proto)) != sizeof(proto)) {
        errprintf("failed to send protocol opcode");
        goto error;
    }

    tv.tv_sec = HANDSHAKE_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)) == -1) {
        errprintf("setsockopt failed");
        goto error;
    }

    if (read(sock_fd, &handshake, sizeof(handshake)) != sizeof(handshake)) {
        errprintf("did not receive handshake from server");
        goto error;
    }

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)) == -1) {
        errprintf("setsockopt failed");
        goto error;
    }

    if (!handshake) {
        errprintf("server did not acknowledge connection");
        goto error;
    }

    /* send text length and text */
    if (send_block(sock_fd, text, text_length) == -1)
        goto error;

    /* send key length and key */
    if (send_block(sock_fd, key, key_length) == -1)
        goto error;

    /* receive (en/de)crypted text */
    if (receive_block(sock_fd, &text_modified, &text_length) == -1)
        goto error;

    /* dump (en/de)crypted text */
    for (i = 0; i < text_length; i += sizeof(int))
        printf("%.*s", (int) sizeof(int), text_modified + i);

    putchar('\n');

    free(text);
    free(key);
    free(text_modified);

    exit(EXIT_SUCCESS);

error:
    free(text);
    free(key);
    free(text_modified);

    exit(EXIT_FAILURE);
}
