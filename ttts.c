#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include "protocol.h"
#include "arraylist.h"

#define QUEUE_SIZE 8
#define BUFSIZE 512
#define HOSTSIZE 100
#define PORTSIZE 10
#define NAMESIZE 32
#define MAX_PLAYERS 2
#define MAX_CLIENTS 100
#define DEBUG 0
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

/*
 * Tic-Tac-Toe Game Server Implementation
 * Authors: Sean M. Patrick & Fulton R. Wilcox
 */

typedef struct{ //struct to store data for each client. Each thread handles an array of 2 client structs for each game
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int socket_fd;
    int uid;
    char* name;
} client_t;

void handler(int signum);
void install_handlers(sigset_t *mask);
int open_listener(char *service, int queue_size);
int get_players(int listener, client_t *players);
int get_message(int socket_fd, char *buffer);
void send_message(int socket_fd, char *message);
void *play_game(void *player_info);
void print_board(char *board);
char* extract_name();
int isOccupied(char* name); //checks to see if name is being used
int game_result(char* board);
int is_valid_move(char* board, char* buffer, int *index, char *move);
int format_check(char* message);
int is_message_type(char* type, char* str);

volatile int active = 1;
int num_clients = 0, uid = 0; //total clients throughout the server, unique id for each client
pthread_mutex_t mutex; //mutex to lock specific variables
ttt_protocol protocol; //object with ttt protocol messages types
char buffer[BUFSIZE];
array_list client_names;

int main(int argc, char **argv){
    init(&protocol); //populate protocol object with message types
    init_list(&client_names, MAX_CLIENTS);
    sigset_t mask;
    pthread_t thread; //declare thread object
    char *service = argc == 2 ? argv[1] : "14247";
    install_handlers(&mask); //set up signal handling
    signal(SIGPIPE, SIG_IGN);
    int listener = open_listener(service, QUEUE_SIZE); //server settings
    if(listener < 0) exit(EXIT_FAILURE);
    pthread_mutex_init(&mutex, NULL); //initialize mutex
    puts("Welcome to the Tic-Tac-Toe Server!");
    puts("Listening for incoming connections");

    while(active){ //infinite loop
        client_t *players = (client_t *) calloc(MAX_PLAYERS, sizeof(client_t)); //allocate array of 2 client_t's
        if(num_clients <= MAX_CLIENTS){ //if total clients is at most 100 (50 concurrent games)
            if(get_players(listener, players) == -1) break; //populate client_t array with socket_fd's, uid's, and send appropriate message to each client
            int error = pthread_sigmask(SIG_BLOCK, &mask, NULL); //temporarily disable signals for worker threads
            if(error) { fprintf(stderr, "sigmask: %s\n", strerror(error)); exit(EXIT_FAILURE);}
            error = pthread_create(&thread, NULL, play_game, players); //create thread to run play_game() and pass array of client_t's as argument
            if(error) {fprintf(stderr, "Failed to create thread: error id %d\n", error); exit(EXIT_FAILURE);}
            pthread_detach(thread); //clean up child threads when they terminate
            error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL); //unblock handled signals
            if(error) { fprintf(stderr, "sigmask: %s\n", strerror(error)); exit(EXIT_FAILURE); }
        }
    }
    destroy(&client_names);
    fprintf(stderr, "Shutting down");
    close(listener); //close listenting socket
    pthread_mutex_destroy(&mutex); //destroy mutex object
    pthread_exit(NULL); //required to be last statement in main when threading in main
}

/*
 * Takes client socket file descriptor and pointer to character array buffer
 * Reads from client socket and prints message to output
 * Returns 1 when message received was formatted properly, 0 otherwise
 */
int get_message(int socket_fd, char *buffer){
    memset(buffer, '\0', BUFSIZE);
    int bytes = read(socket_fd, buffer, BUFSIZE);
    if(bytes < 0) {fprintf(stderr, "Error getting client message\n"); return 0;}
    fprintf(stderr, "Client %d message: %s\n", socket_fd - 3, buffer);
    return format_check(buffer);
}

/*
 * Takes client socket file descriptor and pointer to message string
 * Writes message string to client socket
 */
void send_message(int socket_fd, char *message){
    if(socket_fd == 0) fprintf(stderr, " socket fd is 0 ");
    int bytes = write(socket_fd, message, strlen(message));
    if(bytes < 0) {fprintf(stderr, "Error sending message to client\n"); return;}
    return;
}

/*
 * Takes socket file descriptor and pointer to array of client_t structs
 * Listens for incoming connections from clients until 2 clients are connected
 * If client types a name already in use, prompts client to input a new name until they choose a valid one
 * Populates array of client_t structs with appropriate client information
 * Returns 0 when 2 clients successfully connect, -1 when listening loop is interrupted via signals
 */
int get_players(int listener, client_t *players) {
    struct sockaddr_storage remote_host;
    socklen_t remote_host_len;
    int num_of_players = 0;
    remote_host_len = sizeof(remote_host);
    while(active && num_of_players < MAX_PLAYERS) { //while number of current client connections is less than 2
        int sock = accept(listener, (struct sockaddr *)&remote_host, &remote_host_len);
        if(sock < 0) { perror("accept"); continue; }
        if(!get_message(sock, buffer)) {send_message(sock, "INVL|19|Improper prococol.|"); continue;}
        pthread_mutex_lock(&mutex); //locking shared variables to avoid race conditions
        char* name = extract_name();
        while(isOccupied(name)) {
            send_message(sock, "INVL|14|Name Occupied|");
            free(name);
            if(!get_message(sock, buffer)) {send_message(sock, "INVL|19|Improper prococol.|"); continue;}
            name = extract_name();
        }
        num_clients++;
        players[num_of_players].socket_fd = sock;
        players[num_of_players].addr = remote_host;
        players[num_of_players].addr_len = remote_host_len;
        players[num_of_players].uid = uid++;
        players[num_of_players].name = name;
        push(&client_names, name); 

        char host[HOSTSIZE], port[PORTSIZE];
        int error = getnameinfo((struct sockaddr *)&players[num_of_players].addr, players[num_of_players].addr_len, host, HOSTSIZE, port, PORTSIZE, NI_NUMERICSERV);
        if(error) { strcpy(host, "??"); strcpy(port, "??"); }
        fprintf(stderr, "Connection from %s:%s\n", host, port);
        fprintf(stderr, "Number of client connections: %d\n", num_clients);
        
        pthread_mutex_unlock(&mutex);

        if(DEBUG) {
            puts("new player added");
            fprintf(stderr, "%d\n", players[num_of_players].uid);
        }
        if(num_of_players == 0) send_message(players[num_of_players].socket_fd, "WAIT|0|"); //if only 1 client joined, tell them to wait for second player
        num_of_players++;
    }
    if(!active) {
        free(players[0].name);
        free(players[1].name);
        free(players);
        return -1;
    }
    return 0;
}

/*
 * Parses name from server message containing a client player's name input and allocates new string representing that name
 * Returns pointer to name string
 */
char* extract_name() {
    char* name;
    int i;
    for(i=5; i<BUFSIZE; i++) {
        if(buffer[i] == '|') {
            for(int j = i+1; j < BUFSIZE; j++) {
                if(buffer[j] == '|') {
                    name = malloc(sizeof(char) * (j - i));
                    memcpy(name, &buffer[i+1], j - i - 1);
                    name[j - i - 1] = '\0';
                    break;
                }
            }
            break;
        }
    }
    return name;
}

/*
 * Signal handler
 * Sets volatile int active to 0
 */
void handler(int signum){
    active = 0;
}

/*
 * Initializes signal handlers
 */
void install_handlers(sigset_t *mask){
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    sigaddset(mask, SIGTERM);
}

/*
 * Takes pointer to char array representing port number, and integer representing list size for data structures
 * Attempts to create socket based on port number input
 * Binds socket to requested port number
 * Enables listening for incoming connection requests
 * Returns -1 on failure, and socket file descriptor when socket was successfully created
 */
int open_listener(char *service, int queue_size){
    int socket_fd, error;
    struct addrinfo hint, *info_list, *info;
    int option = 1;

    //initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    //get info for listening socket
    error = getaddrinfo(NULL, service, &hint, &info_list);
    if (error) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error)); return -1; }

    //attempt to create socket
    for(info = info_list; info != NULL; info = info->ai_next){
        socket_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

        //if could not create socket, try next method
        if(socket_fd == -1) { fprintf(stderr, "%s\n", strerror(errno)); continue;}

        if(setsockopt(socket_fd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0){
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }

        //bind socket to requested port
        error = bind(socket_fd, info->ai_addr, info->ai_addrlen);
        if(error) { fprintf(stderr, "%s\n", strerror(errno)); close(socket_fd); continue; }

        //enable listening for incoming connection requests
        error = listen(socket_fd, queue_size);
        if(error) { fprintf(stderr, "%s\n", strerror(errno)); close(socket_fd); continue; }

        //if we got this far, socket was opened
        break;
    }

    freeaddrinfo(info_list);

    //info will be null if no method succeeded
    if(info == NULL) { fprintf(stderr, "Could not bind\n"); return -1; }

    return socket_fd;
}

/*
 * Takes pointer to char array representing Tic-Tac-Toe board
 * Prints state of Tic-Tac-Toe board to output
 */
void print_board(char *board) {
    char buffer[BUFSIZE];
    memset(buffer, '\0', BUFSIZE);
    buffer[0] = '\n';
    int index = 1;
    for(int i=0;i<9;i++) {
        buffer[index] = ' '; index++;
        buffer[index] = '|'; index++;
        buffer[index] = board[i]; index++;
        buffer[index] = '|'; index++;
        fprintf(stderr, " |%c|", board[i]);
        if(i == 2 || i == 5) {
            fprintf(stderr, "\n -----------\n");
            buffer[index] = '\n'; index++;
            buffer[index] = ' '; index++;
            for(int j=0; j<11; j++) {
                buffer[index] = '-'; index++;
            }
            buffer[index] = '\n'; index++;
        }
    }
    fprintf(stderr, "\n");
}

/*
 * New threads created from main() start executing this function when 2 clients connect to the server
 * Takes pointer to specific data containing array of 2 player's client_t struct information
 * Handles all Tic-Tac-Toe game functionality and message protocol information
 * Utilizes mutex to coordinate shared data manipulation
 * Closes client sockets and frees appropriate data when game ends or when connection from client is interrupted
 */
void *play_game(void *player_info){
    char game_buf[BUFSIZE];
    memset(game_buf, '\0', BUFSIZE);
    char board[9] = {'.', '.', '.', '.', '.', '.', '.', '.', '.'};
    client_t *clients = (client_t *) player_info; //client_t array
    char c1_message[BUFSIZE] = "X|\0";
    char c2_message[BUFSIZE] = "O|\0";
    char c1_msg[BUFSIZE];
    char c2_msg[BUFSIZE];
    strcat(c1_message, clients[0].name);
    strcat(c2_message, clients[1].name);
    construct_message(&protocol, BEGN, strlen(c1_message)+1, c1_message, c1_msg);
    send_message(clients[0].socket_fd, c1_msg);
    construct_message(&protocol, BEGN, strlen(c2_message)+1, c2_message, c2_msg);
    send_message(clients[1].socket_fd, c2_msg);
    int counter = 0;
    int draw_suggestion = 0;
    while(active){
        int user;
        if(counter % 2 == 0) user = 0; else user = 1;
        if(!get_message(clients[user].socket_fd, game_buf)) {
            send_message(clients[user].socket_fd, "INVL|20|*Improper prococol.|");
            send_message(clients[(user + 1) % 2].socket_fd, "INVL|24|*Opponent disconnected.|");
            break;
        }
        int index = 0;
        char move[3];
        memset(move, 0, 3);
        int valid = is_valid_move(board, game_buf, &index, move);
        if(valid == 1) {
            char msg[BUFSIZE];
            char role = (user == 0) ? 'X' : 'O';
            board[index] = (user == 0) ? 'X' : 'O';
            char s[16] = {role, '|', move[0], ',', move[1], '|', board[0], board[1], board[2], board[3], board[4], board[5], board[6], board[7], board[8], '\0'};
            int result = game_result(board);
            if(result != 3) {
                if(result == 0) {
                    /*client[0] wins*/ 
                    char* winner = "W|You win!";
                    char* loser = "L|You lose.  Be better.";
                    construct_message(&protocol, OVER, strlen(winner)+1, winner, msg);
                    send_message(clients[0].socket_fd, msg);
                    construct_message(&protocol, OVER, strlen(loser)+1, loser, msg);
                    send_message(clients[1].socket_fd, msg);

                    construct_message(&protocol, MOVD, strlen(s)+1, s, msg);
                    send_message(clients[user].socket_fd, msg);
                    send_message(clients[(user + 1) % 2].socket_fd, msg);
                }
                else if(result == 1) {
                    /*client[1] wins*/                    
                    char* winner = "W|You win!";
                    char* loser = "L|You lose.  Be better.";
                    construct_message(&protocol, OVER, strlen(winner)+1, winner, msg);
                    send_message(clients[1].socket_fd, msg);
                    construct_message(&protocol, OVER, strlen(loser)+1, loser, msg);
                    send_message(clients[0].socket_fd, msg);

                    construct_message(&protocol, MOVD, strlen(s)+1, s, msg);
                    send_message(clients[user].socket_fd, msg);
                    send_message(clients[(user + 1) % 2].socket_fd, msg);
                }
                else {
                    /*draw*/
                    char* draw = "D|Draw.";
                    construct_message(&protocol, OVER, strlen(draw)+1, draw, msg);
                    send_message(clients[0].socket_fd, msg);
                    send_message(clients[1].socket_fd, msg);

                    construct_message(&protocol, MOVD, strlen(s)+1, s, msg);
                    send_message(clients[user].socket_fd, msg);
                    send_message(clients[(user + 1) % 2].socket_fd, msg);
                }
                break;
            }
            construct_message(&protocol, MOVD, strlen(s)+1, s, msg);
            send_message(clients[user].socket_fd, msg);
            send_message(clients[(user + 1) % 2].socket_fd, msg);
            counter++;
        }
        else if(valid == 5){ //user typed DRAW S
            char msg[BUFSIZE];
            construct_message(&protocol, DRAW, 2, "S", msg);
            send_message(clients[(user + 1) % 2].socket_fd, msg);
            draw_suggestion = 1;
            counter ++;
        }
        else if(valid == 6 && draw_suggestion == 1){ //user typed DRAW A
            char msg[BUFSIZE];
            char* draw = "D|Draw.";
            construct_message(&protocol, OVER, strlen(draw)+1, draw, msg);
            send_message(clients[user].socket_fd, msg);
            send_message(clients[(user + 1) % 2].socket_fd, msg);
            break;
        }
        else if(valid == 7 && draw_suggestion == 1){ //user typed DRAW R
            char msg[BUFSIZE];
            construct_message(&protocol, DRAW, 2, "R", msg);
            send_message(clients[(user + 1) % 2].socket_fd, msg);
            draw_suggestion = 0;
            counter ++;
        }
        else {
            char* errmsg;
            if(valid == 2) errmsg = "Space is occupied.";
            else if(valid == 3) errmsg = "Out of bounds.";
            else errmsg = "Incorrect format.";
            char msg[BUFSIZE];
            if(valid == 4){ //user typed RSGN
                char* winner = "W|Your opponent has resigned. You win!";
                char* loser = "L|You have resigned.";
                construct_message(&protocol, OVER, strlen(loser)+1, loser, msg);
                send_message(clients[user].socket_fd, msg);
                construct_message(&protocol, OVER, strlen(winner)+1, winner, msg);
                send_message(clients[(user + 1) % 2].socket_fd, msg);
                break;
            }
            construct_message(&protocol, INVL, strlen(errmsg)+1, errmsg, msg);
            send_message(clients[user].socket_fd, msg);
        }
    }
    for(int i = 0; i < MAX_PLAYERS; i ++){
        close(clients[i].socket_fd);
    } //close client sockets
    pthread_mutex_lock(&mutex); //lock shared variable
    num_clients -= 2;
    insert(&client_names, clients[0].uid, "\0"); 
    insert(&client_names, clients[1].uid, "\0");
    free(clients[0].name);
    free(clients[1].name);
    free(clients);
    fprintf(stderr, "Number of client connections: %d\n", num_clients);
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

/*
 * Takes pointer to char array representing game board, poiner to message buffer, and pointer to int representing index of board
 * Determines if a move is valid based on the state of the board and the move input from client and sets index of board when necessary
 * Returns 1 if move was valid, 2 if board position is already occupied, 
 * 3 if board position is out of bounds or if improperly formatted
 * 4 if client sent RSGN, 5 if client sent DRAW S, 6 if client sent DRAW A, and 7 if client sent DRAW R
 * 69 if improperly formatted
 */
int is_valid_move(char* board, char* buffer, int *index, char *move) {
    if(strlen(buffer) <= 8){
        if(buffer[0] == 'R' && buffer[1] == 'S' && buffer[2] == 'G' && buffer[3] == 'N'){
            return 4;
        }
    }
    if(strlen(buffer) <= 10){
        if(buffer[0] == 'D' && buffer[1] == 'R' && buffer[2] == 'A' && buffer[3] == 'W'){
            if(buffer[7] == 'S'){
                return 5;
            }
            else if(buffer[7] == 'A'){
                return 6;
            }
            else if(buffer[7] == 'R'){
                return 7;
            }
        }
    }
    int i;
    for(i = 7; i < strlen(buffer); i++) {
        if(buffer[i] == '|') {i++; break;}
    }
    if(buffer[i+1] != ',') return 69;
    move[0] = buffer[i]; move[1] = buffer[i+2]; move[2] = '\0';
    *index = 0;
    switch(move[0]) {
        case('1'): break;
        case('2'): {*index += 3; break;}
        case('3'): {*index += 6; break;}
        default: return 3;
    }
    switch(move[1]) {
        case('1'): break;
        case('2'): {*index += 1; break;}
        case('3'): {*index += 2; break;}
        default: return 3;
    }
    if(board[*index] != '.') return 2;
    return 1;
}

/*
 * Takes pointer to char array representing game board
 * Determines the current state of the game
 * Returns 0 when clients[0] wins, 1 when clients[1] wins, 2 when there is a draw, and 3 if the game has not ended
 */
int game_result(char* board) {
    if(board[0] == 'X' && board[1] == 'X' && board[2] == 'X') return 0;
    if(board[3] == 'X' && board[4] == 'X' && board[5] == 'X') return 0;
    if(board[6] == 'X' && board[7] == 'X' && board[8] == 'X') return 0;
    if(board[0] == 'X' && board[3] == 'X' && board[6] == 'X') return 0;
    if(board[1] == 'X' && board[4] == 'X' && board[7] == 'X') return 0;
    if(board[2] == 'X' && board[5] == 'X' && board[8] == 'X') return 0;
    if(board[0] == 'X' && board[4] == 'X' && board[8] == 'X') return 0;
    if(board[2] == 'X' && board[4] == 'X' && board[6] == 'X') return 0;

    if(board[0] == 'O' && board[1] == 'O' && board[2] == 'O') return 1;
    if(board[3] == 'O' && board[4] == 'O' && board[5] == 'O') return 1;
    if(board[6] == 'O' && board[7] == 'O' && board[8] == 'O') return 1;
    if(board[0] == 'O' && board[3] == 'O' && board[6] == 'O') return 1;
    if(board[1] == 'O' && board[4] == 'O' && board[7] == 'O') return 1;
    if(board[2] == 'O' && board[5] == 'O' && board[8] == 'O') return 1;
    if(board[0] == 'O' && board[4] == 'O' && board[8] == 'O') return 1;
    if(board[2] == 'O' && board[4] == 'O' && board[6] == 'O') return 1;

    for(int i = 0; i<9; i++) {
        if(board[i] == '.') return 3;
    }

    return 2;
}

/*
 * Takes pointer to string representing name that client entered
 * Checks name against list of client names within client name arraylist
 * Returns 1 if client name is already taken, 0 otherwise
 */
int isOccupied(char* name) {
    for(int i=0; i<get_length(&client_names); i++) {
        if(strcmp(client_names.data[i], name) == 0) return 1;
    }
    return 0;
}

/*
 * Takes pointer to char array representing protocol message type, and pointer to string to check against
 * Returns 1 if string contains the message type indicated by the first argument, 0 otherwise
 */
int is_message_type(char* type, char* str) {
    if(str != NULL){
        if(type[0] == str[0] && type[1] == str[1] && type[2] == str[2] && type[3] == str[3]) return 1;
    }
    return 0;
}

/*
 * Takes pointer to string containting message received from a client socket
 * Returns 1 if the message was formatted properly according to the protocol, 0 otherwise
 */
int format_check(char* message) {
    for(int i=0; i<8; i++) {
        if(!is_message_type(protocol.message_types[i], message)) { 
            if(i == 7) return 0;
        }
        break;
    }
    int size = 1;
    int index = 5;
    for(int i=5; i<strlen(message); i++) {
        if(message[i] != '|') size++;
        else {index = i+1; break;}
    }
    char* num = malloc(size);
    memcpy(num, &message[5], size);
    num[size-1] = '\0';
    if(strlen(message) - index != atoi(num)) {free(num); return 0;}
    free(num);
    return 1;
}
