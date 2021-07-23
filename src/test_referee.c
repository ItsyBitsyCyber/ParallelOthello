#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_opponent.h"
#include "comms.h"

const int GAME_OVER = 3;
static int status = BLACK;

int comms_init(int* my_colour) { 
    opponent_initialise();    
    *my_colour = BLACK;
    return SUCCESS;
}

int comms_get_cmd(char cmd[], char move[]) { 
    if (status == BLACK) {
        strncpy(cmd, "gen_move", CMDBUFSIZE);
    } else if (status == WHITE) {
        opponent_gen_move(move);
        if (strncmp(move, "pass\n", MOVEBUFSIZE) == 0) {
            strncpy(cmd, "game_over", CMDBUFSIZE);
            status = GAME_OVER;
        } else {
            strncpy(cmd, "play_move", CMDBUFSIZE);
            status = BLACK;
        }
    } else if (status == GAME_OVER) {
        strncpy(cmd, "game_over", CMDBUFSIZE);
    }
    return SUCCESS; 
}

int comms_send_move(char player_move[]) {
    if (strncmp(player_move, "pass", MOVEBUFSIZE) == 0) { 
        status = GAME_OVER;
    } else {
        opponent_apply_move(player_move);    
        status = WHITE;
    }
    return SUCCESS; 
}
