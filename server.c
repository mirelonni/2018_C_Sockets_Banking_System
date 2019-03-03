#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 255
#define BUFLEN 256

struct user {
    char name[12];
    char lastName[12];
    int card;
    short pin;
    char pass[8];
    double balance;
    char state;        // the state of the user
    char sock;         // the user tcp socket
    double sum_to_trans;
    int who_to_trans;
};

struct terminal {
    int card;
    char attempts;
}; // used at login to count the attempts

void error(char *msg) {
    perror(msg);
    exit(-1);
}

/*
   Function returns the position of a user in an array by card number
 */
int find_user_by_card(struct user *users, int n, int card_no) {
    for (int i = 0; i < n; i++) {
        if (users[i].card == card_no) {
            return i;
        }
    }

    return -1;
}

/*
   Function returns the position of a user in an array by the socket number
 */
int find_user_by_sock(struct user *users, int n, int sock_no) {
    for (int i = 0; i < n; i++) {
        if (users[i].sock == sock_no) {
            return i;
        }
    }

    return -1;
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno, clilen, index;
    int sockfd2;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, usr, card = 0, dest;
    char mesaj[BUFLEN];
    char strbalance[20];
    char pass[8];
    struct terminal terminals[MAX_CLIENTS];

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

    if (argc < 3) {
        fprintf(stderr, "Usage : %s port_server user_data_file\n", argv[0]);
        exit(1);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    sockfd2 = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    portno = atoi(argv[1]);

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    memset((char *)&cli_addr, 0, sizeof(cli_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) <
        0) {
        error("ERROR on binding");
    }

    if (bind(sockfd2, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) <
        0) {
        error("ERROR on binding");
    }

    listen(sockfd, MAX_CLIENTS);

    FD_SET(0, &read_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd2, &read_fds);

    fdmax = sockfd;

    if (fdmax < sockfd2) {
        fdmax = sockfd2;
    }

    int no_clients = 0;
    FILE *fp;
    fp = fopen(argv[2], "r");
    fscanf(fp, "%d", &no_clients);
    struct user users[no_clients];

    for (i = 0; i < no_clients; i++) {

        fscanf(fp, "%s %s %d %hu %s %lf", users[i].name, users[i].lastName,
               &users[i].card, &users[i].pin, users[i].pass, &users[i].balance);

        // default for all users
        users[i].state = 0;
        users[i].sock = -1;
        users[i].sum_to_trans = 0;
        users[i].who_to_trans = -1;
    }

    fclose(fp);

    for (i = 0; i < MAX_CLIENTS; i++) {
        // init for each terminals
        terminals[i].card = 0;
        terminals[i].attempts = 0;
    }

    while (1) {

        tmp_fds = read_fds;

        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
            error("ERROR in select");
        }

        for (i = 0; i <= fdmax; i++) {

            if (FD_ISSET(i, &tmp_fds)) {

                if (i == sockfd) {

                    clilen = sizeof(cli_addr);

                    if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
                                            &clilen)) == -1) {
                        error("ERROR in accept");
                    } else {
                        FD_SET(newsockfd, &read_fds);

                        if (newsockfd > fdmax) {
                            fdmax = newsockfd;
                        }
                    }

                } else if (i == 0) { // stdin

                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN, stdin);

                    // the only server commnad is quit
                    if (strncmp(buffer, "quit", 4) == 0) {
                        for (int j = 5; j <= fdmax; j++) {
                            // when the server quits send a termination message on each terminal
                            if (FD_ISSET(j, &read_fds)) {
                                n = send(j, buffer, strlen(buffer), 0);
                                FD_CLR(j, &read_fds);
                            }

                            if (n < 0) {
                                error("Send error");
                            }
                        }

                        close(sockfd);
                        close(sockfd2);
                        exit(0);
                    }

                } else if (i == sockfd2) { // UDP

                    memset(buffer, 0, BUFLEN);
                    memset(mesaj, 0, BUFLEN);

                    n = recvfrom(sockfd2, buffer, BUFLEN, 0, (struct sockaddr *)&cli_addr,
                                 &clilen);

                    memset(pass, 0, 8);
                    char *pch;
                    pch = strtok(buffer, " ");

                    if (strncmp(buffer, "unlock", 6) == 0) { // message is "unlock <nr_card>"
                        pch = strtok(NULL, " ");
                        card = atoi(pch);

                        usr = find_user_by_card(users, no_clients, card);

                        // server awaits the first part of the unlock procedure
                        if (usr >= 0) {
                            if (users[usr].state == 3) {
                                // server requests from the client the secret password and puts the state to "unlocking"
                                strcpy(mesaj, "UNLOCK> Send the secret password");
                                users[usr].state = 4;
                            } else if (users[usr].state == 4) {
                                // if the server is already in the "unlocking" state it refuses the message "unlock <nr_card>""
                                strcpy(mesaj, "UNLOCK> -7 : Unlock failed");
                            } else {
                                strcpy(mesaj, "UNLOCK> -6 : Operation failed");
                            }

                        } else {

                            strcpy(mesaj, "UNLOCK> -4 : Not existing card number");
                        }

                    } else { // message is "<nr_card> <parola secreta>"
                        card = atoi(pch);
                        pch = strtok(NULL, " ");
                        strcpy(pass, pch);
                        usr = find_user_by_card(users, no_clients, card);

                        if (usr >= 0) {
                            if (users[usr].state == 3) {
                                strcpy(mesaj, "UNLOCK> -7 : Unlock failed");
                                // server is not in "unlocking" state and receives the "<nr_card> <parola secreta>" message
                            } else if (users[usr].state == 4) {

                                if (strncmp(pass, users[usr].pass, strlen(users[usr].pass)) ==
                                    0) {
                                    strcpy(mesaj, "UNLOCK> Card unlocked");
                                    // if the card is unlocked reset the attempts and the state
                                    users[usr].state = 0;
                                    index = users[usr].sock;
                                    terminals[index].card = 0;
                                    terminals[index].attempts = 0;

                                    // for loop for when the card was blocked from terminal A and unlocked from terminal B
                                    for (int k = 0; k < MAX_CLIENTS; k++) {
                                        if (terminals[k].card == users[usr].card) {
                                            terminals[k].card = 0;
                                            terminals[k].attempts = 0;
                                        }
                                    }
                                } else {
                                    strcpy(mesaj, "UNLOCK> -7 : Unlock failed");

                                    users[usr].state = 3;
                                }
                            } else {
                                strcpy(mesaj, "UNLOCK> -6 : Operation failed");
                            }

                        } else {

                            strcpy(mesaj, "UNLOCK> -4 : Not existing card number");
                        }
                    }

                    n = sendto(sockfd2, mesaj, strlen(mesaj), 0,
                               (const struct sockaddr *)&cli_addr, clilen);

                } else { // TCP

                    memset(buffer, 0, BUFLEN);

                    if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        if (n == 0) {
                            // when the client drops out
                            printf("server: socket %d hung up\n", i);
                            usr = find_user_by_sock(users, no_clients, i);
                            users[usr].state = 0;
                            users[usr].sum_to_trans = 0;
                            users[usr].who_to_trans = -1;
                            users[usr].sock = -1;
                            terminals[i].card = 0;
                            terminals[i].attempts = 0;
                        } else {
                            error("ERROR in recv");
                        }

                        close(i);
                        FD_CLR(i, &read_fds);
                    }

                    else {

                        memset(mesaj, 0, BUFLEN);

                        char *pch;
                        pch = strtok(buffer, " ");

                        if (strncmp(pch, "login", 5) == 0) {
                            if (users[find_user_by_sock(users, no_clients, i)].state != 2) {
                                pch = strtok(NULL, " ");
                                card = atoi(pch);
                                pch = strtok(NULL, " ");
                                int pin = atoi(pch);
                                usr = find_user_by_card(users, no_clients, card);

                                if (card != terminals[i].card) {
                                    // reset the terminal attempts when the card number is changed
                                    terminals[i].attempts = 0;
                                    terminals[i].card = card;
                                }

                                if (usr >= 0) {
                                    users[usr].sock = i;

                                    if (users[usr].state == 1) {
                                        strcpy(mesaj, "IBANK> -2 : Session already open");
                                    } else if (users[usr].state == 3 || users[usr].state == 4) { // if the card is "locked" or "unlocking"

                                        strcpy(mesaj, "IBANK> -5 : Card locked");
                                    } else {
                                        if (users[usr].pin == pin) { // reset the attempts at correct credentials and set the card as "unlocked"

                                            users[usr].state = 1;
                                            users[usr].sock = i;
                                            terminals[i].card = 0;
                                            terminals[i].attempts = 0;

                                            strcpy(mesaj, "IBANK> Welcome ");
                                            strcat(mesaj, users[usr].name);
                                            strcat(mesaj, " ");
                                            strcat(mesaj, users[usr].lastName);

                                        } else {
                                            terminals[i].attempts++; // increment the terminal attempts at each wrong pin

                                            if (terminals[i].attempts >= 3) { // put the card in the "locked" state at 3 wrong attempts
                                                users[usr].state = 3;
                                                strcpy(mesaj, "IBANK> -5 : Card locked");
                                            } else {
                                                strcpy(mesaj, "IBANK> -3 : Wrong pin");
                                            }
                                        }
                                    }

                                } else {
                                    strcpy(mesaj, "IBANK> -4 : Not existing card number");
                                }
                            } else {
                                memset(mesaj, 0, BUFLEN);
                                strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                users[usr].state = 1;
                                users[usr].sum_to_trans = 0;
                                users[usr].who_to_trans = 0;
                            }

                        } else if (strncmp(pch, "logout", 6) == 0) {

                            usr = find_user_by_sock(users, no_clients, i);

                            if (usr >= 0) {
                                if (users[usr].state == 2) { // message sent to terminal when in "transfer" but the first letter is not 'y'
                                    memset(mesaj, 0, BUFLEN);
                                    strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                    users[usr].state = 1;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;
                                } else { // normal use case
                                    users[usr].state = 0;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = -1;
                                    users[usr].sock = -1;
                                    terminals[i].card = 0;
                                    terminals[i].attempts = 0;
                                    strcpy(mesaj, "IBANK> Client disconnected");
                                }
                            } else {
                                strcpy(mesaj, "IBANK> -1 : Client not logged in");
                            }

                        } else if (strncmp(pch, "listbalance", 11) == 0) {

                            usr = find_user_by_sock(users, no_clients, i);

                            if (usr >= 0) {
                                strcpy(mesaj, "IBANK> : ");
                                memset(strbalance, 0, 20);
                                sprintf(strbalance, "%.2f", users[usr].balance);
                                strcat(mesaj, strbalance);

                                if (users[usr].state == 2) { // message sent to terminal when in "transfer" but the first letter is not 'y'
                                    memset(mesaj, 0, BUFLEN);
                                    strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                    users[usr].state = 1;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;
                                }
                            } else {
                                strcpy(mesaj, "IBANK> -1 : Client not logged in");
                            }

                        } else if (strncmp(pch, "transfer", 8) == 0) {

                            pch = strtok(NULL, " ");
                            card = atoi(pch);
                            pch = strtok(NULL, " ");
                            double sum = atof(pch);
                            usr = find_user_by_sock(users, no_clients, i);
                            dest = find_user_by_card(users, no_clients, card);

                            if (usr >= 0) {
                                if (users[usr].state == 2) { // message sent to terminal when in "transfer" but the first letter is not 'y'
                                    strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                    users[usr].state = 1;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;
                                } else if (dest >= 0) {
                                    if (users[usr].balance >= sum) { // normal use case of transfer
                                        users[usr].sum_to_trans = sum;
                                        users[usr].who_to_trans = dest;
                                        users[usr].state = 2;

                                        strcpy(mesaj, "IBANK> Transfer ");
                                        memset(strbalance, 0, 20);
                                        sprintf(strbalance, "%.2f", sum);
                                        strcat(mesaj, strbalance);
                                        strcat(mesaj, " to ");
                                        strcat(mesaj, users[dest].name);
                                        strcat(mesaj, " ");
                                        strcat(mesaj, users[dest].lastName);
                                        strcat(mesaj, "? [y/n]");
                                    } else {
                                        strcpy(mesaj, "IBANK> -8 : Insufficient funds");
                                    }
                                } else {
                                    strcpy(mesaj, "IBANK> -4 : Not existing card number");
                                }
                            } else {
                                strcpy(mesaj, "IBANK> -1 : Client not logged in");
                            }

                        } else if (strncmp(pch, "quit", 4) == 0) {

                            usr = find_user_by_sock(users, no_clients, i);

                            if (usr >= 0) {
                                if (users[usr].state != 2) { // terminal can quit only if not in "transfer" state

                                    users[usr].state = 0;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = -1;
                                    users[usr].sock = -1;
                                    terminals[i].card = 0;
                                    terminals[i].attempts = 0;

                                    FD_CLR(i, &read_fds);

                                } else {

                                    memset(mesaj, 0, BUFLEN);
                                    strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                    users[usr].state = 1;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;

                                }
                            }

                        } else { // any other message received

                            usr = find_user_by_sock(users, no_clients, i);

                            if (users[usr].state == 2) {
                                if (pch[0] == 'y') { // when in "transfer" state and first letter is 'y'
                                    dest = users[usr].who_to_trans;
                                    users[usr].state = 1;
                                    users[dest].balance += users[usr].sum_to_trans;
                                    users[usr].balance -= users[usr].sum_to_trans;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;

                                    strcpy(mesaj, "IBANK> Transfer done");
                                } else {
                                    strcpy(mesaj, "IBANK> -9 : Operation canceled");
                                    users[usr].state = 1;
                                    users[usr].sum_to_trans = 0;
                                    users[usr].who_to_trans = 0;
                                }
                            }
                        }

                        n = send(i, mesaj, strlen(mesaj), 0);

                        if (n < 0) {
                            error("error send");
                        }
                    }
                }
            }
        }
    }

    close(sockfd);

    return 0;
}
