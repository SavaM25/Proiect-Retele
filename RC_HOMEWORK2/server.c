#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 2505

int nrClienti = 0;
pthread_mutex_t clientCountLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int idThread;
    int clientSocket;
} ThreadData;

void *clientThread(void *arg)
{
    ThreadData td = *((ThreadData *)arg);
    free(arg);

    printf("[Thread %d] Pornit pentru socket %d\n", td.idThread, td.clientSocket);

    handleClient(&td);

    close(td.clientSocket);

    pthread_mutex_lock(&clientCountLock);
    nrClienti--;
    printf("[SERVER] Client deconectat. Clienți activi: %d\n", nrClienti);
    pthread_mutex_unlock(&clientCountLock);

    return NULL;
}

void handleClient(ThreadData *td)
{
    int len;

    while (1)
    {
        int bytesRead = readExact(td->clientSocket, &len, sizeof(int));

        if (bytesRead <= 0)
        {
            //se ajunge aici daca clientul s-a deconectat
            if (bytesRead == 0)
            {
                printf("[Thread %d] Clientul a închis conexiunea.\n", td->idThread);
            }
            else
            {
                printf("[Thread %d] Eroare la citirea lungimii. errno: %d\n", td->idThread, errno);
            }
            break;
        }

        if (len <= 0 || len > 4096)
        {
            printf("[Thread %d] Lungime invalidă (%d). Deconectare.\n", td->idThread, len);
            break;
        }

        // citire comanda
        char *command = malloc(len + 1);
        if (!command)
        {
            printf("[Thread %d] Eroare alocare memorie.\n", td->idThread);
            break;
        }

        if (readExact(td->clientSocket, command, len) <= 0)
        {
            printf("[Thread %d] Eroare la citirea comenzii.\n", td->idThread);
            free(command);
            break;
        }
        command[len] = '\0';

        printf("[Thread %d] Comandă primită: %s\n", td->idThread, command);

        //generare raspuns
        char responseBuffer[4150];
        int n = snprintf(responseBuffer, sizeof(responseBuffer), "RESPONS TO (%s)", command);

        //verificare snprintf
        if (n < 0 || n >= sizeof(responseBuffer))
        {
            printf("[Thread %d] Eroare sau buffer prea mic pentru răspuns.\n", td->idThread);
            free(command);
            continue;
        }

        int respLen = n;

        if (writeExact(td->clientSocket, &respLen, sizeof(int)) <= 0)
        {
            printf("[Thread %d] Eroare la trimiterea lungimii răspunsului.\n", td->idThread);
            free(command);
            break;
        }

        if (writeExact(td->clientSocket, responseBuffer, respLen) <= 0)
        {
            printf("[Thread %d] Eroare la trimiterea răspunsului.\n", td->idThread);
            free(command);
            break;
        }

        free(command);
    }
}

int readExact(int socket, void *buffer, int size)
{
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

int writeExact(int socket, const void *buffer, int size)
{
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
    int serverSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int threadCounter = 0;

    //creare socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("socket()");
        return errno;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //configurare struct server
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("bind()");
        return errno;
    }

    if (listen(serverSocket, 10) < 0)
    {
        perror("listen()");
        return errno;
    }

    printf("[SERVER] Rulează pe portul %d...\n", PORT);

    while (1)
    {
        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientSocket < 0)
        {
            perror("accept()");
            continue;
        }

        pthread_mutex_lock(&clientCountLock);
        nrClienti++;
        printf("[SERVER] Client conectat. Clienți activi: %d\n", nrClienti);
        pthread_mutex_unlock(&clientCountLock);

        //creare structura thread
        ThreadData *td = malloc(sizeof(ThreadData));
        td->idThread = threadCounter++;
        td->clientSocket = clientSocket;

        //creare thread
        pthread_t th;
        pthread_create(&th, NULL, clientThread, td);
        pthread_detach(th); // IMPORTANT!!!
    }

    return 0;
}
