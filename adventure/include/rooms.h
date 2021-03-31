#ifndef ROOMS_H
#define ROOMS_H

enum { NUM_ROOMS = 7, NUM_ROOM_TYPE = 3, MIN_CONN = 3, MAX_CONN = 6 };

#define ROOM_TYPES(fn)  \
        fn(START_ROOM), \
        fn(END_ROOM),   \
        fn(MID_ROOM)    \

#define ENUM(e) e

#define STRING(e) #e

enum room_type { ROOM_TYPES(ENUM) };

struct room {
    char *name;
    struct room *connections[MAX_CONN];
    enum room_type type;
};

int read_room_names(char ***room_names);
int create_room_dir(char **room_dir);
int create_rooms(char *room_dir, char **room_names, int num_room_names);
int write_rooms(char *room_dir, struct room *rooms);
int read_rooms(char *room_dir, struct room **rooms);

#endif /* ROOMS_H */
