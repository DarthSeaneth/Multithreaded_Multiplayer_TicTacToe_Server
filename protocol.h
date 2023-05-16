#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#define PLAY 0
#define WAIT 1
#define BEGN 2
#define MOVE 3
#define MOVD 4
#define INVL 5
#define RSGN 6
#define DRAW 7
#define OVER 8

typedef struct{
    char message_types[9][5];
} ttt_protocol;

void init(ttt_protocol *protocol);
char *get_message_type(ttt_protocol *protocol, int key);
char *construct_message(ttt_protocol *protocol, int key, int size, char* msg, char* message);
int contains_protocol(ttt_protocol *protocol, char* message);

#endif