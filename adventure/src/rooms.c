#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "rooms.h"
#include "util.h"


static char const *ROOM_DIR_FMT = "%s.rooms.%ju";
static char const *ROOM_NAME_FILE = "../resources/room_names.txt";
char const *ROOM_TYPE_STR[] = { ROOM_TYPES(STRING) };


int read_room_names(char ***room_names) {
    char ch, *line, *room_name_path, *source_file;
    int num_room_names = 0, num_room_names_valid = 0, r = 0, r_ = 0;

    FILE *fp;
    size_t len = 0;

    /* open room name file */
    source_file = malloc(strlen(__FILE__) + 1);
    strcpy(source_file, __FILE__);

    if (asprintf(&room_name_path, "%s/%s", dirname(source_file), ROOM_NAME_FILE) == -1) {
        errprintf("asprintf failed");
        free(source_file);
        return -1;
    }

    free(source_file);

    if (!(fp = fopen(room_name_path, "r"))) {
        errprintf("failed to open '%s'", room_name_path);
        free(room_name_path);
        return -1;
    }

    free(room_name_path);

    /* count number of distinct room names */
    for (;;) {
        ch = fgetc(fp);

        if (ch == EOF) {
            if (ferror(fp)) {
                errprintf("fgetc failed");
                fclose(fp);
                return -1;

            } else {
                break;
            }

        } else if (ch == '\n') {
            ++num_room_names;
        }
    }

    rewind(fp);

    assert(num_room_names >= NUM_ROOMS);

    /* read in room names */
    *room_names = malloc(num_room_names * sizeof(char *));

    for (r = 0; r < num_room_names; ++r) {
        line = NULL;
        len = 0;

        errno = 0;
        if (getline(&line, &len, fp) == -1) {
            free(line);

            if (errno == EINVAL || errno == ENOMEM) {
                errprintf("getline failed");

                r_ = 0;
                for (r_ = 0; r_ < num_room_names_valid; ++r_)
                    free((*room_names)[r_]);

                free((*room_names));

                fclose(fp);

                return -1;

            } else {
                break;
            }
        }

        line[strlen(line) - 1] = '\0';
        (*room_names)[r] = line;

        ++num_room_names_valid;
    }

    fclose(fp);

    return num_room_names;
}


int create_room_dir(char **room_dir) {
    char *username;
    pid_t pid;
    struct stat st;

    username = getlogin();
    pid = getpid();

    if (asprintf(room_dir, ROOM_DIR_FMT, username, (uintmax_t) pid) == -1) {
        errprintf("asprintf failed");
        return -1;
    }

    if (stat(*room_dir, &st) == 0) {
        errprintf("file '%s' already exists", *room_dir);
        free(*room_dir);
        return -1;

    } else if (errno != ENOENT) {
        errprintf("failed to stat file '%s'", *room_dir);
        free(*room_dir);
        return -1;
    }

    if (mkdir(*room_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        errprintf("failed to create directory '%s'", *room_dir);
        free(*room_dir);
        return -1;
    }

    return 0;
}


int create_rooms(char *room_dir, char **room_names, int num_room_names) {
    struct room *next_room_ptr, room, *rooms, *room_ptr;

    int conn, conn_next, conn_prev, num_connections, room_idx, room_idx_next,
        room_name_idx, ret, *room_name_taken, unique;

    /* see rand */
    srand(time(NULL));

    /* create rooms structures */
    rooms = malloc(NUM_ROOMS * sizeof(struct room));
    room_name_taken = calloc(num_room_names, sizeof(int));

    for (room_idx = 0; room_idx < NUM_ROOMS; ++room_idx) {
        /* randomly choose next room name */
        do {
            room_name_idx = rand() % num_room_names;
        } while (room_name_taken[room_name_idx]);

        room.name = room_names[room_name_idx];
        room_name_taken[room_name_idx] = 1;

        /* pre-allocate connection array */
        conn = 0;
        for (conn = 0; conn < MAX_CONN; ++conn)
            room.connections[conn] = NULL;

        /* set room type */
        if (room_idx == 0)
            room.type = START_ROOM;
        else if (room_idx == NUM_ROOMS - 1)
            room.type = END_ROOM;
        else
            room.type = MID_ROOM;

        rooms[room_idx] = room;
    }

    free(room_name_taken);

    /* connect rooms */
    for (room_idx = 0; room_idx < NUM_ROOMS - 1; ++room_idx) {
        room_ptr = &rooms[room_idx];

        num_connections = rand() % (MAX_CONN - MIN_CONN) + MIN_CONN;

        for (conn = 0; conn < num_connections; ++conn) {
            if (room_ptr->connections[conn])
                continue;

            for (;;) {
                room_idx_next = rand() % NUM_ROOMS;

                if (room_idx_next == room_idx)
                    continue;

                next_room_ptr = &rooms[room_idx_next];

                unique = 1;
                for (conn_prev = conn - 1; conn_prev >= 0; --conn_prev) {
                    if (room_ptr->connections[conn_prev] == next_room_ptr) {
                        unique = 0;
                        break;
                    }
                }

                if (!unique)
                    continue;

                conn_next = 0;
                while (next_room_ptr->connections[conn_next])
                    ++conn_next;

                next_room_ptr->connections[conn_next] = room_ptr;

                break;
            }

            room_ptr->connections[conn] = &rooms[room_idx_next];
        }
    }

    ret = write_rooms(room_dir, rooms);

    free(rooms);

    return ret;
}


static void write_room(struct room room, FILE *fp) {
    struct room *next_room;
    int conn = 0;

    fprintf(fp, "ROOM NAME: %s\n", room.name);

    for (;;) {
        next_room = room.connections[conn];
        if (!next_room)
            break;

        fprintf(fp, "CONNECTION %d: %s\n", conn + 1, next_room->name);

        if (++conn >= MAX_CONN)
            break;
    }

    fprintf(fp, "ROOM TYPE: %s\n", ROOM_TYPE_STR[room.type]);
}


int write_rooms(char *room_dir, struct room *rooms) {
    char *file_name;
    FILE *fp;

    struct room room;
    int room_idx = 0;

    for (room_idx = 0; room_idx < NUM_ROOMS; ++room_idx) {
        room = rooms[room_idx];

        file_name = NULL;
        if (asprintf(&file_name, "%s/%s.txt", room_dir, room.name) == -1) {
            errprintf("asprintf failed");
            return -1;
        }

        if (!(fp = fopen(file_name, "w"))) {
            errprintf("failed to open '%s'", file_name);
            free(file_name);
            return -1;
        }

        free(file_name);

        write_room(room, fp);

        fclose(fp);
    }

    return 0;
}


int read_rooms(char *room_dir, struct room **rooms) {
    int conn_idx, lineno, num_rooms = 0, room_idx, room_idx_next, room_type_idx;
    char *c = NULL, *line = NULL, *room_path = NULL, *val = NULL;

    DIR *dir;
    struct dirent *dirent;
    FILE *fp;
    size_t len;
    regex_t valid_record;

    /* open directory */
    if (!(dir = opendir(room_dir))) {
        errprintf("failed to read directory '%s'", room_dir);
        return -1;
    }

    /* count number of rooms (regular files in room directory) */

    while ((dirent = readdir(dir))) {
        if (dirent->d_type != DT_REG)
            continue;

        ++num_rooms;
    }

    rewinddir(dir);

    /* read rooms into array */

    /* allocate room array */
    *rooms = malloc(num_rooms * sizeof(struct room));
    for (room_idx = 0; room_idx < num_rooms; ++room_idx) {
        (*rooms)[room_idx].name = NULL;

        for (conn_idx = 0; conn_idx < MAX_CONN; ++conn_idx)
            (*rooms)[room_idx].connections[conn_idx] = NULL;
    }

    /* compile file record validation regex */
    if (regcomp(&valid_record,
                "^(ROOM NAME|CONNECTION [1-9][0-9]*|ROOM TYPE): .*$",
                REG_EXTENDED)) {

        errprintf("failed to compile regex");
        return -1;
    }

    /* iterate over files in room directory */
    while ((dirent = readdir(dir))) {
        /* skip all non-regular files */
        if (dirent->d_type != DT_REG)
            continue;

        /* open room file */
        room_path = NULL;
        if (asprintf(&room_path, "%s/%s", room_dir, dirent->d_name) == -1) {
            errprintf("asprintf failed");
            goto error1;
        }

        if (!(fp = fopen(room_path, "r"))) {
            errprintf("failed to open '%s'", room_path);
            goto error1;
        }

        /* reset connection index */
        conn_idx = 0;

        /* iterate over lines in room file */
        lineno = 0;
        for (;;) {
            line = NULL;
            len = 0;

            /* read next line */
            errno = 0;
            if (getline(&line, &len, fp) == -1) {
                free(line);

                if (errno == EINVAL || errno == ENOMEM) {
                    errprintf("getline failed");
                    goto error2;

                } else {
                  break;
                }
            }

            line[strlen(line) - 1] = '\0';

            /* check whether line constitutes valid record */
            if (regexec(&valid_record, line, 0, NULL, 0)) {
                errprintf("invalid record '%s' in '%s'", line, room_path);
                goto error2;
            }

            /* obtain record value */
            c = strchr(line, ':') + 1;
            while (isspace(*c))
                ++c;

            val = malloc(strlen(c) + 1);
            strcpy(val, c);

            if (lineno == 0) {
                /* check if room with given name was already encountered */
                room_idx = 0;
                while ((*rooms)[room_idx].name
                       && strcmp(val, (*rooms)[room_idx].name) != 0) {

                    ++room_idx;
                }

                /* otherwise add room name to array */
                if (!(*rooms)[room_idx].name)
                    (*rooms)[room_idx].name = val;
                else
                    free(val);

            } else if (strncmp(line, "ROOM TYPE", strlen("ROOM_TYPE")) == 0) {
                /* read room type */
                for (room_type_idx = 0; room_type_idx < NUM_ROOM_TYPE; ++room_type_idx) {
                    if (strcmp(val, ROOM_TYPE_STR[room_type_idx]) == 0) {
                        (*rooms)[room_idx].type = room_type_idx;
                        break;
                    }
                }

                free(val);

                /* error on invalid room type */
                if (room_type_idx == NUM_ROOM_TYPE) {
                    errprintf("invalid room type '%s'", val);
                    goto error2;
                }

            } else if (strncmp(line, "CONNECTION", strlen("CONNECTION")) == 0) {
                for (room_idx_next = 0; room_idx_next < NUM_ROOMS; ++room_idx_next) {
                    /* add next room to array if not already encountered */
                    if (!(*rooms)[room_idx_next].name) {
                        (*rooms)[room_idx_next].name = val;
                        break;
                    }

                    if (strcmp(val, (*rooms)[room_idx_next].name) == 0) {
                        free(val);
                        break;
                    }
                }

                /* set connection pointer */
                (*rooms)[room_idx].connections[conn_idx] = &(*rooms)[room_idx_next];

                ++conn_idx;
            }

            free(line);

            ++lineno;
        }

        free(room_path);

        fclose(fp);
    }

    closedir(dir);
    regfree(&valid_record);

    return num_rooms;

error2:
    fclose(fp);
    free(line);

error1:
    closedir(dir);
    regfree(&valid_record);

    for (room_idx = 0; room_idx < num_rooms; ++room_idx)
        free((*rooms)[room_idx].name);

    free(*rooms);
    free(room_path);

    return -1;
}
