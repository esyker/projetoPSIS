#ifndef SERVER_H
#define SERVER_H

#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

// Type of events - used to specify the type of event when sending messages and pushing SDL events
typedef enum {INIT=-1,EMPTY=0,BRICK=1,PACMAN=2,MONSTER=3,CHERRY=4,LEMON=5,POWER_PACMAN=6,BOARD_FULL=7,SCOREBOARD=8,SCORE_MSG=9} event_type;
//
typedef enum {NOT_INIT=0,INITIALIZATION=1} initialization_type;

// Message sent from client to server
typedef struct server_message{
  int type;      // event_type
  int player_id; // id of the player which this message contains its move or score (-1 for non player messages)
  int score;     // score of player player_id (message of type SCORE_MSG)
  /*fields used to paint figures*/
  int c;         // Color of the pacman or monster
  int x;         // x position on the board
  int y;         // y position
} server_message;

// messages send from the client to the server
typedef struct client_message{
    int color;       // color chosen by the client
    int figure_type; // event type, the client only send this variable with INIT, PACMAN and MONSTER
    int x;           // x position
    int y;           // y position
}client_message;

// Structure used to store information about the players/clients
typedef struct player_info{
  int color;                // color of the player
  int client_fd;            // file descriptor used to send and receive messages from the client
  int pacman_x,pacman_y;    // pacman postion
  int super_powered_pacman; // 1 if true 0 if false
  int monster_eat_count;    // used to check if pacman has eaten 2 monsters
  int monster_x,monster_y;  // monster postion
  pthread_mutex_t mutex;    // used to read or write the current position of the figures
  sem_t sem_inact;          // used to signal player Inactivity
  sem_t sem_monster_eaten;  // used to signal if the players monster was eaten
  int monster_eaten;        // 0 if false 1 if true
  sem_t sem_pacman_eaten;   // used to signal of the players pacman was eaten
  int pacman_eaten;         // 0 if false 1 if true
  int score;                // score of the player/ number of things eaten by the player
  pthread_t playerThread_id;// Thread identifier of the playerThread that recives messages from the player
  pthread_barrier_t barrier;
  int exit;                 //player thread exits
}player_info;

// Structure used to store information about the fruits on the board
typedef struct fruit_info{
  int x,y;         // position
  int type;        // event_type, a fruit is either CHERRY or LEMON
  sem_t sem_fruit; // used signal if a fruit was eaten and implemnet fruit timer
  int eaten;       // 0 if false, 1 if true;
  pthread_mutex_t mutex; //used to read or write the current position of the figures
  int id;          //id is the thread id, in order to be unique
  pthread_t thread_id;
  int exit;        //fruit thread exits
}fruit_info;

// Structure used to share same the scoreboard to all clients/players
typedef struct score_info{
  sem_t sem_score;
  pthread_t thread_id;
}score_info;

// Strucure used to exit server and clean up memory
typedef struct server_info{
  pthread_t thread_id;
}server_info;

// Structure used to store information of a single square of the gameboard
typedef struct board_square{
  int figure_type; // event_type, occupier of the board square, is either EMPTY, BRICK, PACMAN, MONSTER, LEMON or CHERRY
  int color;       // color of the figure, for types PACMAN or MONSTER
  int player_id;   // id of the player owner of the figure, or -1 for empty, fruit or brick
  player_info* player;  // pointer to player strucuture, if the type is PACMAN or MONSTER
  fruit_info* fruit;    // pointer to fruit strucuture, if the type is CHERRY or LEMON
}board_square;

// strucuture used to store information on the whole game board, and mantain syncronization across threads
typedef struct board{
  board_square** array;        // 2D array of board squares, for each position of the board
  pthread_mutex_t* line_lock;  // mutexes used to mantain sycnronization lines of the board, across the various playerThreads
  pthread_mutex_t* column_lock;// mutexes used to mantain sycnronization columns of the board
  int size_x, size_y;          // size of the game board
  int numb_bricks;             // number of bricks on the board, read from the board file, used to obtain the number of spaces avalible
  int numb_players;            // number of players, used to obtain the number of spaces avalible
  pthread_mutex_t numb_players_mutex; //mutex used to get the number of spaces availble, to see if a new player can also play
}board;

#endif /* SERVER_H */
