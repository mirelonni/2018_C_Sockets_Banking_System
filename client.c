#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFLEN 256

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd, sockfd2, n, is_logged = 0, last_card = 0, len, pid, trans;
    char secret = 0;
    struct sockaddr_in serv_addr, serv_addr2;
    struct hostent *server;
    FILE *fp;
    char name[32];

    fd_set read_fds; // read array used in select()
    fd_set tmp_fds;
    int fdmax;

    char buffer[BUFLEN];
    char message[BUFLEN];
    char *pch;

    if (argc < 3) {
        fprintf(stderr, "Usage %s IP_server port_server\n", argv[0]);
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    sockfd2 = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd2 < 0) {
        error("ERROR opening socket");
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    memset((char *)&serv_addr2, 0, sizeof(serv_addr2));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    serv_addr2.sin_family = AF_INET;
    serv_addr2.sin_port = htons(atoi(argv[2]));
    serv_addr2.sin_addr.s_addr = INADDR_ANY;

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(0, &read_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd2, &read_fds);

    fdmax = sockfd;

    if (fdmax < sockfd2) {
        fdmax = sockfd2;
    }

    pid = getpid();
    sprintf(name, "client-%d.log", pid);

    fp = fopen(name, "w"); // output file

    while (1) {

        tmp_fds = read_fds;

        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {

                if (i == sockfd) { // for TCP

                    memset(buffer, 0, BUFLEN);
                    n = recv(sockfd, buffer, BUFLEN, 0);

                    if (n < 0) {
                        error("ERROR writing to socket");
                    }

                    // if else-s for the different messages received from the server
                    if (strncmp(buffer, "quit", 4) == 0) {
                        // if the server closes I close the client
                        close(sockfd);
                        close(sockfd2);
                        exit(0);
                    } else if (strncmp(buffer, "IBANK> Welcome", 14) == 0) {
                        printf("%s\n", buffer);
                        fprintf(fp, "%s\n", buffer);
                        is_logged = 1; // is_logged prevents sending to the server the login command twice

                    } else if (strncmp(buffer, "IBANK> Transfer", 15) == 0) {
                        printf("%s\n", buffer);
                        fprintf(fp, "%s\n", buffer);
                        trans = 1; // flag for "Transfer" 1 for in transfer 0 for not in transfer

                        if (strncmp(buffer, "IBANK> Transfer done", 20) == 0) {
                            trans = 0; // transfer flag resetted for good transfer
                        }
                    } else if (strncmp(buffer, "IBANK> -9", 9) == 0) {
                        printf("%s\n", buffer);
                        fprintf(fp, "%s\n", buffer);
                        trans = 0; // transfer flag resetted for bad transfer
                    } else {

                        printf("%s\n", buffer);
                        fprintf(fp, "%s\n", buffer);
                        fflush(stdout);
                    }

                } else if (i == sockfd2) {   // for UDP

                    memset(buffer, 0, BUFLEN);
                    len = sizeof(serv_addr2);
                    n = recvfrom(sockfd2, buffer, BUFLEN, 0,
                                 (struct sockaddr *)&serv_addr2, &len);
                    buffer[n] = '\0';

                    if (strncmp(buffer, "UNLOCK> Send", 12) == 0) {
                        secret = 1; // if I'm in the course of unlocking(the sever
                        // awaits the secret password) I set the secret flag
                    } else if (strncmp(buffer, "UNLOCK> Card", 12) == 0) {
                        secret = 0; // when unlocked flag is resetted
                    } else if (strncmp(buffer, "UNLOCK> -7", 10) == 0) {
                        secret = 0; // not anymore in the course of unlocking
                    }

                    printf("%s\n", buffer);
                    fprintf(fp, "%s\n", buffer);

                } else if (i == 0) { // stdin

                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    fprintf(fp, "%s", buffer);
                    memcpy(message, buffer, BUFLEN);

                    if (strncmp(buffer, "quit", 4) == 0) {
                        n = send(sockfd, message, strlen(message), 0);

                        if (trans == 0) {

                            fclose(fp);
                            exit(0);
                        }
                    } else if (strncmp(buffer, "login", 5) == 0) {
                        pch = strtok(buffer, " ");
                        pch = strtok(NULL, " ");

                        if (pch != NULL) {
                            last_card = atoi(pch);
                        }

                        if (is_logged == 1) {
                            // only one login at a time is allowed
                            if (trans == 1) {
                                n = send(sockfd, message, strlen(message), 0);
                            } else {
                                printf("-2 : Session already open\n");
                                fprintf(fp, "-2 : Session already open\n");
                            }
                        } else {

                            n = send(sockfd, message, strlen(message), 0);
                        }

                    } else if (strncmp(buffer, "logout", 6) == 0) {

                        if (trans == 0) {
                            is_logged = 0; // resetting the login flag
                        }

                        n = send(sockfd, message, strlen(message), 0);

                    } else if (strncmp(buffer, "unlock", 6) == 0 && secret != 1) {

                        sprintf(message, "unlock %d", last_card);

                        if (trans == 0) {
                            sendto(sockfd2, message, strlen(message), 0,
                                   (const struct sockaddr *)&serv_addr2, sizeof(serv_addr2));
                        } else {
                            n = send(sockfd, message, strlen(message), 0);
                        }

                    } else if (secret == 1) { // if the server asks me for the secret password
                        sprintf(message, "%d %s", last_card, buffer);

                        sendto(sockfd2, message, strlen(message), 0,
                               (const struct sockaddr *)&serv_addr2, sizeof(serv_addr2));

                    } else {
                        // otherwise send the normal message
                        n = send(sockfd, message, strlen(message), 0);
                    }
                }
            }
        }
    }

    fclose(fp);
    return 0;
}
