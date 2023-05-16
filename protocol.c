#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"

/*
 * Tic-Tac-Toe Protocol Information Library
 * Authors: Sean M. Patrick & Fulton R. Wilcox
 */

/*
 * Populates protocol message type string array with appropriate message types
 * Useful for checking valid messages and for string matching
 */
void init(ttt_protocol *protocol){
    strcpy(protocol->message_types[PLAY], "PLAY\0"); //PLAY name
    strcpy(protocol->message_types[WAIT], "WAIT\0"); //WAIT
    strcpy(protocol->message_types[BEGN], "BEGN\0"); //BEGN role name
    strcpy(protocol->message_types[MOVE], "MOVE\0"); //MOVE role position
    strcpy(protocol->message_types[MOVD], "MOVD\0"); //MOVD role position board
    strcpy(protocol->message_types[INVL], "INVL\0"); //INVL reason
    strcpy(protocol->message_types[RSGN], "RSGN\0"); //RSGN
    strcpy(protocol->message_types[DRAW], "DRAW\0"); //DRAW message
    strcpy(protocol->message_types[OVER], "OVER\0"); //OVER outcome reason
}

/*
 * Takes pointer to protocol object and integer representing specific message type
 * Returns message type string or NULL if key is invalid
 */
char *get_message_type(ttt_protocol *protocol, int key){
    if(key < 0 || key > 8) return NULL;
    return protocol->message_types[key];
}

/*
 * Takes pointer to protocol object, integer representing message type, integer representing size of the corresponding message
 * pointer to string containing message, and pointer to string to store entire message in
 * Formats the message based on the input information, populates message with that formatted message, and returns pointer to that message string
 */
char *construct_message(ttt_protocol *protocol, int key, int size, char* msg, char* message) {
    memset(message, '\0', 512);
    strcpy(message, get_message_type(protocol, key));
    char pipe = '|';
    char size_char[10];
    sprintf(size_char, "%d", size);
    strncat(message, &pipe, 1);
    strcat(message, size_char);
    strncat(message, &pipe , 1);
    if(msg) {   
        strcat(message, msg);
        strncat(message, &pipe , 1);
    }
    char end = '\0';
    strncat(message, &end, 1);
    //fprintf(stderr, " %s ", message);
    return message;
}

/*
 * Takes pointer to protocol object and string representing a message
 * Returns 1 if the input string contains a protocol message type, 0 if not
 */
int contains_protocol(ttt_protocol *protocol, char* message) {
    for(int i=0; i < 9; i++) {
        if(message[0] == protocol->message_types[i][0] && message[1] == protocol->message_types[i][1] &&
        message[2] == protocol->message_types[i][2] && message[3] == protocol->message_types[i][3]) return 1;
    }
    return 0;
}