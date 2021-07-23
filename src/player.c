/* vim: ai:sw=4:ts=4:sts:et */

/*H**********************************************************************
 *
 *    This is a skeleton to guide development of Othello engines that can be used
 *    with the Ingenious Framework and a Tournament Engine. 
 *    The communication with the referee is handled by an implementaiton of comms.h,
 *    All communication is performed at rank 0.
 *
 *    Board co-ordinates for moves start at the top left corner of the board i.e.
 *    if your engine wishes to place a piece at the top left corner, the "gen_move"
 *    function must return "00".
 *
 *    The match is played by making alternating calls to each engine's "gen_move"
 *    and "play_move" functions. The progression of a match is as follows:
 *        1. Call gen_move for black player
 *        2. Call play_move for white player, providing the black player's move
 *        3. Call gen move for white player
 *        4. Call play_move for black player, providing the white player's move
 *        .
 *        .
 *        .
 *        N. A player makes the final move and "game_over" is called for both players
 *    
 *    IMPORTANT NOTE:
 *        Any output that you would like to see (for debugging purposes) needs
 *        to be written to file. This can be done using file fp, and fprintf(),
 *        don't forget to flush the stream. 
 *        I would suggest writing a method to make this
 *        easier, I'll leave that to you.
 *        The file name is passed as argv[4], feel free to change to whatever suits you.
 *H***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <assert.h>
#include "comms.h"

/* minimax algo */
#define MAX_DEPTH 5
#define ALPHA -1000
#define BETA 1000
#define MAX_INT 1000

const int OUTER = 3;
const int ALLDIRECTIONS[8] = {-11, -10, -9, -1, 1, 9, 10, 11};
const int BOARDSIZE = 100;
const int LEGALMOVSBUFSIZE = 65;
const char piecenames[4] = {'.', 'b', 'w', '?'};
const int SHARE = 1;

int *gen_move(char *move);
void play_move(char *move);
void game_over();
void run_worker();
void initialise_board();
void free_board();

int *legalmoves(int player);
int legalp(int move, int player);
int validp(int move);
int wouldflip(int move, int dir, int player);
int opponent(int player);
int findbracketingpiece(int square, int dir, int player);
int randomstrategy();
void makemove(int move, int player);
void makeflips(int move, int dir, int player);
int get_loc(char *movestring);
void get_move_string(int loc, char *ms);
void printboard();
char nameof(int piece);
int count(int player, int *board);
void divide_moves(int *global_moves, int *local_moves);
void sort_moves(int player);
int iterative_minimax(int *board, int current_depth, int max_depth, int player, int alpha, int beta);
int minimax(int *board, int current_depth, int max_depth, int player, int alpha, int beta);
int evaluate_board(int *board, int player);
int *copy_board(int *board);
void alpha_beta_sharing(int alpha, int beta);
void print_process_moves(int *local_moves, int *send_counts); /* DEBUG */

int my_colour;
int time_limit;
int running;
int rank;
int size;
int *board;
FILE *fp;
int *moves;
int *local_moves;
int *send_counts, *displacements; /* dividing moves */
/* weights for evaluation funciton */
int weights[100] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 20, 0, 10, 10, 10, 10, 0, 20, 0,
                    0, 0, 0, 5, 5, 5, 5, 0, 0, 0,
                    0, 10, 5, 3, 1, 1, 3, 5, 10, 0,
                    0, 10, 5, 1, 7, 7, 1, 5, 10, 0,
                    0, 10, 5, 1, 7, 7, 1, 5, 10, 0,
                    0, 10, 5, 3, 1, 1, 3, 5, 10, 0,
                    0, 0, 0, 5, 5, 5, 5, 0, 0, 0,
                    0, 20, 0, 10, 10, 10, 10, 0, 20, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* parallelise process 0 and other processes that do not enter the main while loop */
int flag = 1;
const int COMPUTE = 1, STOP = 2;

int main(int argc, char *argv[])
{
    char cmd[CMDBUFSIZE];
    char opponent_move[MOVEBUFSIZE];
    char my_move[MOVEBUFSIZE];

    double start, end; /* timing */

    /* starts MPI */
    MPI_Init(&argc, &argv);
    start = MPI_Wtime();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* get current process id */
    MPI_Comm_size(MPI_COMM_WORLD, &size); /* get number of processes */
    my_colour = EMPTY;
    initialise_board();
    MPI_Status status;

    /* array of valid moves */
    moves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
    memset(moves, 0, LEGALMOVSBUFSIZE);
    local_moves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
    memset(local_moves, 0, LEGALMOVSBUFSIZE);

    /* arrays for Scatterv function */
    send_counts = (int *)malloc(size * sizeof(int));
    memset(send_counts, 0, size);
    displacements = (int *)malloc(size * sizeof(int));
    memset(displacements, 0, size);

    /* Rank 0 is responsible for handling communication with the server */
    if (rank == 0 && argc == 3)
    {
        time_limit = atoi(argv[1]);
        fp = fopen(argv[2], "w");
        fprintf(fp, "This is an example of output written to file.\n");
        fflush(fp);

        if (comms_init(&my_colour) == FAILURE)
            return FAILURE;
        running = 1;

        MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
        while (running == 1)
        {
            if (comms_get_cmd(cmd, opponent_move) == FAILURE)
            {
                fprintf(fp, "Error getting cmd\n");
                fflush(fp);
                running = 0;
                break;
            }

            if (strcmp(cmd, "game_over") == 0)
            {
                running = 0;
                fprintf(fp, "Game over\n");
                fflush(fp);
                break;

                /* Rank 0 calls gen_move */
            }
            else if (strcmp(cmd, "gen_move") == 0)
            {
                MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
                MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);
                memset(my_move, 0, MOVEBUFSIZE);
                strncpy(my_move, "pass\n", MOVEBUFSIZE);
                int best_move = 0;
                int temp_move = 0;
                int temp_score = 0;
                int score = -1000;

                /* send current board state to processes */
                for (int i = 1; i < size; i++)
                {
                    MPI_Send(board, BOARDSIZE, MPI_INT, i, COMPUTE, MPI_COMM_WORLD);
                }
                /* get legal moves & calculate displacements and moves per process */
                legalmoves(my_colour);
                divide_moves(moves, local_moves);

#ifdef DEBUG
                print_process_moves(local_moves, send_counts);
#endif
                /* get the best move from each process and compare */
                for (int i = 1; i < size; i++)
                {
                    MPI_Recv(&best_move, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&temp_score, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (temp_score > score)
                    {
                        temp_move = best_move;
                        score = temp_score;
                    }
                }
                get_move_string(best_move, my_move);
#ifdef DEBUG
                printf("Chosen move: %s\n", my_move);
#endif
                makemove(best_move, my_colour);

                if (comms_send_move(my_move) == FAILURE)
                {
                    running = 0;
                    fprintf(fp, "Move send failed\n");
                    fflush(fp);
                    break;
                }
                printboard();   
            }
            else if (strcmp(cmd, "play_move") == 0)
            {
                /* Add the opponent's move to my board */
                play_move(opponent_move);
                printboard();
            }
        }
        /* send message to tell other processes to stop */
        int over = 0;
        for (int i = 1; i < size; i++)
            MPI_Send(&over, 1, MPI_INT, i, STOP, MPI_COMM_WORLD);
    }
    else
    {
        /* this function will return a move which will be sent to process 0 */
        run_worker(rank);
    }

    game_over();
    end = MPI_Wtime();
    if (rank == 0)
    { /* use time on master proc  */
#ifdef DEBUG
        printf("B: %d | W:%d \n", count(BLACK, board), count(WHITE, board));
        printf("Runtime = %f\n", end - start);
#endif
    }
}

/*
    Called at the start of execution on all ranks
 */
void initialise_board()
{
    int i;
    running = 1;
    board = (int *)malloc(BOARDSIZE * sizeof(int));
    for (i = 0; i <= 9; i++)
        board[i] = OUTER;
    for (i = 10; i <= 89; i++)
    {
        if (i % 10 >= 1 && i % 10 <= 8)
            board[i] = EMPTY;
        else
            board[i] = OUTER;
    }
    for (i = 90; i <= 99; i++)
        board[i] = OUTER;
    board[44] = WHITE;
    board[45] = BLACK;
    board[54] = BLACK;
    board[55] = WHITE;
}

void free_board()
{
    free(board);
}

/**
 * Function used for running worker functions during execution of program. 
 *  
 * @param rank
 * 
 * Rank i (i != 0) executes this code 
 * ----------------------------------
 *  Called at the start of execution on all ranks except for rank 0.
 *   - all workers execute the iterative version of the minimax algorithm 
 *   - all workers return their best move and score to rank 0 
 */
void run_worker(int rank)
{
    int *temp_board, *legal_moves, my_score, current_move;
    int temp_score = -1000;
    int temp_move = -1;
    int depth = 0;
    int best_move = 0; 
    
    MPI_Status status;
    initialise_board();

    char my_move[MOVEBUFSIZE];
    memset(my_move, 0, MOVEBUFSIZE);
    
    int *process_counts, *process_displacements;
    process_counts = (int *)malloc((size + 1) * sizeof(int));
    process_displacements = (int *)malloc((size + 1) * sizeof(int));
    
    if (my_colour == EMPTY)
    {
        my_colour = BLACK;
    }
    while (flag == 1)
    {
        temp_score = -10000;
        MPI_Recv(board, BOARDSIZE, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        legal_moves = legalmoves(my_colour);

        /* determine how many moves each process gets */
        for (int i = 1; i < size; i++)
        {
            process_counts[i] = moves[0] / (size - 1);
        }

        /* divide remainder between processes */
        int remainder = moves[0] % (size - 1);
        int curr_worker = 0;
        while (remainder >= 0)
        {
            process_counts[curr_worker]++;
            curr_worker = (curr_worker + 1) % (size - 1);
            remainder--;
        }

        /* determine displacements */
        int sum = 0;
        process_displacements[1] = 0;
        for (int i = 1; i < size; i++)
        {
            process_displacements[i] = sum;
            sum += process_counts[i];
        }

#ifdef DEBUG
        if (rank == 1)
        {
            printf("Total moves:");
            for (int i = 0; i < legal_moves[0]; i++)
            {
                printf("%d ", legal_moves[i]);
            }
            printf("\n");
        }
#endif
        int i = process_displacements[rank];
        if (rank == 1)
            i = i + 1;
        for (int j = i; j < process_displacements[rank] + process_counts[rank]; j++)
        {
            current_move = legal_moves[j];
            temp_board = copy_board(board);
            my_score = iterative_minimax(board, depth, MAX_DEPTH, my_colour, ALPHA, BETA);
            if (my_score > temp_score)
            {
                temp_score = my_score;
                temp_move = current_move;
            }

            board = copy_board(temp_board);
            free(temp_board);
        }
        if (temp_move > -1)
        {
            get_move_string(temp_move, my_move);
        }
        else
        {
            strncpy(my_move, "pass\n", MOVEBUFSIZE);
        }
        best_move = temp_move;
        if (status.MPI_TAG == COMPUTE)
        {

            /* process will send best move back to master */
            MPI_Send(&best_move, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Send(&temp_score, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

        else
            flag = 0;
    }
}
/**
 * Function to iteratively run the Minimax algorithm
 * The function runs the algorithm with increasing 
 * depth until it reaches the maximum depth.
 * 
 * @param board 
 * @param current_depth 
 * @param max_depth 
 * @param player 
 * @param alpha
 * @param beta
 *
 * @return best_move
 *  */
int iterative_minimax(int *board, int current_depth, int max_depth, int player, int alpha, int beta)
{
    int best_score = -ALPHA;

    for (int depth = 0; depth < MAX_DEPTH; depth++)
    {
        best_score = minimax(board, current_depth, depth, player, alpha, beta);
    }
    return best_score;
}

/*
   Rank 0 executes this code: 
   --------------------------
   Can be used to generate a valid random move. 
 */
int *gen_move(char *move)
{
    int loc = 0;
    if (my_colour == EMPTY)
    {
        my_colour = BLACK;
    }
    /* P0 get all legal moves */
    legalmoves(my_colour);

    if (moves[0] == 0)
    {
        /* no more legal moves */
        strncpy(move, "pass\n", MOVEBUFSIZE);
        return moves;
    }
    else
    {
        /* return array of moves */
        loc = moves[(rand() % moves[0]) + 1];
        get_move_string(loc, move);
        makemove(loc, my_colour);
        return moves;
    }
}
/**
    Function to divide up array of available moves among processes. 
    
    @param global_moves
    @param process_moves 
*/
void divide_moves(int *global_moves, int *process_moves)
{
    /* divide available moves among processes */
    /* determine how many moves each process gets */
    for (int i = 1; i < size; i++)
    {
        send_counts[i] = moves[0] / (size - 1);
    }

    /* divide remainder between processes */
    int remainder = moves[0] % (size - 1);
    int curr_worker = 0;
    while (remainder >= 0)
    {
        send_counts[curr_worker]++;
        curr_worker = (curr_worker + 1) % (size - 1);
        remainder--;
    }
    /* determine displacements */
    int sum = 0;
    displacements[1] = 0;
    for (int i = 1; i < size; i++)
    {
        displacements[i] = sum;
        sum += send_counts[i];
    }
}
/**   
 *  Function to sort the list of legal moves
 *  prior to executing the minimax.
 *  
 *  @param  player
 *   
 */
void sort_moves(int player)
{
    int *ratings = malloc(32 * sizeof(int));
    int n;
    moves = legalmoves(player);
    int num_moves = moves[0];
    for (int i = 1; i <= num_moves; i++)
    {
        n = moves[i];
        ratings[i] = weights[n];
    }

    for (int i = 0; i < num_moves; ++i)
    {
        for (int j = i + 1; j < num_moves; ++j)
        {
            if (ratings[i] < ratings[j])
            {
                int temp = moves[i];
                moves[i] = moves[j];
                moves[j] = temp;
            }
        }
    }
}
/**  
    Function that copies the original board 
    to a tempory board. 
    Used to store previous board states. 
    
    @params: board to be copied    

    @return copied board
 */
int *copy_board(int *board)
{
    int *temp_board = (int *)malloc(BOARDSIZE * sizeof(int));

    for (int i = 0; i < 99; i++)
    {
        temp_board[i] = board[i];
    }

    return temp_board;
}

/**
    Function to recursively perform MiniMax algorithm given a specific
    move and bored state.  
    
    @param: board, current_depth, max_depth, player, alpha, beta
*/
int minimax(int *board, int current_depth, int max_depth, int player, int alpha, int beta)
{ 
    int *temp_board, *legal_moves;
    int num_moves, score;
    int prune = 0;

    if (current_depth >= max_depth)
    {
        return evaluate_board(board, player);
    }
    legal_moves = legalmoves(my_colour);
    num_moves = legal_moves[0];

    if (num_moves == 0)
    {
        return evaluate_board(board, player);
    }

    for (int i = 1; i < num_moves; i++)
    {

        temp_board = copy_board(board);
        makemove(legal_moves[i], player);
        score = minimax(board, current_depth + 1, max_depth, opponent(player), alpha, beta);

        if (player == my_colour) /* maximizing function */
        { 
            if (score > alpha)
                alpha = score;
        }

        if (player == opponent(my_colour)) /* minimizing function */
        { 
            if (score < beta)
                beta = score;
        }

        if (alpha > beta)
            prune = 1;

        board = copy_board(temp_board);
        if (prune == 1)
        {
            /* share alphabeta */
            if(current_depth < 2){
                alpha_beta_sharing(alpha, beta);
            }           

            break;
        }
    }

    if (player == my_colour)
        return alpha;
    else
        return beta;
}
/**
 * Function which allows one process to share alpha beta 
 * values with other processes. 
 * 
 * @param alpha
 * @param beta  
 * 
 */
void alpha_beta_sharing(int alpha, int beta)
{
    char buffer[100];
    int available, a, b;
    int position = 0;
    MPI_Pack(&alpha, 1, MPI_INT, buffer, 100, &position, MPI_COMM_WORLD);
    MPI_Pack(&beta, 1, MPI_INT, buffer, 100, &position, MPI_COMM_WORLD);
    for (int i = 0; i < size; i++)
    {
        if (i != rank)
        {
            MPI_Iprobe(i, SHARE, MPI_COMM_WORLD, &available, MPI_STATUS_IGNORE);
            if (available)
            {
#ifdef DBUG
                printf("Old alpha %d  & beta %d\n", alpha, beta);
#endif
                MPI_Recv(buffer, 100, MPI_PACKED, i, SHARE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Unpack(buffer, 100, &position, &a, 1, MPI_INT, MPI_COMM_WORLD);
                MPI_Unpack(buffer, 100, &position, &b, 1, MPI_INT, MPI_COMM_WORLD);

                if (a > alpha)
                    alpha = a;
                if (b < beta)
                    beta = b;
          
            
#ifdef DBUG
                printf("Recived alpha %d & beta %d\n", alpha, beta);
#endif

            }
        }
    }
    for (int i = 0; i < size; i++)
    {
        if (rank != i)
        {
#ifdef DBUG
            printf("Sharing alpha %d & beta %d\n", alpha, beta);
#endif
            MPI_Bsend(buffer, position, MPI_PACKED, i, SHARE, MPI_COMM_WORLD);
        }
    }
}
/**
 * Funciton to evaluate the state of the board
 * after a certan move was made
 * 
 * @param board
 * @param player
 * 
 * @return evaluation rating
 */
int evaluate_board(int *board, int player)
{
    int player_moves, opp_moves, move;
    player_moves = 0;
    opp_moves = 0;
    for (move = 11; move <= 88; move++)
    {
        if (legalp(move, player))
            player_moves = weights[move] + player_moves;
        if (legalp(move, opponent(player)))
            opp_moves = weights[move] + opp_moves;
    }

    return player_moves - opp_moves;
}
/*
    Called when the other engine has made a move. The move is given in a
    string parameter of the form "xy", where x and y represent the row
    and column where the opponent's piece is placed, respectively.
 */
void play_move(char *move)
{
    int loc;
    if (my_colour == EMPTY)
    {
        my_colour = WHITE;
    }
    if (strcmp(move, "pass") == 0)
    {
        return;
    }
    loc = get_loc(move);
    makemove(loc, opponent(my_colour));
}

void game_over()
{
    free_board();
    MPI_Finalize();
}

void get_move_string(int loc, char *ms)
{
    int row, col, new_loc;
    new_loc = loc - (9 + 2 * (loc / 10));
    row = new_loc / 8;
    col = new_loc % 8;
    ms[0] = row + '0';
    ms[1] = col + '0';
    ms[2] = '\n';
    ms[3] = 0;
}

int get_loc(char *movestring)
{
    int row, col;
    row = movestring[0] - '0';
    col = movestring[1] - '0';
    return (10 * (row + 1)) + col + 1;
}

int *legalmoves(int player)
{
    int move, i;
    moves[0] = 0;
    i = 0;
#ifdef DEBUG
    if (rank == 0)
        printf("all moves");
#endif
    for (move = 11; move <= 88; move++)
        if (legalp(move, player))
        {
            i++;
            moves[i] = move;
#ifdef DEBUG
            printf(" %d", moves[i]);
#endif
        }
    moves[0] = i;

#ifdef DEBUG
    printf("\n");
#endif
    return moves;
}

int legalp(int move, int player)
{
    int i;
    if (!validp(move))
        return 0;
    if (board[move] == EMPTY)
    {
        i = 0;
        while (i <= 7 && !wouldflip(move, ALLDIRECTIONS[i], player))
            i++;
        if (i == 8)
            return 0;
        else
            return 1;
    }
    else
        return 0;
}

int validp(int move)
{
    if ((move >= 11) && (move <= 88) && (move % 10 >= 1) && (move % 10 <= 8))
        return 1;
    else
        return 0;
}

int wouldflip(int move, int dir, int player)
{
    int c;
    c = move + dir;
    if (board[c] == opponent(player))
        return findbracketingpiece(c + dir, dir, player);
    else
        return 0;
}

int findbracketingpiece(int square, int dir, int player)
{
    while (board[square] == opponent(player))
        square = square + dir;
    if (board[square] == player)
        return square;
    else
        return 0;
}

int opponent(int player)
{
    if (player == BLACK)
        return WHITE;
    if (player == WHITE)
        return BLACK;
    fprintf(fp, "illegal player\n");
    return EMPTY;
}

int randomstrategy(int player)
{
    int r;
    int *moves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
    memset(moves, 0, LEGALMOVSBUFSIZE);
    if (player == 1)
        my_colour = BLACK;
    else
        my_colour = WHITE;
    legalmoves(my_colour);
    if (moves[0] == 0)
    {
        /* no more legal moves */
        return -1;
    }
    srand(time(NULL));
    /*choose random move from moves array */
    r = moves[(rand() % moves[0]) + 1];
    //free(moves);
    return (r);
}

void makemove(int move, int player)
{
    int i;
    board[move] = player;
    for (i = 0; i <= 7; i++)
        makeflips(move, ALLDIRECTIONS[i], player);
}

void makeflips(int move, int dir, int player)
{
    int bracketer, c;
    bracketer = wouldflip(move, dir, player);
    if (bracketer)
    {
        c = move + dir;
        do
        {
            board[c] = player;
            c = c + dir;
        } while (c != bracketer);
    }
}

void printboard()
{
    int row, col;
    fprintf(fp, "   1 2 3 4 5 6 7 8 [%c=%d %c=%d]\n",
            nameof(BLACK), count(BLACK, board), nameof(WHITE), count(WHITE, board));
    for (row = 1; row <= 8; row++)
    {
        fprintf(fp, "%d  ", row);
        for (col = 1; col <= 8; col++)
            fprintf(fp, "%c ", nameof(board[col + (10 * row)]));
        fprintf(fp, "\n");
    }
    fflush(fp);
}

char nameof(int piece)
{
    assert(0 <= piece && piece < 5);
    return (piecenames[piece]);
}

int count(int player, int *board)
{
    int i, cnt;
    cnt = 0;
    for (i = 1; i <= 88; i++)
        if (board[i] == player)
            cnt++;
    return cnt;
}
/**
 * Debuf function used for checking
 * if array of moves divided sucessfully
 */ 
void print_process_moves(int *local_moves, int *send_counts)
{
    printf("Process %d moves ", rank);
    for (int i = 0; i < send_counts[rank]; i++)
    {
        printf("%d ", local_moves[i]);
    }
    printf("\n");
}
