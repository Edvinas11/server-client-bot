#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

typedef struct {
    int sock;
    char username[50];
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int clientCount = 0;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

int centralSock;

void add_client(int sock, const char *username) {
    pthread_mutex_lock(&clientsMutex);
    if (clientCount < MAX_CLIENTS) {
        clients[clientCount].sock = sock;
        strncpy(clients[clientCount].username, username, sizeof(clients[clientCount].username)-1);
        clientCount++;
    }
    pthread_mutex_unlock(&clientsMutex);
}

void remove_client_by_username(const char *username) {
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < clientCount; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            close(clients[i].sock);
            for (int j = i; j < clientCount - 1; j++) {
                clients[j] = clients[j+1];
            }
            clientCount--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);
}

// Gija, kuri laiko komandas iš centrinio serverio (pvz., BLOCK)
void *listen_central(void *arg) {
    char buffer[BUFFER_SIZE];
    int r;
    while ((r = recv(centralSock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[r] = '\0';
        printf("Gauta iš centrinio: %s", buffer);
        if (strncmp(buffer, "BLOCK", 5) == 0) {
            char blockedName[50];
            sscanf(buffer, "BLOCK %49s", blockedName);
            printf("Blokuojamas vartotojas: %s\n", blockedName);
            remove_client_by_username(blockedName);
        }
    }
    return NULL;
}

void *handle_client(void *arg) {
    int clientSock = *((int*)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    char username[50] = {0};
    int r;

    // Registracijos seka:
    char *req = "ATSIUSKVARDA\n";
    send(clientSock, req, strlen(req), 0);

    r = recv(clientSock, buffer, BUFFER_SIZE - 1, 0);
    if (r <= 0) {
        close(clientSock);
        return NULL;
    }
    buffer[r] = '\0';
    sscanf(buffer, "%49s", username);

    // Siunčiame registracijos užklausą centriniam serveriui
    char regMsg[BUFFER_SIZE];
    snprintf(regMsg, sizeof(regMsg), "REGISTER:%s\n", username);
    send(centralSock, regMsg, strlen(regMsg), 0);

    // Iš karto leidžiame klientui rašyti
    char *approval = "VARDASOK\n";
    send(clientSock, approval, strlen(approval), 0);

    add_client(clientSock, username);
    printf("Klientas prisijungė: %s\n", username);

    // Priimame žinutes iš kliento ir persiunčiame centriniam serveriui
    while ((r = recv(clientSock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[r] = '\0';
        if (strncmp(buffer, "exit", 4) == 0)
            break;
        char msg[BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "username:%s;msg:%s\n", username, buffer);
        send(centralSock, msg, strlen(msg), 0);
    }
    remove_client_by_username(username);
    close(clientSock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Naudojimas: %s <vietinis_prievadas> <centrinio_ip> <centrinio_prievadas>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int localPort = atoi(argv[1]);
    char *centralIP = argv[2];
    int centralPort = atoi(argv[3]);

    // Prisijungiame prie centrinio serverio
    centralSock = socket(AF_INET, SOCK_STREAM, 0);
    if (centralSock < 0) {
        perror("central socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in centralAddr;
    memset(&centralAddr, 0, sizeof(centralAddr));
    centralAddr.sin_family = AF_INET;
    centralAddr.sin_port = htons(centralPort);
    inet_pton(AF_INET, centralIP, &centralAddr.sin_addr);
    if (connect(centralSock, (struct sockaddr *)&centralAddr, sizeof(centralAddr)) < 0) {
        perror("Nepavyko prisijungti prie centrinio");
        close(centralSock);
        exit(EXIT_FAILURE);
    }
    // Identifikuojame save kaip tarpini serveris
    char *idMsg = "INTERMEDIATE\n";
    send(centralSock, idMsg, strlen(idMsg), 0);

    // Paleidžiame giją, kuri klausosi centrinio serverio komandų
    pthread_t centralThread;
    pthread_create(&centralThread, NULL, listen_central, NULL);
    pthread_detach(centralThread);

    // Sukuriame serverį, kad priimtume klientų jungtis
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("vietinis socket");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(localPort);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Tarpinis serveris klausosi prievade %d...\n", localPort);

    while (1) {
        int *new_sock = malloc(sizeof(int));
        *new_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*new_sock < 0) {
            perror("accept");
            free(new_sock);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, new_sock);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}
