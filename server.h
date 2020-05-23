#include <pthread.h>
#include <semaphore.h>

typedef enum {INIT=-1,EMPTY=0,BRICK=1,PACMAN=2,MONSTER=3,CHERRY=4,LEMON=5,POWER_PACMAN=6}event_type;
typedef enum{NOT_INIT=0,INITIALIZATION=1} initialization_type;

typedef struct server_message{
  int type;//-1 initialization message, 0 clear place, 1 brick, 2 pacman, 3 monster, 4 fruit, 9 update_score
  /*fields used to send score id a player*/
  int player_id;
  /* field used to send score with id*/
  int score;
  /*fields used to paint figures*/
  int c;//color
  /*used to clear or paint*/
  int x;
  int y;
} server_message;

typedef struct client_message{
    int color;
    int figure_type;//1 pacman, 2 monster,-1 init
    int x;
    int y;
    //int move_direction;//1 down, 2 up, 3 left, 4 right
}client_message;

typedef struct player_info{
  int color;
  int client_fd;
  int pacman_x,pacman_y;
  int super_powered_pacman;//1 if true 0 if false
  int monster_eat_count;//used to check if pacman has eaten 2 monsters
  int monster_x,monster_y;
  pthread_mutex_t mutex;//used to read or write the current position of the figures
  sem_t sem_inact;//used to signal player Inactivity
  sem_t sem_monster_eaten;
  int monster_eaten;//0 if false 1 if true
  sem_t sem_pacman_eaten;
  int pacman_eaten;//0 if false 1 if true
  int score;
  int exit;//player thread exits
}player_info;

typedef struct fruit_info{
  int x;
  int y;
  int type;//4 CHERRY, 5 LEMON
  sem_t sem_fruit;
  pthread_mutex_t mutex;//used to read or write the current position of the figures
  int id;//id is the thread id, in order to be unique
  pthread_t thread_id;
  int exit;//fruit thread exits
}fruit_info;

typedef struct board_square{
  int figure_type;//0=empty 1=brick 2=pacman 3=monster 4=fruit
  int color; //color of the figure
  int player_id;//id of the player owner of the figure, or -1 for empty, fruit or brick
  player_info* player;
  fruit_info* fruit;
}board_square;

typedef struct board{
  board_square** array;
  pthread_mutex_t* line_lock;
  pthread_mutex_t* column_lock;
  int size_x;
  int size_y;
}board;
