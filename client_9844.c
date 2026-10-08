/*
 * IE3010 - Network Programming
 * NetMessenger Client
 * Registration: IT23629844
 * Port: 15844
 * NID: NID:6298
 *
 * Usage:
 *   ./client_9844 <server-ip> <username>
 *
 * Example:
 *   ./client_9844 127.0.0.1 dulara
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 15844
#define NID "NID:6298"
#define MAX_LINE 4096
#define MAX_NAME 32
#define MAX_FILE_SIZE (10 * 1024 * 1024)

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
    char *p = buf;
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

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int send_line(int fd, const char *line) {
    char out[MAX_LINE + 2];
    int n = snprintf(out, sizeof(out), "%s\n", line);
    if (n <= 0 || (size_t)n >= sizeof(out)) return -1;
    return send_all(fd, out, (size_t)n);
}

static void print_help(void) {
    puts("Commands:");
    puts("  LIST");
    puts("  BCAST <message>");
    puts("  PMSG <username> <message>");
    puts("  JOIN <room>");
    puts("  LEAVE <room>");
    puts("  ROOMS");
    puts("  RMSG <room> <message>");
    puts("  SENDFILE <target> <filename> <local-path>");
    puts("  QUIT");
}

static void *receiver_thread(void *arg) {
    int fd = *(int *)arg;
    char line[MAX_LINE];

    while (1) {
        int r = recv_line(fd, line, sizeof(line));
        if (r <= 0) break;
        if (r == 2) {
            fprintf(stderr, "[SERVER] line too long\n");
            continue;
        }

        if (strncmp(line, "FILE_FROM ", 10) == 0) {
            char sender[MAX_NAME], filename[256];
            unsigned long long size = 0;
            if (sscanf(line + 10, "%31s %255s %llu",
                       sender, filename, &size) != 3 ||
                size > MAX_FILE_SIZE) {
                fprintf(stderr, "[CLIENT] invalid file header\n");
                break;
            }

            mkdir("received_files", 0755);
            char path[512];
            snprintf(path, sizeof(path), "received_files/%s", filename);

            FILE *f = fopen(path, "wb");
            if (!f) {
                fprintf(stderr, "[CLIENT] cannot open %s\n", path);
                unsigned char *discard = malloc((size_t)size);
                if (!discard) break;
                if (recv_exact(fd, discard, (size_t)size) != 1) {
                    free(discard); break;
                }
                free(discard);
                continue;
            }

            unsigned char buffer[8192];
            unsigned long long remaining = size;
            while (remaining > 0) {
                size_t want = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
                int rr = recv_exact(fd, buffer, want);
                if (rr != 1) {
                    fclose(f);
                    remove(path);
                    return NULL;
                }
                fwrite(buffer, 1, want, f);
                remaining -= want;
            }
            fclose(f);
            printf("\n[FILE] received from %s: %s (%llu bytes) -> %s\n",
                   sender, filename, size, path);
            printf("> ");
            fflush(stdout);
        } else {
            printf("\n%s\n> ", line);
            fflush(stdout);
        }
    }

    fprintf(stderr, "\n[CLIENT] Server connection closed.\n");
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-ip> <username>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    const char *username = argv[2];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP\n");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    char reg[MAX_LINE];
    snprintf(reg, sizeof(reg), "REGISTER %s", username);
    if (send_line(fd, reg) < 0) {
        perror("send");
        close(fd);
        return 1;
    }

    char first[MAX_LINE];
    int rr = recv_line(fd, first, sizeof(first));
    if (rr != 1) {
        fprintf(stderr, "No registration response\n");
        close(fd);
        return 1;
    }
    printf("%s\n", first);
    if (strncmp(first, "OK ", 3) != 0) {
        close(fd);
        return 1;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, receiver_thread, &fd) != 0) {
        perror("pthread_create");
        close(fd);
        return 1;
    }
    pthread_detach(tid);

    print_help();

    char input[MAX_LINE];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\0';
        if (!*input) continue;

        if (strncmp(input, "SENDFILE ", 9) == 0) {
            char target[MAX_NAME], filename[256], path[512];
            if (sscanf(input + 9, "%31s %255s %511s", target, filename, path) != 3) {
                puts("Usage: SENDFILE <target> <filename> <local-path>");
                continue;
            }

            FILE *f = fopen(path, "rb");
            if (!f) { perror("fopen"); continue; }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz <= 0 || (unsigned long long)sz > MAX_FILE_SIZE) {
                fclose(f);
                puts("File must be between 1 byte and 10 MB.");
                continue;
            }

            unsigned char *data = malloc((size_t)sz);
            if (!data) {
                fclose(f);
                puts("Out of memory.");
                continue;
            }
            size_t got = fread(data, 1, (size_t)sz, f);
            fclose(f);
            if (got != (size_t)sz) {
                free(data);
                puts("Could not read complete file.");
                continue;
            }

            char header[MAX_LINE];
            snprintf(header, sizeof(header), "SENDFILE %s %s %ld",
                     target, filename, sz);
            if (send_line(fd, header) == 0 && send_all(fd, data, (size_t)sz) == 0)
                printf("[CLIENT] File sent to server.\n");
            else
                perror("send");
            free(data);
        } else if (strcmp(input, "HELP") == 0) {
            print_help();
        } else {
            if (send_line(fd, input) < 0) {
                perror("send");
                break;
            }
            if (strcmp(input, "QUIT") == 0) break;
        }
    }

    close(fd);
    return 0;
}
