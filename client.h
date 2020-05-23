typedef enum {INIT=-1,EMPTY=0,BRICK=1,PACMAN=2,MONSTER=3,CHERRY=4,LEMON=5,POWER_PACMAN=6,SCOREBOARD=8,SCORE_MSG=9}event_type;

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
    int figure_type;//2 pacman, 3 monster, -1 init
    //int movement_direction;//1 down, 2 up, 3 left, 4 right
    int x;
    int y;
}client_message;

typedef struct player_info{
  int player_id;
  int x_monster, y_monster;
  int x_pacman,y_pacman;
  pthread_mutex_t monster_lock;
  pthread_mutex_t pacman_lock;
}player_info;
