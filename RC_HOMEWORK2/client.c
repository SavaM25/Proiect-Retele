/*
    Client TCP compatibil cu serverul multithread.
    Trimitere protocol:
       1) int = lungimea comenzii
       2) string = comanda

    Serverul raspunde identic:
       1) int = lungimea raspunsului
       2) string = raspuns
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.10.100.30"
#define SERVER_PORT 2505

int readExact(int socket, void *buffer, int size) {
    int total = 0;
    int bytes;

    while (total < size)
    {
        bytes = read(socket, buffer + total, size - total);
        if (bytes <= 0)
            return bytes;
        total += bytes;
    }
    return total;
}

int writeExact(int socket, const void *buffer, int size) {
    int total = 0;
    int bytes;

    while (total < size)
    {
        bytes = write(socket, buffer + total, size - total);
        if (bytes <= 0)
            return bytes;
        total += bytes;
    }
    return total;
}

int main()
{
    int sock;
    struct sockaddr_in serverAddr;

    // Creare Socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Eroare socket()");
        return errno;
    }

    // Configurare Server
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0)
    {
        perror("Adresa IP invalidă");
        return errno;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Eroare connect()");
        return errno;
    }
    printf("Client conectat la server (%s:%d)\n", SERVER_IP, SERVER_PORT);

    while (1)
    {
        char comanda[4096];

        printf("> ");
        fflush(stdout);

        if (!fgets(comanda, sizeof(comanda), stdin))
            break;

        comanda[strcspn(comanda, "\n")] = '\0';

        if (strcmp(comanda, "exit") == 0)
            break;

        int len = strlen(comanda);

        // trimitem lungime
        if (writeExact(sock, &len, sizeof(int)) <= 0)
        {
            perror("Eroare la trimiterea lungimii");
            break;
        }

        // trimitem comanda
        if (writeExact(sock, comanda, len) <= 0)
        {
            perror("Eroare la trimiterea comenzii");
            break;
        }

        // citim lungime
        int raspLen = 0;
        if (readExact(sock, &raspLen, sizeof(int)) <= 0)
        {
            perror("Eroare la primirea lungimii răspunsului");
            break;
        }
        if (raspLen < 0 || raspLen > 4096)
        {
            printf("Răspuns invalid de la server!\n");
            break;
        }

        // citim raspuns
        char *raspuns = malloc(raspLen + 1);
        if (!raspuns)
        {
            printf("Eroare alocare memorie\n");
            break;
        }

        if (readExact(sock, raspuns, raspLen) <= 0)
        {
            perror("Eroare la primirea răspunsului");
            free(raspuns);
            break;
        }
        raspuns[raspLen] = '\0';

        printf("[Server] %s\n", raspuns);

        free(raspuns);
    }

    // ------------------ Închidere ------------------
    close(sock);
    printf("Client închis.\n");

    return 0;
}
