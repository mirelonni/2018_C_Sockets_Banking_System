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

  fd_set read_fds; // multimea de citire folosita in select()
  fd_set tmp_fds;
  int fdmax;

  char buffer[BUFLEN];
  char mesaj[BUFLEN];
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

  fp = fopen(name, "w"); // fisierul in care scriu

  while (1) {

    tmp_fds = read_fds;

    if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
      error("ERROR in select");

    for (int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &tmp_fds)) {

        if (i == sockfd) {
          // pentru conexiunea TCP
          memset(buffer, 0, BUFLEN);
          n = recv(sockfd, buffer, BUFLEN, 0);
          if (n < 0) {
            error("ERROR writing to socket");
          }
          // if else-uri pentru actiunile pe care le fac cand primesc de la
          // server iferite mesaje
          if (strncmp(buffer, "quit", 4) == 0) {
            // daca serverul se inchide inchid si clientul
            close(sockfd);
            close(sockfd2);
            exit(0);
          } else if (strncmp(buffer, "IBANK> Welcome", 14) == 0) {
            printf("%s\n", buffer);
            fprintf(fp, "%s\n", buffer);
            is_logged = 1; // setez flagul de logged daca cineva s-a logat de pe
                           // acest client pentru a nu mai trimite la server
                           // mesajul de login daca il mai introduc odata ce
                           // sunt logat

          } else if (strncmp(buffer, "IBANK> Transfer", 15) == 0) {
            printf("%s\n", buffer);
            fprintf(fp, "%s\n", buffer);
            trans = 1; // daca mesajul primit incepe cu "Transfer" setez flagul
                       // de transfer pe 1 urmand sa il resetez ulterior daca
                       // s-a facut sau nu transferul
            if (strncmp(buffer, "IBANK> Transfer realizat", 24) == 0) {
              trans = 0; // resetarea flagului de transfer daca acesta a fost
                         // realizat
            }
          } else if (strncmp(buffer, "IBANK> -9", 9) == 0) {
            printf("%s\n", buffer);
            fprintf(fp, "%s\n", buffer);
            trans = 0; // resetarea flagului de transfer daca acesta nu a fost
                       // realizat
          } else {
            // caz normal unde doar afisef si scriu in fisier ce am primit
            printf("%s\n", buffer);
            fprintf(fp, "%s\n", buffer);
            fflush(stdout);
          }

        } else if (i == sockfd2) {
          // pentru conexiunea UDP
          memset(buffer, 0, BUFLEN);
          len = sizeof(serv_addr2);
          n = recvfrom(sockfd2, buffer, BUFLEN, 0,
                       (struct sockaddr *)&serv_addr2, &len);
          buffer[n] = '\0';

          if (strncmp(buffer, "UNLOCK> Trimite", 15) == 0) {
            secret = 1; // daca sunt in cursul unei deblocari(serverul vrea
                        // parola seceta de la mine) setez flagul de secret
          } else if (strncmp(buffer, "UNLOCK> Card", 12) == 0) {
            secret = 0; // resetez flagul de reset cand cardul a fost deblocat
          } else if (strncmp(buffer, "UNLOCK> -7", 10) == 0) {
            secret =
                0; // resetez flagul de reset cand flagul un a fost deblocat
          }

          printf("%s\n", buffer);
          fprintf(fp, "%s\n", buffer);

        } else if (i == 0) {

          memset(buffer, 0, BUFLEN);
          fgets(buffer, BUFLEN - 1, stdin);
          fprintf(fp, "%s", buffer);
          memcpy(mesaj, buffer, BUFLEN);

          if (strncmp(buffer, "quit", 4) == 0) {
            n = send(sockfd, mesaj, strlen(mesaj), 0);

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
              // daca deja este cineva logat pe acest client nu mai trimit nimic
              // decat daca este vorba de un transfer
              if (trans == 1) {
                n = send(sockfd, mesaj, strlen(mesaj), 0);
              } else {
                printf("-2 : Sesiune deja deschisa\n");
                fprintf(fp, "-2 : Sesiune deja deschisa\n");
              }
            } else {

              n = send(sockfd, mesaj, strlen(mesaj), 0);
            }

          } else if (strncmp(buffer, "logout", 6) == 0) {

            if (trans == 0) {
              is_logged = 0; // resetarea flagului de login
            }
            n = send(sockfd, mesaj, strlen(mesaj), 0);

          } else if (strncmp(buffer, "unlock", 6) == 0 && secret != 1) {
            // daca doar scriu unlock()serveru nu stie ca eu vreau sa deblochez
            // cardul
            sprintf(mesaj, "unlock %d", last_card);
            if (trans == 0) {
              sendto(sockfd2, mesaj, strlen(mesaj), 0,
                     (const struct sockaddr *)&serv_addr2, sizeof(serv_addr2));
            } else {
              // daca scriu unlock in raspunsul transferului
              n = send(sockfd, mesaj, strlen(mesaj), 0);
            }

          } else if (secret == 1) {
            // daca serverul imi cere parola secreta
            sprintf(mesaj, "%d %s", last_card, buffer);

            sendto(sockfd2, mesaj, strlen(mesaj), 0,
                   (const struct sockaddr *)&serv_addr2, sizeof(serv_addr2));

          } else {
            // pentru orice alt caz trimit mesajul normal
            n = send(sockfd, mesaj, strlen(mesaj), 0);
          }
        }
      }
    }
  }
  fclose(fp);
  return 0;
}
