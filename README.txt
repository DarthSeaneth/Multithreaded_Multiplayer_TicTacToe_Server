Sean M. Patrick (smp429) & Fulton R. Wilcox (frw14)

CS214 Systems Programming Project III: Tic-Tac-Toe On-line

Implementation Description:
     Our implementation is a multi-threaded multiplayer Tic-Tac-Toe server to allow multiple two-player Tic-Tac-Toe games.  The maximum amount of clients
     that the server will accept at a time is 100, and therefore the maximum number of concurrent games is 50.
     Communication between the client and server is handled in designated read and write functions (send_message() and get_message()).
     The server is responsible for continuously adding players and creating a new thread to run the two-player game where the thread will execute play_game().
     Once a client launches with the arguments being the domain name and port nuber of the server, 
     they are prompted to enter their name.  Once the name is entered, connect_to_server() is called on the client side, which creates a connection 
     between the server and the client, or returns -1 upon failure.  On the server side, the server verifies that the name isn't occupied, or else the client is 
     prompted to enter a new name until they choose a name that is not occupied.  Important information is then extracted from the client and saved in a client struct, 
     and the client is sent a wait message if there aren't enough players, or a game is launched if they have an opponent player.  
     The server then launches a new thread and the play_game() function is started in the server and the clients.  
     Valid inputs for the clients are moves, draw, and resign.  Moves are formatted as a 3 
     character sequrence, where rows are denoted 1-3, and columns are denoted 1-3, each separated by a comma.  
     Messages not of this format that aren't draw suggestions or resignations are rejected.  
     In the case of a valid move, the server will send the formatted MOVD to both clients to indicate that a valid move has been made.  
     The server does not listen to a client if it is not their turn.  
     A client should not type a move if it is not their turn.  
     After each move, the server checks to see the status of the game, and ends the game if there has been a draw, resign, or if a player has won.  
     If the game is over, a message will be sent indicating the winner and both clients will be disconnected.
     Our implementation also utilizes the concept of mutex (mutual exclusion) to prevent race conditions occurring when threads are accessing shared data,
     which was done by using pthread_mutex_t data types from the pthread.h library.

Functions:
     ttts.c:
          void handler(int signum)
               -Signal handler
               -Sets volatile int active to 0

          void install_handlers(sigset_t *mask)
               -Initializes signal handlers

          int open_listener(char *service, int queue_size)
               -Takes pointer to char array representing port number, and integer representing list size for data structures
               -Attempts to create socket based on port number input
               -Binds socket to requested port number
               -Enables listening for incoming connection requests
               -Returns -1 on failure, and socket file descriptor when socket was successfully created

          int get_players(int listener, client_t *players)
               -Takes socket file descriptor and pointer to array of client_t structs
               -Listens for incoming connections from clients until 2 clients are connected
               -If client types a name already in use, prompts client to input a new name until they choose a valid one
               -Populates array of client_t structs with appropriate client information
               -Returns 0 when 2 clients successfully connect, -1 when listening loop is interrupted via signals

          int get_message(int socket_fd, char *buffer)
               -Takes client socket file descriptor and pointer to character array buffer
               -Reads from client socket and prints message to output
               -Returns 1 when message received was formatted properly, 0 otherwise

          void send_message(int socket_fd, char *message)
               -Takes client socket file descriptor and pointer to message string
               -Writes message string to client socket

          void *play_game(void *player_info)
               -New threads created from main() start executing this function when 2 clients connect to the server
               -Takes pointer to specific data containing array of 2 player's client_t struct information
               -Handles all Tic-Tac-Toe game functionality and message protocol information
               -Utilizes mutex to coordinate shared data manipulation
               -Closes client sockets and frees appropriate data when game ends or when connection from client is interrupted

          void print_board(char *board)
               -Takes pointer to char array representing Tic-Tac-Toe board
               -Prints state of Tic-Tac-Toe board to output

          char* extract_name()
               -Parses name from server message containing a client player's name input and allocates new string representing that name
               -Returns pointer to name string

          int isOccupied(char* name)
               -Takes pointer to string representing name that client entered
               -Checks name against list of client names within client name arraylist
               -Returns 1 if client name is already taken, 0 otherwise

          int game_result(char* board)
               -Takes pointer to char array representing game board
               -Determines the current state of the game
               -Returns 0 when clients[0] wins, 1 when clients[1] wins, 2 when there is a draw, and 3 if the game has not ended

          int is_valid_move(char* board, char* buffer, int *index)  
               -Takes pointer to char array representing game board, poiner to message buffer, and pointer to int representing index of board
               -Determines if a move is valid based on the state of the board and the move input from client and sets index of board when necessary
               -Returns 1 if move was valid, 2 if board position is already occupied, 
               -3 if board position is out of bounds or if the input was not formatted correctly.
               -4 if client sent RSGN, 5 if client sent DRAW S, 6 if client sent DRAW A, and 7 if client sent DRAW R

          int format_check(char* message)
               -Takes pointer to string containting message received from a client socket
               -Returns 1 if the message was formatted properly according to the protocol, 0 otherwise

          int is_message_type(char* type, char* str)
               -Takes pointer to char array representing protocol message type, and pointer to string to check against
               -Returns 1 if string contains the message type indicated by the first argument, 0 otherwise

     ttt.c:
          int connect_to_server(char *host, char *service)
               -Takes pointer to char arrays representing address of remote host (server) and port number
               -Attempts to create a socket based on remote host information
               -Connects socket to the server
               -Returns server socket file descriptor on success, -1 otherwise

          void get_message(int socket_fd, char *buffer)
               -Takes server socket file descriptor and pointer to message buffer
               -Reads from socket and allocates a string representing the message from the server

          void send_message(int socket_fd, char *message)
               -Takes server socket file descriptor and pointer to message that will be sent
               -Writes message string to server socket 

          void play_game(int server_fd)
               -Takes server socket file descriptor
               -Handles client Tic-Tac-Toe game functionality and protocol and message protocol information
               -Keeps track of the current turn of the player client
               -Prints state of game board when appropriate
               -Ends game when necessary

          void get_name(char* buffer)
               -Takes pointer to message buffer
               -Parses name from buffer and allocates new string representing that name

          int message_contains(char* str)
               -Takes pointer to a string
               -Returns 0 if input string is not contained within the message buffer, 1 otherwise

          int is_message_type(char* str)
               -Takes pointer to a string representing a protocol message type
               -Returns 1 if message buffer contains message type indicated by the arguments, 0 otherwise

          void print_board(char *board)
               -Takes pointer to game board char array
               -Prints current state of the board

          void send_move(char* buffer, int server_fd)
               -Takes server socket file descriptor and pointer to input buffer
               -Properly formats the move entered by the user and sends it to the server

          void make_move(char* arr, char* msg)
               -Takes pointer to char array representing the game board, and pointer to string representing the formatted MOVD message
               -Parses move from message and interprets where on the board the player wants to make their move
               -Places player's role character in appropriate place on board

     protocol.c:
          void init(ttt_protocol *protocol)
               -Populates protocol message type string array with appropriate message types
               -Useful for checking valid messages and for string matching

          char *get_message_type(ttt_protocol *protocol, int key)
               -Takes pointer to protocol object and integer representing specific message type
               -Returns message type string or NULL if key is invalid

          char *construct_message(ttt_protocol *protocol, int key, int size, char* msg, char* message)
               -Takes pointer to protocol object, integer representing message type, integer representing size of the corresponding message,
               -pointer to string containing message, and pointer to string to store entire message in
               -Formats the message based on the input information, populates message with that formatted message, and returns pointer to that message string

          int contains_protocol(ttt_protocol *protocol, char* message)
               -Takes pointer to protocol object and string representing a message
               -Returns 1 if the input string contains a protocol message type, 0 if not

Test Plan:
     Our test plan had many components and we utilized the plan as we implemented and tested each aspect of our server and client.
     After implementing each aspect of the server and client we would thoroughly test different scenarios and conditions with respect to the part that we implemented.
     First we implemented the basic server/client socket connection functionality, which was vital for this project and it allowed us to properly set up the server
     socket settings and connection, as well as the client socket connection and set up.
     Then we implemented the main, multi-threaded client connection listening loop, which was extremely important to set up in order to allow our implementation
     to handle multiple concurrent, 2-player games while also continuing to listen for client connections. 
     This allowed us to perfect our implementation of multi-threading to handle concurrent games, and revealed many issues that led us to be able to properly set up
     the multi-threaded server and to clean up the threads and properly handle all thread data.
     Then finally we implemented the tic-tac-toe game functionality and all message protocol functionality, which was the largest part of the process.
     There were many steps to this process, and along the way we encountered many errors and mistakes which allowed us to perfect our implementation to properly
     send appropriately formatted messages between our server and clients, properly handle all game functionality, and properly handle all client connections
     and respond to all possible scenarios appropriately.
     After we perfected our client implementation, it was then vital to start handling malformatted messages potentially being sent from client programs as our
     client program at the time was formatting all messages properly and only sending one message at a time.
     To test this, we then modified our client program extensively in order for it to send improperly formatted messages and even multiple messages at once.
     This allowed us to see all of the errors and mistakes that we made in our server implementation, which then allowed us to account for these important cases.
     We then perfected our server implementation by handling improperly formatted client messages appropriately, and also in the case of a client sending multiple
     messages at once, our server will only detect the first full message sent and ignore any subsequent messages that were sent at the same exact time.
     All in all, this process allowed us to perfect our server implementation to work perfectly with our functional client program, 
     as well as to work with other, potentially problematic client programs.

***IMPORTANT NOTES:
     -Our server implementation expects a client program to prompt a user to enter their name (maximum 32 characters), and then send the server the properly
     formatted PLAY message as such: PLAY|# of remaining bytes in message|"Player name"|  Example: PLAY|5|Sean|
     Our server keeps track of the name that each client entered by storing them in an arraylist
     Each client must have a unique name, and so if a client choose a name that is already taken, it will send an appropriate INVL message to the client and then
     it will wait for them to input another name.
     Therefore, a client must prompt the user to enter a name repeatedly until the client does not receive the INVL message, if the client has chosen a name that was 
     already chosen by another client.
     -A client should not enter anything into standard input if it is not their turn
     -Valid client inputs when it is their turn: moves of the format x,x where x is an integer between 1 and 3
     DRAW S, DRAW R, DRAW A, RSGN -> the server will send a specific INVL message if inputs from the client are not of this format
     -If a client program sends a malformatted message or any invalid message protocol type, the server will send an INVL message to the client, and the server
     will also respond by sending a message to the other client and properly handling the client connections whenever necessary
     -We arbitrarily chose a limit of 100 client connections for our server, and so our server can handle 50 concurrent 2-player games
