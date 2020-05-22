typedef struct ex_message{
  int player_id;
  int c;
  int character;
  int x;
  int y;
} exe_message;

typedef struct player_info{
  int client_fd;
  int color;
  int pacman_x,pacman_y;
  int monster_x,monster_y;
}player_info;

typedef struct player_thread_init_message{//used only by server
  int player_id;
  int client_sock_fd;
}player_thread_init_message;
