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
  char nume[12];
  char prenume[12];
  int card;
  short pin;
  char pass[8];
  double sold;

  char state;          // starea in care se afla userul descris
                       // de cei 6 parametri de mai sus
  char sock;           // de unde se conecteaza userul prin tcp
  double sum_to_trans; // folosita la etapa intermediara a transferului
  int who_to_trans;    // folosita la etapa intermediara a transferului
};

struct terminal {
  int card;
  char atempts;
}; // folosit la login pentru a tine minte incercarile consecutive de la fiecare
   // client

void error(char *msg) {
  perror(msg);
  exit(-1);
}

// functie care intoarce pozitia unui ures intr-un array de useri dupa numarul
// sau de card
int find_user_by_card(struct user *users, int n, int card_no) {
  for (int i = 0; i < n; i++) {
    if (users[i].card == card_no) {
      return i;
    }
  }
  return -1;
}

// functie care intoarce pozitia unui ures intr-un array de useri dupa socketul
// de pe care s-a conectat
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
  char strsold[20];
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
  // adaug la multima de multiplexare stdin si UDP

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

    fscanf(fp, "%s %s %d %hu %s %lf", users[i].nume, users[i].prenume,
           &users[i].card, &users[i].pin, users[i].pass, &users[i].sold);
    // introduc datele din fisier in memorie

    users[i].state = 0;
    users[i].sock = -1;
    users[i].sum_to_trans = 0;
    users[i].who_to_trans = -1;
    // adaug caracteristicile default ale fiecariu user
  }
  fclose(fp);

  for (i = 0; i < MAX_CLIENTS; i++) {
    terminals[i].card = 0;
    terminals[i].atempts = 0;
    // initializez array-ul de treminale
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

        } else if (i == 0) {

          memset(buffer, 0, BUFLEN);
          fgets(buffer, BUFLEN, stdin);

          // singrua comanda la server este quit deci caz doar pentru aceasta
          if (strncmp(buffer, "quit", 4) == 0) {
            for (int j = 5; j <= fdmax; j++) {
              // pe fiecare socket din cele adaugate in multime trimit un mesaj
              // de quit pentru a anunta clientii de oprirea serverului
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

          // daca primesc pe UDP
        } else if (i == sockfd2) {

          memset(buffer, 0, BUFLEN);
          memset(mesaj, 0, BUFLEN);

          n = recvfrom(sockfd2, buffer, BUFLEN, 0, (struct sockaddr *)&cli_addr,
                       &clilen);

          memset(pass, 0, 8);
          char *pch;
          pch = strtok(buffer, " ");
          // exista doua cazuri:
          // mesajul este unlock <nr_card>
          if (strncmp(buffer, "unlock", 6) == 0) {
            pch = strtok(NULL, " ");
            card = atoi(pch);

            usr = find_user_by_card(users, no_clients, card);

            // in acest caz serverul asteapta sa primeasca prima parte a
            // procedurii de unlock
            if (usr >= 0) {
              if (users[usr].state == 3) {
                // daca cardul este blocat serverul ii trimite clientului
                // instructiunea de a trimite parola secreta si actualizeaza
                // starea cardului la "in curs de deblocare"
                strcpy(mesaj, "UNLOCK> Trimite parola secreta");
                users[usr].state = 4;
              } else if (users[usr].state == 4) {
                // daca cardul este deja "in curs de deblocare" si se primeste
                // un mesaj de tipul unlock <nr_card> i se trimite clientului de
                // pe care s a apelat deblocarea ca aceasta a esuat(NOTA5 din
                // pdf de la deblocare)
                strcpy(mesaj, "UNLOCK> -7 : Deblocare esuata");
              } else {
                strcpy(mesaj, "UNLOCK> -6 : Operatie esuata");
              }

            } else {

              strcpy(mesaj, "UNLOCK> -4 : Numar card inexistent");
            }
            // mesajul este <nr_card> <parola secreta>
          } else {
            card = atoi(pch);
            pch = strtok(NULL, " ");
            strcpy(pass, pch);
            usr = find_user_by_card(users, no_clients, card);

            if (usr >= 0) {
              if (users[usr].state == 3) {
                strcpy(mesaj, "UNLOCK> -7 : Deblocare esuata");
                // vice-versa cu primele conditii, daca cardul este blocat dar
                // primeste mesaj cu <nr_card> <parola>
              } else if (users[usr].state == 4) {

                if (strncmp(pass, users[usr].pass, strlen(users[usr].pass)) ==
                    0) {
                  strcpy(mesaj, "UNLOCK> Card deblocat");
                  // daca am deblocat cardul se reseteaza atempturile si
                  // state-ul
                  users[usr].state = 0;
                  index = users[usr].sock;
                  terminals[index].card = 0;
                  terminals[index].atempts = 0;
                  // for pentru cazul in care s-au dat 3 comenzi gresite de
                  // login de pe un terminal si cardul s-a deblocat de pe alt
                  // terminal
                  for (int k = 0; k < MAX_CLIENTS; k++) {
                    if (terminals[k].card == users[usr].card) {
                      terminals[k].card = 0;
                      terminals[k].atempts = 0;
                    }
                  }
                } else {
                  strcpy(mesaj, "UNLOCK> -7 : Deblocare esuata");

                  users[usr].state = 3;
                }
              } else {
                strcpy(mesaj, "UNLOCK> -6 : Operatie esuata");
              }

            } else {

              strcpy(mesaj, "UNLOCK> -4 : Numar card inexistent");
            }
          }

          n = sendto(sockfd2, mesaj, strlen(mesaj), 0,
                     (const struct sockaddr *)&cli_addr, clilen);

        } else {

          memset(buffer, 0, BUFLEN);

          if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
            if (n == 0) {
              // am lasat acest mesaj din lab08 in cazul in care un client
              // opreste fortat executia
              printf("server: socket %d hung up\n", i);
              usr = find_user_by_sock(users, no_clients, i);
              users[usr].state = 0;
              users[usr].sum_to_trans = 0;
              users[usr].who_to_trans = -1;
              users[usr].sock = -1;
              terminals[i].card = 0;
              terminals[i].atempts = 0;
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

            // if else-uri pentru toate cazurile de mesaje primite prin TCP
            if (strncmp(pch, "login", 5) == 0) {
              if (users[find_user_by_sock(users, no_clients, i)].state != 2) {
                pch = strtok(NULL, " ");
                card = atoi(pch);
                pch = strtok(NULL, " ");
                int pin = atoi(pch);
                usr = find_user_by_card(users, no_clients, card);
                if (card != terminals[i].card) {
                  // se reseteaza atempturile daca de pe terminalul de pe care
                  // se
                  // trimite cererea se incearca un alt numar de card
                  terminals[i].atempts = 0;
                  terminals[i].card = card;
                }
                if (usr >= 0) {
                  users[usr].sock = i;
                  if (users[usr].state == 1) {
                    strcpy(mesaj, "IBANK> -2 : Sesiune deja deschisa");
                  } else if (users[usr].state == 3 || users[usr].state == 4) {
                    // daca cardul este blocat sau in curs de deblocare mesajul
                    // primit este ca acesta este bocat
                    strcpy(mesaj, "IBANK> -5 : Card blocat");
                  } else {
                    if (users[usr].pin == pin) {

                      users[usr].state = 1;
                      users[usr].sock = i;
                      terminals[i].card = 0;
                      terminals[i].atempts = 0;

                      // la credentiale corecte se reseteaza atempturile si se
                      // seteaza starea cardului corespunzatoare

                      strcpy(mesaj, "IBANK> Welcome ");
                      strcat(mesaj, users[usr].nume);
                      strcat(mesaj, " ");
                      strcat(mesaj, users[usr].prenume);

                    } else {
                      terminals[i].atempts++;
                      // la o greseala atempturile de pe un terminal se
                      // incrementeaza

                      if (terminals[i].atempts >= 3) {
                        // la 3 greseli se blocheaza cardul
                        users[usr].state = 3;
                        strcpy(mesaj, "IBANK> -5 : Card blocat");
                      } else {
                        strcpy(mesaj, "IBANK> -3 : Pin gresit");
                      }
                    }
                  }

                } else {
                  strcpy(mesaj, "IBANK> -4 : Numar card inexistent");
                }
              } else {
                // daca un transfer a fost deja initializat pe acest terminal
                // si se mai da odata comanda transfer se se considera alt
                // caracter(nu 'y') si se reseteaza daele de transfer
                memset(mesaj, 0, BUFLEN);
                strcpy(mesaj, "IBANK> -9 : Operatie anulata");
                users[usr].state = 1;
                users[usr].sum_to_trans = 0;
                users[usr].who_to_trans = 0;
              }

            } else if (strncmp(pch, "logout", 6) == 0) {

              usr = find_user_by_sock(users, no_clients, i);
              // daca de pe un terminal este un user logat se reseteaza datele
              // acestuia
              if (usr >= 0) {
                if (users[usr].state == 2) {
                  // daca un transfer a fost deja initializat pe acest terminal
                  // si se mai da odata comanda transfer se se considera alt
                  // caracter(nu 'y') si se reseteaza daele de transfer
                  memset(mesaj, 0, BUFLEN);
                  strcpy(mesaj, "IBANK> -9 : Operatie anulata");
                  users[usr].state = 1;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = 0;
                } else {
                  users[usr].state = 0;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = -1;
                  users[usr].sock = -1;
                  terminals[i].card = 0;
                  terminals[i].atempts = 0;
                  strcpy(mesaj, "IBANK> Clientul a fost deconectat");
                }
              } else {
                strcpy(mesaj, "IBANK> -1 : Clientul nu este autentificat");
              }

            } else if (strncmp(pch, "listsold", 8) == 0) {

              usr = find_user_by_sock(users, no_clients, i);
              if (usr >= 0) {
                strcpy(mesaj, "IBANK> : ");
                memset(strsold, 0, 20);
                // si la un transfer de numere intregi soldul din mesaj va avea
                // doua zecimale
                sprintf(strsold, "%.2f", users[usr].sold);
                strcat(mesaj, strsold);
                if (users[usr].state == 2) {
                  // daca un transfer a fost deja initializat pe acest terminal
                  // si se mai da odata comanda transfer se se considera alt
                  // caracter(nu 'y') si se reseteaza daele de transfer
                  memset(mesaj, 0, BUFLEN);
                  strcpy(mesaj, "IBANK> -9 : Operatie anulata");
                  users[usr].state = 1;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = 0;
                }
              } else {
                strcpy(mesaj, "IBANK> -1 : Clientul nu este autentificat");
              }

            } else if (strncmp(pch, "transfer", 8) == 0) {

              pch = strtok(NULL, " ");
              card = atoi(pch);
              pch = strtok(NULL, " ");
              double sum = atof(pch);
              usr = find_user_by_sock(users, no_clients, i);
              dest = find_user_by_card(users, no_clients, card);
              if (usr >= 0) {
                if (users[usr].state == 2) {
                  // daca un transfer a fost deja initializat pe acest terminal
                  // si se mai da odata comanda transfer se se considera alt
                  // caracter(nu 'y') si se reseteaza daele de transfer
                  strcpy(mesaj, "IBANK> -9 : Operatie anulata");
                  users[usr].state = 1;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = 0;
                } else if (dest >= 0) {
                  if (users[usr].sold >= sum) {
                    // daca ambii utilizatori ai transferului exista se seteaza
                    // starea de transfer suma transferata si destinatia
                    // acesteia(daca exista fonduri)
                    users[usr].sum_to_trans = sum;
                    users[usr].who_to_trans = dest;
                    users[usr].state = 2;

                    strcpy(mesaj, "IBANK> Transfer ");
                    memset(strsold, 0, 20);
                    sprintf(strsold, "%.2f", sum);
                    strcat(mesaj, strsold);
                    strcat(mesaj, " catre ");
                    strcat(mesaj, users[dest].nume);
                    strcat(mesaj, " ");
                    strcat(mesaj, users[dest].prenume);
                    strcat(mesaj, "? [y/n]");
                  } else {
                    strcpy(mesaj, "IBANK> -8 : Fonduri insuficiente");
                  }
                } else {
                  strcpy(mesaj, "IBANK> -4 : Numar card inexistent");
                }
              } else {
                strcpy(mesaj, "IBANK> -1 : Clientul nu este autentificat");
              }

            } else if (strncmp(pch, "quit", 4) == 0) {

              usr = find_user_by_sock(users, no_clients, i);

              if (usr >= 0) {
                if (users[usr].state != 2) {
                  // la quit se inchide conexiunea cu clientul doar daca cardul
                  // de pa care a venit "quit" nu este intr-o stare de transfer,
                  // in acest caz "quit" se interpreteaza cum ca nu are prima
                  // litara 'y' deci nu trece transferul
                  users[usr].state = 0;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = -1;
                  users[usr].sock = -1;
                  terminals[i].card = 0;
                  terminals[i].atempts = 0;

                  FD_CLR(i, &read_fds);

                } else {

                  memset(mesaj, 0, BUFLEN);
                  strcpy(mesaj, "IBANK> -9 : Operatie anulata");
                  users[usr].state = 1;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = 0;
                }
              }

            } else {
              // pentru orice altceva decat comenzile cheie se ignora ce a fost
              // primit sau daca cardul de pe care a venit mesajul este in curs
              // de transfer se verifica prima litera si se actioneaza
              // corespunzator
              usr = find_user_by_sock(users, no_clients, i);
              if (users[usr].state == 2) {
                if (pch[0] == 'y') {
                  dest = users[usr].who_to_trans;
                  users[usr].state = 1;
                  users[dest].sold += users[usr].sum_to_trans;
                  users[usr].sold -= users[usr].sum_to_trans;
                  users[usr].sum_to_trans = 0;
                  users[usr].who_to_trans = 0;

                  strcpy(mesaj, "IBANK> Transfer realizat cu succes");
                } else {
                  strcpy(mesaj, "IBANK> -9 : Operatie anulata");
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
