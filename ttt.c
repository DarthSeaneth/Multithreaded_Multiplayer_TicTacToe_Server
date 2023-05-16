#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "protocol.h"

#define BUFSIZE 512
#define DEBUG 0

char buffer[BUFSIZE];
ttt_protocol protocol;
char* name;
char* message;
char role;
char opp_role;

/*
 * Tic-Tac-Toe Game Client Implementation
 * Authors: Sean M. Patrick & Fulton R. Wilcox
 */

int connect_to_server(char *host, char *service);
void get_message(int socket_fd, char *buffer);
void send_message(int socket_fd, char *message);
void play_game(int server_fd);
void get_name(char* buffer);
int message_contains(char* str);
int is_message_type(char* str);
void print_board(char *board);
void send_move(char* buffer, int server_fd);
void make_move(char *board, char *message);

int main(int argc, char **argv){
    fprintf(stderr, "Enter name: ");
    read(STDIN_FILENO, buffer, BUFSIZE);
    get_name(buffer);
    init(&protocol);

    int socket_fd;
    if (argc != 3) {
        printf("Specify host and service\n");
        exit(EXIT_FAILURE);
    }

    if(DEBUG) fprintf(stderr, "connecting to server\n");
    socket_fd = connect_to_server(argv[1], argv[2]);
    if(DEBUG) fprintf(stderr, "connected to server\n");
    if(socket_fd < 0) exit(EXIT_FAILURE);
    
    fprintf(stderr, "Welcome to Tic-Tac-Toe Online\n");
    char msg[BUFSIZE];
    construct_message(&protocol, PLAY, strlen(name)+1, name, msg);
    send_message(socket_fd, msg);
    if(DEBUG) fprintf(stderr, "sent message");

    do{
        free(message);
        get_message(socket_fd, buffer);
        if(strcmp(message, "INVL|14|Name Occupied|") == 0) {
            free(name); 
            fprintf(stderr, "Enter name: ");
            memset(buffer, 0, BUFSIZE);
            read(STDIN_FILENO, buffer, BUFSIZE);
            get_name(buffer);
            construct_message(&protocol, PLAY, strlen(name)+1, name, msg);
            send_message(socket_fd, msg);
        }
        else if( (strcmp(message, "INVL|19|Improper prococol.|") == 0) ) {
            free(name);
            return EXIT_SUCCESS;
        }
        else if(strcmp(message, "WAIT|0|") == 0) fprintf(stderr, "Waiting for second player\n");
    } while(!is_message_type("BEGN"));

    play_game(socket_fd);
    
    fprintf(stderr, "\nGame over\n");
    close(socket_fd);
    free(name);
    if(message!= NULL) free(message);
    return EXIT_SUCCESS;
}

/*
 * Takes pointer to message buffer
 * Parses name from buffer and allocates new string representing that name
 */
void get_name(char* buffer) {
    int i;
    for(i=0; i<BUFSIZE; i++) {
        if(buffer[i] == '\n') break;
    }
    name = malloc(sizeof(char) * (i+1));
    memcpy(name, buffer, i);
    name[i] = '\0';
}

/*
 * Takes server socket file descriptor and pointer to message buffer
 * Reads from socket and allocates a string representing the message from the server
 */
void get_message(int socket_fd, char *buffer){
    memset(buffer, 0, BUFSIZE);
    int bytes = read(socket_fd, buffer, BUFSIZE);
    message = malloc((sizeof(char) * bytes) + 1);
    memcpy(message, buffer, bytes);
    message[bytes] = '\0';
    if(bytes < 0) { fprintf(stderr, "Could not get server message\n"); return;}
    if(bytes > 0) { fprintf(stderr, "Server message: %s\n", message);}
    return;
}

/*
 * Takes server socket file descriptor and pointer to message that will be sent
 * Writes message string to server socket 
 */
void send_message(int socket_fd, char *message){
    if(DEBUG) fprintf(stderr, "sending message");
    int bytes = write(socket_fd, message, strlen(message));
    if(bytes < 0) fprintf(stderr, "Error sending message to server\n");
}

/*
 * Takes server socket file descriptor
 * Handles client Tic-Tac-Toe game functionality and protocol and message protocol information
 * Keeps track of the current turn of the player client
 * Prints state of game board when appropriate
 * Ends game when necessary
 */
void play_game(int server_fd) {
    char board[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int counter;
    int draw_suggestion = 0;
    if(message[7] == 'X') {fprintf(stderr, "You are X and are first.  Good luck!\n"); counter = 0; role = 'X'; opp_role = 'O';}
    else {fprintf(stderr, "You are O and are second.  Good luck!\n"); counter = 1; role = 'O'; opp_role = 'X';}
    while(!is_message_type("OVER")) {
        if(message != NULL){
            free(message);
            message = NULL;
        }
        if(draw_suggestion == 0){
            print_board(board);
        }
        if(counter % 2 == 0) {
            fprintf(stderr, "It is your turn.\n");
            read(STDIN_FILENO, buffer, BUFSIZE);
            send_move(buffer, server_fd);
            if(buffer[0] == 'D' && buffer[1] == 'R' && buffer[2] == 'A' && buffer[3] == 'W' && buffer[4] == ' '){
                if(buffer[5] == 'S'){
                    counter ++;
                    draw_suggestion = 1;
                    continue;
                }
                else if(buffer[5] == 'R' && draw_suggestion == 1){
                    counter ++;
                    draw_suggestion = 0;
                    continue;
                }
            }
            memset(&buffer, 0, BUFSIZE);
            get_message(server_fd, buffer);
            if(is_message_type("INVL")) {
                if(message[8] == '*') { 
                return;
                }
                else continue;
            }
            if(is_message_type("OVER")) { free(message); message = NULL; break;}
            else {
                counter++;
            }
        }
        else {
            get_message(server_fd, buffer);
            if(is_message_type("INVL")) {
                if(message[8] == '*') { 
                return;
                }
                else continue;
            }
            if(is_message_type("MOVD")) counter++;
            if(is_message_type("OVER")) { free(message); message = NULL; break;}
            if(is_message_type("DRAW")){
                if(message[7] == 'S'){
                    counter ++;
                    draw_suggestion = 1;
                    continue;
                }
                else if(message[7] == 'A'){
                    counter ++;
                    break;
                }
                else if(message[7] == 'R'){
                    counter ++;
                    draw_suggestion = 0;
                    continue;
                }
            }
        }
        if(buffer[7] != 0) make_move(board, buffer);
    }
    counter++;
    get_message(server_fd, buffer);
    if(buffer[0] == 'M' && buffer[1] == 'O' && buffer[2] == 'V' && buffer[3] == 'D'){
        make_move(board, buffer);
        print_board(board);
    }
    return;
}

/* 
 * Takes pointer to char array representing the game board, and pointer to string representing the formatted MOVD message
 * Parses move from message and interprets where on the board the player wants to make their move
 * Places player's role character in appropriate place on board
 */
void make_move(char* arr, char* msg) {
    int num_pipes = 0;
    int idx = 0;
    while(num_pipes < 4) {
        if (msg[idx] == '|') num_pipes++;
        idx++;
    }
    int board_idx = 0;
    for(int i=idx; i<strlen(msg)-1; i++) {
        if(msg[i] != '.') arr[board_idx] = msg[i];
        board_idx++;
    }
}

/*
 * Takes pointer to a string
 * Returns 0 if input string is not contained within the message buffer, 1 otherwise
 */
int message_contains(char* str) {
    int i;
    for(i = 5; i < strlen(message); i++) {
        if(message[i] == '|') {i++; break;}
    }
    int index = 0;
    for(int j = i; j<strlen(message)-1; j++) {
        if(index >= strlen(str)) return 0;
        if(str[index] != message[j]) return 0;
        index++;
    }
    return 1;
}

/*
 * Takes pointer to a string representing a protocol message type
 * Returns 1 if message buffer contains message type indicated by the arguments, 0 otherwise
 */
int is_message_type(char* str) {

    if(message != NULL){
        if(message[0] == str[0] && message[1] == str[1] && message[2] == str[2] && message[3] == str[3]) return 1;
    }
    return 0;
}

/*
 * Takes pointer to game board char array
 * Prints current state of the board
 */
void print_board(char *board) {
    fprintf(stderr, "   1   2   3");
    int row = 1;
    fprintf(stderr, "\n%d", row);
    row++;
    for(int i=0;i<9;i++) {
        fprintf(stderr, " |%c|", board[i]);
        if(i == 2 || i == 5) {
            fprintf(stderr, "\n  -----------\n");
            fprintf(stderr, "%d", row);
            row++;
        }
    }
    fprintf(stderr, "\n\n");
}

/*
 * Takes pointer to char arrays representing address of remote host (server) and port number
 * Attempts to create a socket based on remote host information
 * Connects socket to the server
 * Returns server socket file descriptor on success, -1 otherwise
 */
int connect_to_server(char *host, char *service){
    int socket_fd, error;
    struct addrinfo hints, *info_list, *info;

    //look up remote host
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 

    error = getaddrinfo(host, service, &hints, &info_list);
    if (error) { fprintf(stderr, "error looking up %s:%s: %s\n", host, service, gai_strerror(error)); return -1; }

    for(info = info_list; info != NULL; info = info->ai_next){
        socket_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (socket_fd < 0) continue;

        error = connect(socket_fd, info->ai_addr, info->ai_addrlen);

        if(error){ close(socket_fd); continue; }

        break;
    }
    freeaddrinfo(info_list);

    if (info == NULL) { fprintf(stderr, "Unable to connect to %s:%s\n", host, service); free(name); return -1; }
    return socket_fd;
}

/*
 * Takes server socket file descriptor and pointer to input buffer
 * Properly formats the move entered by the user and sends it to the server
 */
void send_move(char* buffer, int server_fd) {
    if(buffer[4] != 0 || buffer[4] == '\n'){
        if(buffer[0] == 'R' && buffer[1] == 'S' && buffer[2] == 'G' && buffer[3] == 'N'){
            send_message(server_fd, "RSGN|0|");
            return;
        }
    }
    if(buffer[6] != 0 || buffer[6] == '\n'){
        if(buffer[0] == 'D' && buffer[1] == 'R' && buffer[2] == 'A' && buffer[3] == 'W' && buffer[4] == ' '){
            char msg[BUFSIZE];
            if(buffer[5] == 'S'){
                construct_message(&protocol, DRAW, 2, "S", msg);
                send_message(server_fd, msg);
                return;
            }
            else if(buffer[5] == 'A'){
                construct_message(&protocol, DRAW, 2, "A", msg);
                send_message(server_fd, msg);
                return;
            }
            else if(buffer[5] == 'R'){
                construct_message(&protocol, DRAW, 2, "R", msg);
                send_message(server_fd, msg);
                return;
            }
        }
    }
    char move[3];
    move[0] = buffer[0]; move[1] = buffer[1]; move[2] = buffer[2];
    memset(buffer, 0, BUFSIZE);
    buffer[0] = 'M'; buffer[1] = 'O'; buffer[2] = 'V'; buffer[3] = 'E';
    buffer[4] = '|'; buffer[5] = '6'; buffer[6] = '|';
    buffer[7] = role; buffer[8] = '|';
    buffer[9] = move[0]; buffer[10] = move[1]; buffer[11] = move[2]; buffer[12] = '|';
    send_message(server_fd, buffer);
}