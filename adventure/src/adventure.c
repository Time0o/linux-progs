#define _GNU_SOURCE

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>

#include "rooms.h"
#include "util.h"


/* program name */
char *progname;


/* main function */
int main(int argc, char **argv) {
    char *buf, connections[100], *prompt, *prompt_fmt, *room_dir, **room_names;

    int conn_idx, next_room_valid, num_rooms, num_room_names, path_length, ret,
        room_idx, room_name_idx;

    struct room *current_room, *next_room, *rooms;
    struct ll path_head, *path_tail, *path_node, *tmp;

    (void) argc;

    progname = basename(argv[0]);

    ret = 1;

    /* create room directory */
    room_dir = NULL;
    if (create_room_dir(&room_dir) == -1)
        return 1;

    /* parse room names */
    room_names = NULL;
    num_room_names = read_room_names(&room_names);
    if (num_room_names == -1)
        goto cleanup1;

    /* create rooms */
    if (create_rooms(room_dir, room_names, num_room_names) == -1)
        goto cleanup2;

    /* read rooms in again */
    rooms = NULL;
    if ((num_rooms = read_rooms(room_dir, &rooms)) == -1)
        goto cleanup3;

    ret = 0;

    /* find starting room */
    current_room = &rooms[0];
    for (room_idx = 0; room_idx < NUM_ROOMS; ++room_idx) {
        if (rooms[room_idx].type == START_ROOM)
            current_room = &rooms[room_idx];
    }

    /* store path taken in linked list */
    path_length = 0;
    path_head.val = current_room->name;
    path_head.next = NULL;

    path_tail = &path_head;

    /* start readline loop */
    for (;;) {
        connections[0] = '\0';
        for (conn_idx = 0; conn_idx < MAX_CONN; ++conn_idx) {
            if (!(next_room = current_room->connections[conn_idx]))
                break;

            strcat(connections, next_room->name);

            if (conn_idx < MAX_CONN - 1
                && current_room->connections[conn_idx + 1]) {

                strcat(connections, ", ");
            }
        }

        prompt_fmt = "CURRENT LOCATION: %s\nPOSSIBLE CONNECTIONS: %s\nWHERE TO? >";
        if (asprintf(&prompt, prompt_fmt, current_room->name, connections) == -1)
            errprintf("failed to create prompt");

        buf = readline(prompt);

        free(prompt);

        if (!buf)
            break;

        putchar('\n');

        next_room_valid = 0;
        for (conn_idx = 0; conn_idx < MAX_CONN; ++conn_idx) {
            if (!(next_room = current_room->connections[conn_idx]))
                break;

            if (strcmp(buf, next_room->name) == 0) {
                next_room_valid = 1;
                break;
            }
        }

        free(buf);

        if (next_room_valid) {
            /* update current room */
            current_room = next_room;
            ++path_length;

            /* add room to path */
            path_tail->next = malloc(sizeof(struct ll));
            path_tail->next->val = current_room->name;
            path_tail->next->next = NULL;
            path_tail = path_tail->next;

            if (current_room->type == END_ROOM) {
                printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");

                /* display path taken*/
                printf("YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n",
                       path_length);

                path_node = path_head.next;
                while (path_node) {
                    printf((path_node->next ? "%s\n" : "%s"), path_node->val);
                    path_node = path_node->next;
                }

                /* deallocate path list */
                path_node = path_head.next;
                while (path_node) {
                    tmp = path_node->next;
                    free(path_node);
                    path_node = tmp;
                }

                break;
            }
        } else {
            printf("HUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");
        }
    }

    putchar('\n');

cleanup3:
    for (room_idx = 0; room_idx < num_rooms; ++room_idx)
        free(rooms[room_idx].name);

    free(rooms);

cleanup2:
    for (room_name_idx = 0; room_name_idx < num_room_names; ++room_name_idx)
        free(room_names[room_name_idx]);

    free(room_names);

cleanup1:
    free(room_dir);

    return ret;
}
