#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CONNECTIONS 20
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    int isAdmin; // 1 – jei tai admin bot, 0 – jei tarpinis serveris
} Connection;

Connection connections[MAX_CONNECTIONS];
int connectionCount = 0;
pthread_mutex_t connMutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_to_intermediates(const char *msg, int excludeSock) {
    pthread_mutex_lock(&connMutex);
    for (int i = 0; i < connectionCount; i++) {
        if (!connections[i].isAdmin && connections[i].sockfd != excludeSock) {
            send(connections[i].sockfd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&connMutex);
}

void broadcast_to_admins(const char *msg, int excludeSock) {
    pthread_mutex_lock(&connMutex);
    for (int i = 0; i < connectionCount; i++) {
        if (connections[i].isAdmin && connections[i].sockfd != excludeSock) {
            send(connections[i].sockfd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&connMutex);
}

void remove_connection(int sock) {
    pthread_mutex_lock(&connMutex);
    for (int i = 0; i < connectionCount; i++) {
        if (connections[i].sockfd == sock) {
            for (int j = i; j < connectionCount - 1; j++) {
                connections[j] = connections[j+1];
            }
            connectionCount--;
            break;
        }
    }
    pthread_mutex_unlock(&connMutex);
}

void *handle_connection(void *arg) {
    int sock = *((int*)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Pirmoji žinutė identifikuoja jungtį: jei „ADMIN“, tai admin bot
    int r = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (r <= 0) {
        close(sock);
        return NULL;
    }
    buffer[r] = '\0';
    int isAdmin = 0;
    if (strncmp(buffer, "ADMIN", 5) == 0) {
        isAdmin = 1;
        printf("Admin bot prisijungė.\n");
    } else {
        printf("Tarpinis serveris prisijungė.\n");
    }

    pthread_mutex_lock(&connMutex);
    if (connectionCount < MAX_CONNECTIONS) {
        connections[connectionCount].sockfd = sock;
        connections[connectionCount].isAdmin = isAdmin;
        connectionCount++;
    } else {
        printf("Pasiektas maksimalus jungčių skaičius.\n");
        close(sock);
        pthread_mutex_unlock(&connMutex);
        return NULL;
    }
    pthread_mutex_unlock(&connMutex);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        r = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (r <= 0) break;
        buffer[r] = '\0';
        printf("Gauta žinutė: %s", buffer);
        if (isAdmin) {
            // Admin boto žinutės – persiunčiame į tarpinius serverius
            broadcast_to_intermediates(buffer, sock);
        } else {
            // Tarpinio serverio žinutės – persiunčiame į admin botus
            broadcast_to_admins(buffer, sock);
        }
    }

    printf("Jungtis uždaryta.\n");
    remove_connection(sock);
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Naudojimas: %s <prievadas>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Centrinis serveris klausosi prievade %d...\n", port);

    while (1) {
        int *new_sock = malloc(sizeof(int));
        *new_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*new_sock < 0) {
            perror("accept");
            free(new_sock);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_connection, new_sock);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
