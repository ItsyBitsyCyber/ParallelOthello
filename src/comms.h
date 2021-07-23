#ifndef _REFEREE_H
#define _REFEREE_H

#define SUCCESS 0
#define FAILURE -1

#define EMPTY 0
#define BLACK 1
#define WHITE 2

#define CMDBUFSIZE 100
#define MOVEBUFSIZE 6

int comms_init(int* my_colour); 
int comms_get_cmd(char cmd[], char move[]);
int comms_send_move(char my_move[]);

#endif
