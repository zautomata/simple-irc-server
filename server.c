#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "include/user.h"

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

Node *user_list;
pthread_mutex_t user_list_mutex;

void connect_user(User *);
char *get_command(char[MAXLINE + 1], ssize_t);
void receive_messages(User *);

int main(int argc, char **argv) {
    int server_socket, user_socket;
    struct sockaddr_in server_address;

    user_list = empty_user_list();
    pthread_mutex_init(&user_list_mutex, NULL);

    if(argc != 2) {
        fprintf(stderr, "Uso: %s [porta]\n", argv[0]);
        exit(1);
    };

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "[Erro chamando socket: %s\n", strerror(errno));
        exit(2);
    };

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port        = htons(atoi(argv[1]));
    if(bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        fprintf(stderr, "[Erro chamando bind: %s]\n", strerror(errno));
        exit(3);
    };

    if(listen(server_socket, LISTENQ) == -1) {
        fprintf(stderr, "[Erro chamando listen: %s]\n", strerror(errno));
        exit(4);
    };

    printf("[Servidor Inicializado na porta %s.]\n", argv[1]);
    printf("[Pressione CTRL+C para sair.]\n");

    User *new_user;
    for(;;) {
        if ((user_socket = accept(server_socket, (struct sockaddr *) NULL, NULL)) == -1 ) {
            fprintf(stderr, "[Erro chamando accept: %s]\n", strerror(errno));
            exit(5);
        };
        pthread_mutex_lock(&user_list_mutex);
        user_list = add_user(user_list, "new_user", length(user_list), 0, user_socket);
        pthread_mutex_unlock(&user_list_mutex);
        new_user = user_list->payload;

        pthread_create(&(new_user->thread), NULL, (void * (*) (void *)) connect_user, new_user);
    };
};

char *get_command(char line[MAXLINE + 1], ssize_t n) {
    char *command = malloc(n);
    command       = strcpy(command, line);
    return strtok(command, " \t\r\n");
};

void connect_user(User *user) {
    printf("[Usuario %d conectou-se ao servidor, esperando mensagens]\n", user->id);
    char recvline[MAXLINE + 1];
    ssize_t n;

    pthread_mutex_lock(&user->socket_mutex);
    n = read(user->socket, recvline, MAXLINE);
    pthread_mutex_unlock(&user->socket_mutex);

    while (n > 0) {
        recvline[n] = 0;
        printf("[Usuario %d enviou:] ", user->id);
        if ((fputs(recvline,stdout)) == EOF) {
            fprintf(stderr, "Erro chamando fputs: %s\n", strerror(errno));
            exit(6);
        };
        printf("get_command(recvline, n) == %s\n", get_command(recvline, n));
        if(strcmp(get_command(recvline, n), "CONNECT") == 0) {
            break;
        };
        pthread_mutex_lock(&user->socket_mutex);
        n = read(user->socket, recvline, MAXLINE);
        pthread_mutex_unlock(&user->socket_mutex);
    };
    receive_messages(user);
};

void receive_messages(User *user) {
    printf("[Usuario %d entrou no canal principal]\n", user->id);
    char recvline[MAXLINE + 1];
    ssize_t n;

    char *welcome = "[Voce entrou no canal principal]\n";
    char *prompt  = "[IRCserver Principal] <Usuario>: ";
    pthread_mutex_lock(&user->socket_mutex);
    write(user->socket, welcome, strlen(welcome));
    write(user->socket, prompt, strlen(prompt));
    pthread_mutex_unlock(&user->socket_mutex);

    pthread_mutex_lock(&user->socket_mutex);
    n = read(user->socket, recvline, MAXLINE);
    pthread_mutex_unlock(&user->socket_mutex);

    while (n > 0) {
        recvline[n] = 0;
        printf("[Usuario %d enviou:] ", user->id);
        if ((fputs(recvline,stdout)) == EOF) {
            fprintf(stderr, "Erro chamando fputs: %s\n", strerror(errno));
            exit(6);
        };
        pthread_mutex_lock(&user->socket_mutex);
        write(user->socket, prompt, strlen(prompt));
        pthread_mutex_unlock(&user->socket_mutex);
        /*
         * Escrever em todos os logados no canal.
         */
        pthread_mutex_lock(&user->socket_mutex);
        n = read(user->socket, recvline, MAXLINE);
        pthread_mutex_unlock(&user->socket_mutex);
    };
};