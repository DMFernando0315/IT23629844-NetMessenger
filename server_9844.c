/*
 * IE3010 - Network Programming
 * NetMessenger Server
 * Registration: IT23629844
 * Port: 15844
 * NID: NID:6298
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 15844
#define NID "NID:6298"
#define MAX_CLIENTS 50
#define MAX_ROOMS 50
#define MAX_ROOM_MEMBERS 50
#define MAX_NAME 32
#define MAX_LINE 4096
#define MAX_FILE_SIZE (10 * 1024 * 1024)

typedef struct Client Client;
typedef struct Room Room;

struct Client {
    int fd;
    int active;
    int registered;
    char username[MAX_NAME];
    pthread_t thread;
    pthread_mutex_t send_lock;
};

struct Room {
    int active;
    char name[MAX_NAME];
    Client *members[MAX_ROOM_MEMBERS];
    int member_count;
};

static Client clients[MAX_CLIENTS];
static Room rooms[MAX_ROOMS];
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;

static void log_event(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    pthread_mutex_lock(&log_lock);
    FILE *f = fopen("netmsg_IT23629844.log", "a");
    if (f) {
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday,
                tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        vfprintf(f, fmt, ap);
        fputc('\n', f);
        fclose(f);
    }
    pthread_mutex_unlock(&log_lock);
    va_end(ap);
}

static int send_all_raw(Client *c, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(c->fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int send_line(Client *c, const char *line) {
    char out[MAX_LINE + 128];
    int n = snprintf(out, sizeof(out), "%s\n", line);
    if (n < 0 || (size_t)n >= sizeof(out)) return -1;
    pthread_mutex_lock(&c->send_lock);
    int rc = send_all_raw(c, out, (size_t)n);
    pthread_mutex_unlock(&c->send_lock);
    return rc;
}

static int send_ok(Client *c, const char *fmt, ...) {
    char body[MAX_LINE];
    char out[MAX_LINE + 64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    snprintf(out, sizeof(out), "OK %s %s", body, NID);
    return send_line(c, out);
}

static int send_err(Client *c, const char *fmt, ...) {
    char body[MAX_LINE];
    char out[MAX_LINE + 64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    snprintf(out, sizeof(out), "ERR %s %s", body, NID);
    return send_line(c, out);
}

static int recv_line(int fd, char *buf, size_t cap) {
    size_t used = 0;
    while (used + 1 < cap) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ch == '\n') {
            buf[used] = '\0';
            if (used && buf[used-1] == '\r') buf[used-1] = '\0';
            return 1;
        }
        buf[used++] = ch;
    }
    buf[cap-1] = '\0';
    return 2;
}

static int recv_exact(int fd, void *buf, size_t len) {
    char *p = (char *)buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got += (size_t)n;
    }
    return 1;
}

static Client *find_user_locked(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].active && clients[i].registered &&
            strcmp(clients[i].username, name) == 0) return &clients[i];
    return NULL;
}

static Room *find_room_locked(const char *name) {
    for (int i = 0; i < MAX_ROOMS; ++i)
        if (rooms[i].active && strcmp(rooms[i].name, name) == 0) return &rooms[i];
    return NULL;
}

static Room *get_or_create_room_locked(const char *name) {
    Room *r = find_room_locked(name);
    if (r) return r;
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (!rooms[i].active) {
            rooms[i].active = 1;
            rooms[i].member_count = 0;
            strncpy(rooms[i].name, name, MAX_NAME-1);
            rooms[i].name[MAX_NAME-1] = '\0';
            return &rooms[i];
        }
    }
    return NULL;
}

static int room_has_member(Room *r, Client *c) {
    for (int i = 0; i < r->member_count; ++i)
        if (r->members[i] == c) return 1;
    return 0;
}

static void remove_from_room_locked(Room *r, Client *c) {
    for (int i = 0; i < r->member_count; ++i) {
        if (r->members[i] == c) {
            memmove(&r->members[i], &r->members[i+1],
                    (size_t)(r->member_count-i-1) * sizeof(r->members[0]));
            r->member_count--;
            break;
        }
    }
    if (r->member_count == 0) r->active = 0;
}

static void broadcast_presence(const char *kind, const char *username, Client *exclude) {
    Client *targets[MAX_CLIENTS];
    int count = 0;
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].active && clients[i].registered && &clients[i] != exclude)
            targets[count++] = &clients[i];
    pthread_mutex_unlock(&state_lock);

    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "MSG PRESENCE %s %s", kind, username);
    for (int i = 0; i < count; ++i) send_line(targets[i], msg);
}

static void send_users(Client *c) {
    char list[MAX_LINE] = "";
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].registered) {
            if (list[0]) strncat(list, ",", sizeof(list)-strlen(list)-1);
            strncat(list, clients[i].username, sizeof(list)-strlen(list)-1);
        }
    }
    pthread_mutex_unlock(&state_lock);
    send_ok(c, "USERS %s", list);
}

static void send_rooms(Client *c) {
    char list[MAX_LINE] = "";
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (rooms[i].active) {
            if (list[0]) strncat(list, ",", sizeof(list)-strlen(list)-1);
            strncat(list, rooms[i].name, sizeof(list)-strlen(list)-1);
        }
    }
    pthread_mutex_unlock(&state_lock);
    send_ok(c, "ROOMS %s", list);
}

static void deliver_bcast(Client *sender, const char *message) {
    Client *targets[MAX_CLIENTS];
    int count = 0;
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].active && clients[i].registered && &clients[i] != sender)
            targets[count++] = &clients[i];
    pthread_mutex_unlock(&state_lock);

    char out[MAX_LINE];
    snprintf(out, sizeof(out), "MSG BCAST %s %s", sender->username, message);
    for (int i = 0; i < count; ++i) send_line(targets[i], out);
}

static void deliver_private(Client *sender, Client *target, const char *message) {
    char out[MAX_LINE];
    snprintf(out, sizeof(out), "MSG PRIV %s %s", sender->username, message);
    send_line(target, out);
}

static void deliver_room(Client *sender, Room *room, const char *message) {
    Client *targets[MAX_ROOM_MEMBERS];
    int count = 0;
    pthread_mutex_lock(&state_lock);
    for (int i = 0; i < room->member_count; ++i)
        if (room->members[i] != sender) targets[count++] = room->members[i];
    pthread_mutex_unlock(&state_lock);

    char out[MAX_LINE];
    snprintf(out, sizeof(out), "MSG ROOM %s %s %s",
             room->name, sender->username, message);
    for (int i = 0; i < count; ++i) send_line(targets[i], out);
}

static int send_file_to_client(Client *target, const char *sender,
                               const char *filename, const unsigned char *data,
                               size_t size) {
    char header[MAX_LINE];
    int n = snprintf(header, sizeof(header),
                     "FILE_FROM %s %s %zu\n", sender, filename, size);
    if (n <= 0 || (size_t)n >= sizeof(header)) return -1;

    pthread_mutex_lock(&target->send_lock);
    int rc = send_all_raw(target, header, (size_t)n);
    if (rc == 0) rc = send_all_raw(target, data, size);
    pthread_mutex_unlock(&target->send_lock);
    return rc;
}

static void send_file_to_target(Client *sender, const char *target_name,
                                const char *filename, const unsigned char *data,
                                size_t size) {
    Client *user = NULL;
    Room *room = NULL;
    Client *targets[MAX_ROOM_MEMBERS];
    int count = 0;

    pthread_mutex_lock(&state_lock);
    user = find_user_locked(target_name);
    if (!user) room = find_room_locked(target_name);
    if (room) {
        for (int i = 0; i < room->member_count; ++i)
            if (room->members[i] != sender) targets[count++] = room->members[i];
    }
    pthread_mutex_unlock(&state_lock);

    if (!user && !room) {
        send_err(sender, "002 USER_NOT_FOUND");
        return;
    }

    if (user) {
        if (send_file_to_client(user, sender->username, filename, data, size) < 0)
            send_err(sender, "005 DELIVERY_FAILED");
    } else {
        for (int i = 0; i < count; ++i)
            send_file_to_client(targets[i], sender->username, filename, data, size);
    }
    send_ok(sender, "FILE_RECEIVED %s", filename);
    log_event("FILE sender=%s target=%s filename=%s size=%zu",
              sender->username, target_name, filename, size);
}

static void cleanup_client(Client *c) {
    char username[MAX_NAME] = "";
    int was_registered = 0;

    pthread_mutex_lock(&state_lock);
    if (c->registered) {
        strncpy(username, c->username, sizeof(username)-1);
        was_registered = 1;
    }
    for (int i = 0; i < MAX_ROOMS; ++i)
        if (rooms[i].active) remove_from_room_locked(&rooms[i], c);
    c->active = 0;
    c->registered = 0;
    pthread_mutex_unlock(&state_lock);

    if (was_registered) {
        log_event("DISCONNECT user=%s", username);
        broadcast_presence("LEAVE", username, NULL);
    }
    close(c->fd);
}

static int handle_register(Client *c, const char *username) {
    if (!username || !*username || strlen(username) >= MAX_NAME) {
        send_err(c, "001 INVALID_USERNAME");
        return -1;
    }

    pthread_mutex_lock(&state_lock);
    if (find_user_locked(username)) {
        pthread_mutex_unlock(&state_lock);
        send_err(c, "001 USERNAME_TAKEN");
        return 0;
    }
    c->registered = 1;
    strncpy(c->username, username, MAX_NAME-1);
    c->username[MAX_NAME-1] = '\0';
    pthread_mutex_unlock(&state_lock);

    send_ok(c, "REGISTERED %s", c->username);
    log_event("REGISTER user=%s", c->username);
    broadcast_presence("JOIN", c->username, c);
    return 1;
}

static void *client_thread(void *arg) {
    Client *c = (Client *)arg;
    char line[MAX_LINE];

    int r = recv_line(c->fd, line, sizeof(line));
    if (r != 1) { cleanup_client(c); return NULL; }

    char cmd[32], arg1[MAX_NAME];
    if (sscanf(line, "%31s %31s", cmd, arg1) != 2 ||
        strcmp(cmd, "REGISTER") != 0) {
        send_err(c, "001 REGISTER_REQUIRED");
        cleanup_client(c);
        return NULL;
    }
    if (handle_register(c, arg1) < 0) {
        cleanup_client(c);
        return NULL;
    }

    while (running) {
        r = recv_line(c->fd, line, sizeof(line));
        if (r == 0 || r == -1) break;
        if (r == 2) { send_err(c, "400 LINE_TOO_LONG"); continue; }

        if (strncmp(line, "REGISTER ", 9) == 0) {
            send_err(c, "001 ALREADY_REGISTERED");
        } else if (strcmp(line, "LIST") == 0) {
            send_users(c);
        } else if (strncmp(line, "BCAST ", 6) == 0) {
            const char *msg = line + 6;
            if (!*msg) { send_err(c, "400 INVALID_MESSAGE"); continue; }
            deliver_bcast(c, msg);
            send_ok(c, "SENT");
            log_event("BCAST user=%s", c->username);
        } else if (strncmp(line, "PMSG ", 5) == 0) {
            char target_name[MAX_NAME], msg[MAX_LINE];
            if (sscanf(line + 5, "%31s %[^\n]", target_name, msg) < 2) {
                send_err(c, "400 INVALID_MESSAGE");
                continue;
            }
            pthread_mutex_lock(&state_lock);
            Client *target = find_user_locked(target_name);
            pthread_mutex_unlock(&state_lock);
            if (!target) send_err(c, "002 USER_NOT_FOUND");
            else {
                deliver_private(c, target, msg);
                send_ok(c, "SENT");
                log_event("PMSG sender=%s target=%s", c->username, target_name);
            }
        } else if (strncmp(line, "JOIN ", 5) == 0) {
            char room_name[MAX_NAME];
            if (sscanf(line + 5, "%31s", room_name) != 1) {
                send_err(c, "400 INVALID_ROOM");
                continue;
            }
            pthread_mutex_lock(&state_lock);
            Room *room = get_or_create_room_locked(room_name);
            int joined = 0;
            if (room && !room_has_member(room, c) && room->member_count < MAX_ROOM_MEMBERS) {
                room->members[room->member_count++] = c;
                joined = 1;
            }
            pthread_mutex_unlock(&state_lock);
            if (!room) send_err(c, "005 ROOM_LIMIT");
            else if (!joined) send_err(c, "004 ALREADY_IN_ROOM");
            else {
                send_ok(c, "JOINED %s", room_name);
                log_event("JOIN user=%s room=%s", c->username, room_name);
            }
        } else if (strncmp(line, "LEAVE ", 6) == 0) {
            char room_name[MAX_NAME];
            if (sscanf(line + 6, "%31s", room_name) != 1) {
                send_err(c, "400 INVALID_ROOM");
                continue;
            }
            pthread_mutex_lock(&state_lock);
            Room *room = find_room_locked(room_name);
            if (room) remove_from_room_locked(room, c);
            pthread_mutex_unlock(&state_lock);
            if (!room) send_err(c, "003 ROOM_NOT_FOUND");
            else {
                send_ok(c, "LEFT %s", room_name);
                log_event("LEAVE user=%s room=%s", c->username, room_name);
            }
        } else if (strcmp(line, "ROOMS") == 0) {
            send_rooms(c);
        } else if (strncmp(line, "RMSG ", 5) == 0) {
            char room_name[MAX_NAME], msg[MAX_LINE];
            if (sscanf(line + 5, "%31s %[^\n]", room_name, msg) < 2) {
                send_err(c, "400 INVALID_MESSAGE");
                continue;
            }
            pthread_mutex_lock(&state_lock);
            Room *room = find_room_locked(room_name);
            int member = room && room_has_member(room, c);
            pthread_mutex_unlock(&state_lock);
            if (!room) send_err(c, "003 ROOM_NOT_FOUND");
            else if (!member) send_err(c, "006 NOT_IN_ROOM");
            else {
                deliver_room(c, room, msg);
                send_ok(c, "SENT");
                log_event("RMSG user=%s room=%s", c->username, room_name);
            }
        } else if (strncmp(line, "SENDFILE ", 9) == 0) {
            char target[MAX_NAME], filename[256];
            unsigned long long filesize = 0;
            char extra[8];
            int count = sscanf(line + 9, "%31s %255s %llu %7s",
                               target, filename, &filesize, extra);
            if (count != 3 || filesize > MAX_FILE_SIZE || filesize == 0) {
                send_err(c, filesize > MAX_FILE_SIZE ? "004 FILE_TOO_LARGE" :
                         "400 INVALID_FILE_HEADER");
                continue;
            }
            unsigned char *data = malloc((size_t)filesize);
            if (!data) { send_err(c, "500 OUT_OF_MEMORY"); break; }
            int rr = recv_exact(c->fd, data, (size_t)filesize);
            if (rr != 1) { free(data); break; }

            char dir[512], path[768];
            snprintf(dir, sizeof(dir), "./storage/IT23629844/%s", c->username);
            mkdir("./storage", 0755);
            mkdir("./storage/IT23629844", 0755);
            mkdir(dir, 0755);
            snprintf(path, sizeof(path), "%s/%s", dir, filename);
            FILE *f = fopen(path, "wb");
            if (!f) {
                free(data);
                send_err(c, "500 STORAGE_ERROR");
                continue;
            }
            fwrite(data, 1, (size_t)filesize, f);
            fclose(f);

            send_file_to_target(c, target, filename, data, (size_t)filesize);
            free(data);
        } else if (strcmp(line, "QUIT") == 0) {
            send_ok(c, "BYE");
            log_event("QUIT user=%s", c->username);
            break;
        } else {
            send_err(c, "400 INVALID_COMMAND");
        }
    }

    cleanup_client(c);
    return NULL;
}

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    mkdir("storage", 0755);
    mkdir("storage/IT23629844", 0755);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("NetMessenger Server started\n");
    printf("Registration: IT23629844\n");
    printf("Listening on TCP port %d\n", PORT);
    printf("Node ID: %s\n", NID);
    printf("Maximum clients: %d\n", MAX_CLIENTS);
    fflush(stdout);
    log_event("SERVER_START port=%d nid=%s", PORT, NID);

    while (running) {
        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);
        int fd = accept(server_fd, (struct sockaddr *)&cliaddr, &len);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (!running) break;
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&state_lock);
        int idx = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i].active) { idx = i; break; }
        }
        if (idx < 0) {
            pthread_mutex_unlock(&state_lock);
            close(fd);
            continue;
        }

        Client *c = &clients[idx];
        memset(c, 0, sizeof(*c));
        c->fd = fd;
        c->active = 1;
        pthread_mutex_init(&c->send_lock, NULL);
        pthread_mutex_unlock(&state_lock);

        if (pthread_create(&c->thread, NULL, client_thread, c) != 0) {
            close(fd);
            pthread_mutex_lock(&state_lock);
            c->active = 0;
            pthread_mutex_unlock(&state_lock);
        } else {
            pthread_detach(c->thread);
        }
    }

    close(server_fd);
    log_event("SERVER_STOP");
    return 0;
}
