#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024

typedef struct {
    char username[50];
    time_t joined;
} UserInfo;

static UserInfo users[100];
static int userCount = 0;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Naudojimas: %s <centrinio_ip> <centrinio_prievadas>\n", argv[0]);
        return 1;
    }
    
    char *centralIP = argv[1];
    int centralPort = atoi(argv[2]);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(centralPort);
    inet_pton(AF_INET, centralIP, &serv_addr.sin_addr);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    // Identifikuojame save kaip admin bot
    send(sock, "ADMIN", 5, 0);
    printf("Admin bot prisijungė prie centrinio serverio.\n");
    
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if(r <= 0) break;
        buffer[r] = '\0';
        printf("Gauta: %s", buffer);
        
        // Jei tai registracijos užklausa: "REGISTER:<vardas>\n"
        if (strncmp(buffer, "REGISTER:", 9) == 0) {
            char uname[50];
            sscanf(buffer + 9, "%49s", uname);
            int exists = 0;
            for (int i = 0; i < userCount; i++) {
                if (strcmp(users[i].username, uname) == 0) {
                    exists = 1;
                    break;
                }
            }
            if (exists) {
                printf("Vartotojas %s jau naudojamas.\n", uname);
                char blockCmd[BUFFER_SIZE];
                snprintf(blockCmd, sizeof(blockCmd), "BLOCK %s\n", uname);
                send(sock, blockCmd, strlen(blockCmd), 0);
            } else {
                strcpy(users[userCount].username, uname);
                users[userCount].joined = time(NULL);
                userCount++;
                printf("Registruotas naujas vartotojas: %s\n", uname);
            }
        }
        // Jei tai pokalbių pranešimas: "username:<vardas>;msg:<žinutė>\n"
        else if (strncmp(buffer, "username:", 9) == 0) {
            char uname[50];
            char msg[BUFFER_SIZE];
            char *msgPtr = strstr(buffer, ";msg:");
            if (!msgPtr) continue;
            int len = msgPtr - (buffer + 9);
            if (len > 49) len = 49;
            strncpy(uname, buffer + 9, len);
            uname[len] = '\0';
            msgPtr += 5; // praleidžiame ";msg:"
            strcpy(msg, msgPtr);
            
            // Tikriname, ar pranešime yra blogas žodis "xyz"
            if (strstr(msg, "xyz") != NULL) {
                // Randame registracijos laiką ir jei mažiau nei 60 s, blokuojame
                for (int i = 0; i < userCount; i++) {
                    if (strcmp(users[i].username, uname) == 0) {
                        time_t now = time(NULL);
                        double diff = difftime(now, users[i].joined);
                        if (diff <= 60.0) {
                            printf("Vartotojas %s per pirmą minutę naudojo blogą žodį. Blokuojama.\n", uname);
                            char blockCmd[BUFFER_SIZE];
                            snprintf(blockCmd, sizeof(blockCmd), "BLOCK %s\n", uname);
                            send(sock, blockCmd, strlen(blockCmd), 0);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    close(sock);
    return 0;
}
