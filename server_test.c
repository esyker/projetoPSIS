#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

#include "server.h"
#include "UI_library.h"
#include "linked_list.h"

//gcc server.c linked_list.c UI_library.c -o server -lSDL2 -lSDL2_image -lpthread -Wall -g
//sudo ss -tulwn | grep LISTEN
//sudo ufw deny 8080
//sudo ufw allow 8080
//netstat -tulpn

Uint32 Event_ShowFigure;
int sock_fd;
int server_socket;
LinkedList* players;
LinkedList *fruits;
//pthread_mutex_t players_mutex;

int currSize=1;
int currPlayerCount=0;
board game_board;
int color;

int startx=0, starty=0;

void send_to_player(void* data , void *msg){
  player_info player = (*(player_info*)data);
  send(player.client_fd,msg, sizeof(server_message), 0);
}

void send_to_player_2_messages(void* data , void* msg){
  server_message* msg_array=(server_message*)msg;
  player_info player = (*(player_info*)data);
  send(player.client_fd,&msg_array[0], sizeof(server_message), 0);
  send(player.client_fd,&msg_array[1], sizeof(server_message), 0);
}

void send_to_players(void *msg){
  trasverse(players,msg,send_to_player);
}

void send_to_players_2_messages(void *msg){
  trasverse(players,msg,send_to_player_2_messages);
}

int find_by_id(void* data, void* condition){
  if(((player_info*)data)->client_fd==(*(int*)condition))
    return 1;
  else return 0;
}

player_info* find_player(int player_id){
   return (player_info*)findby(players,find_by_id,&player_id);
}

void load_board(char * file_name){

  FILE *fptr;
  char line[1024];

  if(file_name!=NULL){
    fptr = fopen(file_name,"r");
    if(fptr==NULL){
      printf("Could not read board file!");
      exit(-1);
    }
  }

  if(fgets(line,1024,fptr)==NULL){
    printf("Error reading file!");
    exit(-1);
  }
  if(sscanf(line,"%d %d",&game_board.size_x,&game_board.size_y)!=2){
    printf("Board file does not specify x and y dimensions\n!");
    exit(-1);
  }

  game_board.array=(board_square**)malloc(game_board.size_x*sizeof(board_square*));
  for(int i=0;i<game_board.size_x;i++){
    game_board.array[i]=(board_square*)malloc(game_board.size_y*sizeof(board_square));
  }

  for(int i =0;i<game_board.size_x;i++){
    if(fgets(line,1024,fptr)==NULL){
      printf("Error reading file!");
      exit(-1);
    }
    for(int j=0;j<game_board.size_y;j++){
      game_board.array[i][j].player_id=-1;
      if(line[j]==' ')
        game_board.array[i][j].figure_type=EMPTY;
      else
        game_board.array[i][j].figure_type=BRICK;
    }
  }

  game_board.line_lock=(pthread_mutex_t*)malloc(game_board.size_x*sizeof(pthread_mutex_t));
  game_board.column_lock=(pthread_mutex_t*)malloc(game_board.size_y*sizeof(pthread_mutex_t));
  for(int i=0;i<game_board.size_x;i++){
    if (pthread_mutex_init(&game_board.line_lock[i], NULL) != 0) {
          printf("\n board mutex init has failed\n");
          exit(-1);
    }
  }
  for(int i=0;i<game_board.size_y;i++){
    if (pthread_mutex_init(&game_board.column_lock[i], NULL) != 0) {
          printf("\n board mutex init has failed\n");
          exit(-1);
    }
  }

}

server_message assignRandCoords(player_info* player,int figure_type){
  int color,player_id;

  color=player->color;
  player_id=player->client_fd;

  int i,j;
  int x_lock,y_lock;

  while(1){
      if(figure_type==PACMAN){
        x_lock=player->pacman_x;
        y_lock=player->pacman_y;
      }
      else{
        x_lock=player->monster_x;
        y_lock=player->monster_y;
      }
      pthread_mutex_lock(&game_board.line_lock[x_lock]);
      pthread_mutex_lock(&game_board.column_lock[y_lock]);
      if(game_board.array[x_lock][y_lock].player==player)
        break;
      pthread_mutex_unlock(&game_board.line_lock[x_lock]);
      pthread_mutex_unlock(&game_board.column_lock[y_lock]);
  }
  pthread_mutex_lock(&(player->mutex));
  while(1){
    /*
    if(figure_type==MONSTER)
      break;*/
    i=rand()%game_board.size_x;
    j=rand()%game_board.size_y;
    if(i!=x_lock)
      pthread_mutex_lock(&game_board.line_lock[i]);
    if(j!=y_lock)
      pthread_mutex_lock(&game_board.column_lock[j]);
    if(game_board.array[i][j].figure_type==EMPTY){
      game_board.array[i][j].figure_type=figure_type;
      game_board.array[i][j].color=color;
      game_board.array[i][j].player_id=player_id;
      game_board.array[i][j].player=player;
      if(i!=x_lock)
        pthread_mutex_unlock(&game_board.line_lock[i]);
      if(j!=y_lock)
        pthread_mutex_unlock(&game_board.column_lock[j]);
      break;
    }
    if(i!=x_lock)
      pthread_mutex_unlock(&game_board.line_lock[i]);
    if(j!=y_lock)
      pthread_mutex_unlock(&game_board.column_lock[j]);
  }/*
  if(figure_type==MONSTER){
    i=startx;
    j=starty;
    game_board.array[i][j].figure_type=figure_type;
    game_board.array[i][j].color=color;
    game_board.array[i][j].player_id=player_id;
    starty++;
  }*/
  if(figure_type==MONSTER){
    player->monster_x=i;
    player->monster_y=j;
  }
  else{
    player->pacman_x=i;
    player->pacman_y=j;
  }
  pthread_mutex_unlock(&(player->mutex));
  pthread_mutex_unlock(&game_board.line_lock[x_lock]);
  pthread_mutex_unlock(&game_board.column_lock[y_lock]);
  //printf("COORDINATES:%d %d\n",i,j);
  server_message output;
  output.type=figure_type;
  output.x=i;
  output.y=j;
  output.c=color;
  output.player_id=player_id;

  server_message *event_data;
  SDL_Event new_event;
  event_data = (server_message*)malloc(sizeof(server_message));
  *event_data = output;
  SDL_zero(new_event);
  new_event.type = Event_ShowFigure;
  new_event.user.data1 = event_data;
  SDL_PushEvent(&new_event);
  return output;
}

void print_board(){
  for(int i=0;i<game_board.size_x;i++){
    printf("\n");
    for(int j=0;j<game_board.size_y;j++)
    {
      printf("%d",game_board.array[i][j].figure_type);
    }
  }
}

server_message board_to_message(int x, int y){
  server_message msg;
  msg.type=game_board.array[x][y].figure_type;
  msg.c=game_board.array[x][y].color;
  msg.x=x;
  msg.y=y;
  return msg;
}

/*
server_message* move(int figure_type,int new_x,int new_y,int player_pos){
  pthread_mutex_lock(&players_mutex);
  pthread_mutex_unlock(&players_mutex);
}*/

int validate_play_get_answer(client_message input, player_info* player){

  server_message* output;
  server_message msg;
  int send_message=0;

  pthread_mutex_lock(&(player->mutex));

  printf("Entered players mtex\n");
  int new_x=input.x;
  int new_y=input.y;
  int current_x,current_y;

  if(input.figure_type==PACMAN){
    current_x=player->pacman_x;
    current_y=player->pacman_y;
  }
  else if(input.figure_type==MONSTER){
    current_x=player->monster_x;
    current_y=player->monster_y;
  }
  else{
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  if(abs(current_x-new_x)>1||abs(current_y-new_y)>1
  ||(abs(current_x-new_x)>0&&abs(current_y-new_y)>0)
  ||(new_x<0&&current_x!=0)
  ||(new_x>=game_board.size_x&&current_x!=game_board.size_x-1)
  ||(new_y<0&&current_y!=0)
  ||(new_y>=game_board.size_y&&current_y!=game_board.size_y-1)){
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  if(new_x<0) new_x=1;
  if(new_y<0) new_y=1;
  if(new_x>=game_board.size_x) new_x=game_board.size_x-2;
  if(new_y>=game_board.size_y) new_y=game_board.size_y-2;

  if(input.figure_type==PACMAN)//move_pacman
  {
    printf("Trying x=%d lock\n",current_x);
    pthread_mutex_lock(&game_board.line_lock[current_x]);
    printf("Trying y=%d lock\n",current_y);
    pthread_mutex_lock(&game_board.column_lock[current_y]);
    printf("Trying x=%d lock\n",new_x);
    if(new_x!=current_x)
      pthread_mutex_lock(&game_board.line_lock[new_x]);
    printf("Trying y=%d lock\n",new_y);
    if(new_y!=current_y)
      pthread_mutex_lock(&game_board.column_lock[new_y]);
    printf("New x and y lock obtained\n");
    //CHARACTER MOVES TO BRICK ->BOUNCE
    if(game_board.array[new_x][new_y].figure_type==BRICK){
      int bounce_x=current_x,bounce_y=current_y;
      if(new_x<current_x)bounce_x++;
      else if(new_x>current_x)bounce_x--;
      if(new_y<current_y)bounce_y++;
      else if(new_y>current_y)bounce_y--;
      if(bounce_x>=game_board.size_x||bounce_x<0||bounce_y>=game_board.size_y
        ||bounce_y<0)
      {
        pthread_mutex_unlock(&game_board.line_lock[current_x]);
        pthread_mutex_unlock(&game_board.column_lock[current_y]);
        if(new_x!=current_x)
          pthread_mutex_unlock(&game_board.line_lock[new_x]);
        if(new_y!=current_y)
          pthread_mutex_unlock(&game_board.column_lock[new_y]);
        pthread_mutex_unlock(&(player->mutex));
        return 0;
      }
      if(bounce_x!=new_x&&bounce_x!=current_x){
        pthread_mutex_unlock(&game_board.line_lock[new_x]);
        pthread_mutex_lock(&game_board.line_lock[bounce_x]);
      }
      if(bounce_y!=new_y&&bounce_y!=current_y){
        pthread_mutex_unlock(&game_board.column_lock[new_y]);
        pthread_mutex_lock(&game_board.column_lock[bounce_y]);
      }
      new_x=bounce_x;
      new_y=bounce_y;
    }
    //CHARACTER MOVES TO EMPTY POSITION
    if(game_board.array[new_x][new_y].figure_type==EMPTY){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg.type=EMPTY;
      msg.x=current_x;
      msg.y=current_y;
      output[0]=msg;
      msg.type=PACMAN;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[current_x][current_y].figure_type=EMPTY;
      game_board.array[new_x][new_y].figure_type=PACMAN;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      send_message=1;
    }
    //CHARACTERS OF THE SAME PLAYER-> CHANGE POSITION
    else if(game_board.array[new_x][new_y].figure_type==MONSTER
      &&game_board.array[new_x][new_y].player_id==player->client_fd){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg.type=MONSTER;
      msg.x=current_x;
      msg.y=current_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[0]=msg;
      msg.type=PACMAN;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[current_x][current_y].figure_type=MONSTER;
      game_board.array[current_x][current_y].color=player->color;
      game_board.array[current_x][current_y].player_id=player->client_fd;
      game_board.array[current_x][current_y].player=player;
      game_board.array[new_x][new_y].figure_type=PACMAN;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      player->monster_x=current_x;
      player->monster_y=current_y;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      send_message=1;
    }
    //PACMAN MOVES INTO PACMAN ->CHANGE POSITION
    else if(game_board.array[new_x][new_y].figure_type==PACMAN&&
    (game_board.array[new_x][new_y].player_id!=player->client_fd)){
      output=(server_message*)malloc(2*sizeof(server_message));
      printf("CONDITION MET!\n");
      int new_id=game_board.array[new_x][new_y].player_id;
      int new_color=game_board.array[new_x][new_y].color;
      //player_info* new_player =find_player(new_id);
      player_info* new_player=game_board.array[new_x][new_y].player;
      printf("NEW PLAYER ID:%d\n",new_player->client_fd);
      pthread_mutex_lock(&(new_player->mutex));
      msg.type=PACMAN;
      msg.x=current_x;
      msg.y=current_y;
      msg.player_id=new_id;
      msg.c=new_color;
      output[0]=msg;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      game_board.array[current_x][current_y].color=new_player->color;
      game_board.array[current_x][current_y].player_id=new_player->client_fd;
      game_board.array[new_x][new_y].player=new_player;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      printf("NEW PLAYER X BEFORE:%d\n",new_player->monster_x);
      new_player->pacman_x=current_x;
      printf("NEW PLAYER X AFTER:%d\n",new_player->monster_x);
      printf("NEW PLAYER Y BEFORE:%d\n",new_player->monster_y);
      new_player->pacman_y=current_y;
      printf("NEW PLAYER Y AFTER:%d\n",new_player->monster_y);
      pthread_mutex_unlock(&(new_player->mutex));
      send_message=1;
    }
    //PACMAN INTO FRUIT -> FRUIT IS EATEN AND MOVED TO RANDOM POSITION, SCORE INCREASES
    /*else if(game_board.array[new_x][new_y].figure_type==LEMON||
      game_board.array[new_x][new_y].figure_type==CHERRY){
      output=(server_message*)malloc(2*sizeof(server_message));
      int new_id=game_board.array[new_x][new_y].player_id;
      int new_color=game_board.array[new_x][new_y].color;
      player_info* new_player =find_player(new_id);
      printf("NEW PLAYER ID:%d\n",new_player->client_fd);
      pthread_mutex_lock(&(new_player->mutex));
      msg.type=PACMAN;
      msg.x=current_x;
      msg.y=current_y;
      msg.player_id=new_id;
      msg.c=new_color;
      output[0]=msg;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      game_board.array[current_x][current_y].color=new_player->color;
      game_board.array[current_x][current_y].player_id=new_player->client_fd;
      game_board.array[new_x][new_y].player=player;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      printf("NEW PLAYER X BEFORE:%d\n",new_player->monster_x);
      new_player->pacman_x=current_x;
      printf("NEW PLAYER X AFTER:%d\n",new_player->monster_x);
      printf("NEW PLAYER Y BEFORE:%d\n",new_player->monster_y);
      new_player->pacman_y=current_y;
      printf("NEW PLAYER Y AFTER:%d\n",new_player->monster_y);
      pthread_mutex_unlock(&(new_player->mutex));
      send_message=1;
    }*/
    //
    pthread_mutex_unlock(&game_board.line_lock[current_x]);
    pthread_mutex_unlock(&game_board.column_lock[current_y]);
    if(new_x!=current_x)
      pthread_mutex_unlock(&game_board.line_lock[new_x]);
    if(new_y!=current_y)
      pthread_mutex_unlock(&game_board.column_lock[new_y]);
  }
  else if(input.figure_type==MONSTER)//move_monster
  {
    printf("Trying lock on current monster x!\n");
    pthread_mutex_lock(&game_board.line_lock[current_x]);
    printf("Trying lock on current monster y!\n");
    pthread_mutex_lock(&game_board.column_lock[current_y]);
    printf("locked current position\n");
    if(new_x!=current_x)
      pthread_mutex_lock(&game_board.line_lock[new_x]);
    if(new_y!=current_y)
      pthread_mutex_lock(&game_board.column_lock[new_y]);
    printf("locked new position\n");

    //CHARACTER MOVES TO BRICK ->BOUNCE
    if(game_board.array[new_x][new_y].figure_type==BRICK){
      int bounce_x=current_x,bounce_y=current_y;
      if(new_x<current_x)bounce_x++;
      else if(new_x>current_x)bounce_x--;
      if(new_y<current_y)bounce_y++;
      else if(new_y>current_y)bounce_y--;
      if(bounce_x>=game_board.size_x||bounce_x<0||bounce_y>=game_board.size_y
        ||bounce_y<0)
      {
        pthread_mutex_unlock(&game_board.line_lock[current_x]);
        pthread_mutex_unlock(&game_board.column_lock[current_y]);
        if(new_x!=current_x)
          pthread_mutex_unlock(&game_board.line_lock[new_x]);
        if(new_y!=current_y)
          pthread_mutex_unlock(&game_board.column_lock[new_y]);
        pthread_mutex_unlock(&(player->mutex));
        return 0;
      }
      if(bounce_x!=new_x&&bounce_x!=current_x){
        pthread_mutex_unlock(&game_board.line_lock[new_x]);
        pthread_mutex_lock(&game_board.line_lock[bounce_x]);
      }
      if(bounce_y!=new_y&&bounce_y!=current_y){
        pthread_mutex_unlock(&game_board.column_lock[new_y]);
        pthread_mutex_lock(&game_board.column_lock[bounce_y]);
      }
      new_x=bounce_x;
      new_y=bounce_y;
    }

    //CHARACTER MOVES TO EMPTY POSITION
    if(game_board.array[new_x][new_y].figure_type==EMPTY){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg.type=EMPTY;
      msg.x=current_x;
      msg.y=current_y;
      output[0]=msg;
      msg.type=MONSTER;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[current_x][current_y].figure_type=EMPTY;
      game_board.array[new_x][new_y].figure_type=MONSTER;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      player->monster_x=new_x;
      player->monster_y=new_y;
      send_message=1;
    }
    //CHARACTERS OF THE SAME PLAYER-> CHANGE POSITION
    else if(game_board.array[new_x][new_y].figure_type==PACMAN
      &&game_board.array[new_x][new_y].player_id==player->client_fd){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg.type=PACMAN;
      msg.x=current_x;
      msg.y=current_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[0]=msg;
      msg.type=MONSTER;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[current_x][current_y].figure_type=PACMAN;
      game_board.array[current_x][current_y].color=player->color;
      game_board.array[current_x][current_y].player_id=player->client_fd;
      game_board.array[current_x][current_y].player=player;
      game_board.array[new_x][new_y].figure_type=MONSTER;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      player->monster_x=new_x;
      player->monster_y=new_y;
      player->pacman_x=current_x;
      player->pacman_y=current_y;
      send_message=1;
    }
    //MONSTER MOVES INTO MONSTER -> CHANGE POSITION
    else if(game_board.array[new_x][new_y].figure_type==MONSTER&&
    (game_board.array[new_x][new_y].player_id!=player->client_fd)){
      output=(server_message*)malloc(2*sizeof(server_message));
      printf("CONDITION MET!\n");
      int new_id=game_board.array[new_x][new_y].player_id;
      int new_color=game_board.array[new_x][new_y].color;
      //player_info* new_player =find_player(new_id);
      player_info* new_player=game_board.array[new_x][new_y].player;
      printf("NEW PLAYER ID:%d\n",new_player->client_fd);
      pthread_mutex_lock(&(new_player->mutex));
      msg.type=MONSTER;
      msg.x=current_x;
      msg.y=current_y;
      msg.player_id=new_id;
      msg.c=new_color;
      output[0]=msg;
      msg.type=MONSTER;
      msg.x=new_x;
      msg.y=new_y;
      msg.player_id=player->client_fd;
      msg.c=player->color;
      output[1]=msg;
      game_board.array[new_x][new_y].color=player->color;
      game_board.array[new_x][new_y].player_id=player->client_fd;
      game_board.array[new_x][new_y].player=player;
      game_board.array[current_x][current_y].color=new_player->color;
      game_board.array[current_x][current_y].player_id=new_player->client_fd;
      game_board.array[current_x][current_y].player=player;
      player->monster_x=new_x;
      player->monster_y=new_y;
      printf("NEW PLAYER X BEFORE:%d\n",new_player->monster_x);
      new_player->monster_x=current_x;
      printf("NEW PLAYER X AFTER:%d\n",new_player->monster_x);
      printf("NEW PLAYER Y BEFORE:%d\n",new_player->monster_y);
      new_player->monster_y=current_y;
      printf("NEW PLAYER Y AFTER:%d\n",new_player->monster_y);
      pthread_mutex_unlock(&(new_player->mutex));
      send_message=1;
    }
    //MONSTER AGAINST SUPERPOWERED PACMAN -> MONSTER EATEN AND MOVED TO RANDOM POSITION, OPPONENT SCORE INCREASES

    //MONSTER AGAINST NORMAL PACMAN -> PACMAN EATEN AND MOVED TO RANDOM POSITION, SCORE INCREASES
    //
    pthread_mutex_unlock(&game_board.line_lock[current_x]);
    pthread_mutex_unlock(&game_board.column_lock[current_y]);
    if(new_x!=current_x)
      pthread_mutex_unlock(&game_board.line_lock[new_x]);
    if(new_y!=current_y)
      pthread_mutex_unlock(&game_board.column_lock[new_y]);
  }
  pthread_mutex_unlock(&(player->mutex));
  printf("Sending message\n");
  if(send_message){
      send_to_players_2_messages(output);
  }
  printf("Unlocked players mutex\n");

  if(send_message){
  SDL_Event new_event1,new_event2;
  //event_data = malloc(sizeof(server_message));
  //*event_data = msg;
  SDL_zero(new_event1);
  new_event1.type = Event_ShowFigure;
  new_event1.user.data1 = &output[0];
  SDL_PushEvent(&new_event1);
  SDL_zero(new_event2);
  new_event2.type = Event_ShowFigure;
  new_event2.user.data1 = &output[1];
  SDL_PushEvent(&new_event2);
  return 1;
  }

  return 0;
}

void player_disconect(player_info* player){
  server_message* output=(server_message*)malloc(2*sizeof(server_message));
  server_message msg;

  int monster_x,monster_y,pacman_x,pacman_y;
  pthread_mutex_lock(&(player->mutex));
  //players[player_pos].active=0;//client exited;
  //currPlayerCount--;
  monster_x=player->monster_x;
  monster_y=player->monster_y;
  pacman_x=player->pacman_x;
  pacman_y=player->pacman_y;
  pthread_mutex_lock(&game_board.line_lock[monster_x]);
  pthread_mutex_lock(&game_board.column_lock[monster_y]);
  game_board.array[monster_x][monster_y].figure_type=EMPTY;
  pthread_mutex_unlock(&game_board.line_lock[monster_x]);
  pthread_mutex_unlock(&game_board.column_lock[monster_y]);
  pthread_mutex_lock(&game_board.line_lock[pacman_x]);
  pthread_mutex_lock(&game_board.column_lock[pacman_y]);
  game_board.array[pacman_x][pacman_y].figure_type=EMPTY;
  pthread_mutex_unlock(&game_board.line_lock[pacman_x]);
  pthread_mutex_unlock(&game_board.column_lock[pacman_y]);
  pthread_mutex_unlock(&(player->mutex));
  msg.type=EMPTY;
  msg.x=monster_x;
  msg.y=monster_y;
  output[0]=msg;
  msg.x=pacman_x;
  msg.y=pacman_y;
  output[1]=msg;
  sem_destroy(&(player->sem_inact));
  removeNode(players,player);
  send_to_players(&output[0]);
  send_to_players(&output[1]);
  SDL_Event new_event1,new_event2;
  //event_data = malloc(sizeof(server_message));
  //*event_data = msg;
  SDL_zero(new_event1);
  new_event1.type = Event_ShowFigure;
  new_event1.user.data1 = &output[0];
  SDL_PushEvent(&new_event1);
  SDL_zero(new_event2);
  new_event2.type = Event_ShowFigure;
  new_event2.user.data1 = &output[1];
  SDL_PushEvent(&new_event2);
  return;
}

void * playerInactivity(void * argv){
  player_info* player =(player_info*) argv;
  server_message msg1,msg2;
  struct timespec timeout;
  timeout.tv_sec=time(NULL)+8;
  timeout.tv_nsec=0;

  int pacman_x, pacman_y;
  SDL_Event new_event;

  while(1){
    if(sem_timedwait(&(player->sem_inact),&timeout)==-1){
        if(errno==ETIMEDOUT){
          //clear current position
          printf("PLAYER POINTER:%p\n",player);
          SDL_zero(new_event);
          new_event.type = Event_ShowFigure;
          pthread_mutex_lock(&game_board.line_lock[player->pacman_x]);
          pthread_mutex_lock(&game_board.column_lock[player->pacman_y]);
          pthread_mutex_lock(&(player->mutex));
          pacman_x=player->pacman_x;
          pacman_y=player->pacman_y;
          msg1.type=EMPTY;
          msg1.x=pacman_x;
          msg1.y=pacman_y;
          game_board.array[pacman_x][pacman_y].figure_type=EMPTY;
          pthread_mutex_unlock(&game_board.line_lock[pacman_x]);
          pthread_mutex_unlock(&game_board.column_lock[pacman_y]);
          pthread_mutex_unlock(&(player->mutex));
          new_event.user.data1 = &msg1;
          SDL_PushEvent(&new_event);
          send_to_players(&msg1);
          msg2=assignRandCoords(player,PACMAN);
          send_to_players(&msg2);
          timeout.tv_sec=time(NULL)+8;
        }
    }
    else{
      timeout.tv_sec=time(NULL)+8;
    }
  }

  return NULL;
}

void * fruitGenerator(void * argv){
  fruit_info * fruit=(fruit_info*)malloc(sizeof(fruit_info));
  sem_init(&(fruit->sem_fruit),0,0);
  add(fruits,fruit);
  int i,j;
  fruit->type=(rand()%2)+4;// 4 or 5
  while(1){
    i=rand()%game_board.size_x;
    j=rand()%game_board.size_y;
    pthread_mutex_lock(&game_board.line_lock[i]);
    pthread_mutex_lock(&game_board.column_lock[j]);
    if(game_board.array[i][j].figure_type==EMPTY){
      game_board.array[i][j].figure_type=fruit->type;
      pthread_mutex_unlock(&game_board.line_lock[i]);
      pthread_mutex_unlock(&game_board.column_lock[j]);
      break;
    }
    pthread_mutex_unlock(&game_board.line_lock[i]);
    pthread_mutex_unlock(&game_board.column_lock[j]);
  }
  fruit->x=i;
  fruit->y=j;
  server_message init_output;
  init_output.type=fruit->type;
  init_output.x=i;
  init_output.y=j;
  send_to_players(&init_output);
  server_message *event_data;
  SDL_Event new_event;
  event_data = (server_message*)malloc(sizeof(server_message));
  *event_data = init_output;
  SDL_zero(new_event);
  new_event.type = Event_ShowFigure;
  new_event.user.data1 = event_data;
  SDL_PushEvent(&new_event);

  while(1){
        if(sem_wait(&(fruit->sem_fruit))){
              sleep(2);
              fruit->type=(rand()%2)+4;// 4 or 5
              while(1){
                i=rand()%game_board.size_x;
                j=rand()%game_board.size_y;
                pthread_mutex_lock(&game_board.line_lock[i]);
                pthread_mutex_lock(&game_board.column_lock[j]);
                if(game_board.array[i][j].figure_type==EMPTY){
                  game_board.array[i][j].figure_type=fruit->type;
                  pthread_mutex_unlock(&game_board.line_lock[i]);
                  pthread_mutex_unlock(&game_board.column_lock[j]);
                  break;
                }
                pthread_mutex_unlock(&game_board.line_lock[i]);
                pthread_mutex_unlock(&game_board.column_lock[j]);
              }
              fruit->x=i;
              fruit->y=j;
              server_message output;
              output.type=fruit->type;
              output.x=i;
              output.y=j;
              send_to_players(&output);
              event_data = (server_message*)malloc(sizeof(server_message));
              *event_data = output;
              SDL_zero(new_event);
              new_event.type = Event_ShowFigure;
              new_event.user.data1 = event_data;
              SDL_PushEvent(&new_event);
        }
  }
  return NULL;
}

//SERVER THREADs
void * playerThread(void * argv){

  player_info* player=(player_info*)argv;

  int err_rcv;

  server_message msg;
  server_message *event_data;
  client_message client_msg;
  SDL_Event new_event;

  printf("\nlock on mutex trying\n");
  int client_sock_fd=player->client_fd;
  printf("%d\n",client_sock_fd);
  int my_color=0;

  //establish board size and player_id
  msg.type = INIT;
  msg.x = game_board.size_x;
  msg.y = game_board.size_y;
  msg.player_id=client_sock_fd;

  send(client_sock_fd, &msg, sizeof(msg), 0);

  if((err_rcv = recv(client_sock_fd, &my_color,sizeof(int), 0)) > 0 ){
    if(my_color<0||my_color>255){
        close(client_sock_fd);
        return NULL; //exit?
    }
  }

  player->color=my_color;

  for(int i=0;i<game_board.size_x;i++){
    for(int j=0;j<game_board.size_y;j++){
      pthread_mutex_lock(&game_board.line_lock[i]);
      pthread_mutex_lock(&game_board.column_lock[j]);
      /**/
      msg=board_to_message(i,j);
      send(client_sock_fd, &msg, sizeof(msg), 0);
      /**/
      pthread_mutex_unlock(&game_board.column_lock[j]);
      pthread_mutex_unlock(&game_board.line_lock[i]);
    }
  }

  msg=assignRandCoords(player,MONSTER);
  send_to_players(&msg);
  msg=assignRandCoords(player,PACMAN);
  send_to_players(&msg);

  int message_available=2;
  time_t last_time=time(NULL);

  pthread_t thread_id;
  pthread_t thread_inact;
  sem_init(&(player->sem_inact),0,0);
  pthread_create(&thread_inact,NULL,playerInactivity,(void*)player);
  pthread_detach(thread_inact);
  pthread_create(&thread_id,NULL,fruitGenerator,NULL);
  //pthread_create(&thread_id,NULL,fruitGenerator,(void*)&(player->sem_fruit2));

  while((err_rcv = recv(client_sock_fd, &client_msg , sizeof(client_msg), 0))>0){
    if((time(NULL)-last_time)>=1){
      last_time=time(NULL);
      message_available=2;
    }
    if(message_available>0){
      printf("received %d byte %d %d %d \n", err_rcv, client_msg.figure_type,client_msg.x,client_msg.y);
      if(validate_play_get_answer(client_msg,player)){
        message_available--;
        sem_post(&(player->sem_inact));
      }
    }
    /*
    msg.type=client_msg.figure_type;
    msg.x=client_msg.x;
    msg.y=client_msg.y;
    msg.c=my_color;
    msg.player_id=client_sock_fd;

    event_data = (server_message*)malloc(sizeof(server_message));
    *event_data = msg;
    SDL_zero(new_event);
    new_event.type = Event_ShowFigure;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);

    pthread_mutex_lock(&game_board.column_lock[msg.y]);
    pthread_mutex_lock(&game_board.line_lock[msg.x]);
    game_board.array[msg.x][msg.y].player_id=client_sock_fd;
    game_board.array[msg.x][msg.y].figure_type=client_msg.figure_type;
    game_board.array[msg.x][msg.y].color=my_color;
    pthread_mutex_unlock(&game_board.column_lock[msg.y]);
    pthread_mutex_unlock(&game_board.line_lock[msg.x]);

    pthread_mutex_lock(&players_mutex);
    for(int i=0;i<currSize;i++){
      send(players[i].client_fd, &msg, sizeof(msg), 0);
    }
    pthread_mutex_unlock(&players_mutex);*/
  }
  pthread_cancel(thread_inact);
  player_disconect(player);
  //currPlayerCount--;
  return NULL;
}

void *serverThread(void * argc){
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);

  //pthread_mutex_lock(&players_mutex);
  //players=(player_info*)malloc(currSize*sizeof(player_info));
  //pthread_mutex_unlock(&players_mutex);
  players=constructList();
  fruits=constructList();

  pthread_t thread_id;

  int new_size=0;
  int new_client_fd=0;
  player_info* new_player_info;


  while(1){
    //Server waits for a new client to connect
     new_client_fd = accept(server_socket,(struct sockaddr *)&client_addr,&size_addr);
     if(new_client_fd == -1){
       perror("accept");
       exit(EXIT_FAILURE);
     }
     new_player_info=(player_info*)malloc(sizeof(player_info));
     new_player_info->client_fd=new_client_fd;
     if (pthread_mutex_init(&(new_player_info->mutex), NULL) != 0) {
           printf("\n plyaer mutex init has failed\n");
           exit(-1);
     }
     add(players,(void*)new_player_info);
     //Create a new workThread
     pthread_create(&thread_id,NULL,playerThread,(void*)new_player_info);
     sleep(2);
     pthread_detach(thread_id);
  }
  return (NULL);
}

int main(int argc , char* argv[]){

  srand(time(NULL));
  SDL_Event event;
  int done = 0;
  pthread_t thread_id;

  Event_ShowFigure = SDL_RegisterEvents(1);


  struct sockaddr_in server_local_addr;

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1){
    perror("socket: ");
    exit(-1);
  }
  server_local_addr.sin_family = AF_INET;
  server_local_addr.sin_addr.s_addr = INADDR_ANY;
  server_local_addr.sin_port = htons(3000); //PORT -> 3000
  int err = bind(server_socket, (struct sockaddr *)&server_local_addr,
     sizeof(server_local_addr));
  if(err == -1) {
    perror("bind");
    exit(-1);
  }
  if(listen(server_socket, 5) == -1) {
    perror("listen");
    exit(-1);
  }

  load_board("board.txt");

  create_board_window(game_board.size_x,game_board.size_y);

  //game_board mutexes aren't used because only main thread is running at this point
  for(int i=0;i<game_board.size_x;i++){
    for(int j=0;j<game_board.size_y;j++){
        if(game_board.array[i][j].figure_type==BRICK)
          paint_brick(i,j);
    }
  }
  /*
  if (pthread_mutex_init(&players_mutex, NULL) != 0) {
        printf("\n board mutex init has failed\n");
        exit(-1);
  }*/
  // can we do the accept here?
  // no the accept should be on the thread
  pthread_create(&thread_id, NULL, serverThread, NULL);


  int x;
  int y;
  int figure_type;

  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }
      if(event.type == Event_ShowFigure){
        server_message * data_ptr;
        data_ptr = event.user.data1;
        printf("address:%p type:%d\n",data_ptr,data_ptr->type);
        figure_type = data_ptr->type;
        x=data_ptr->x;
        y=data_ptr->y;
        color=data_ptr->c;

        if(figure_type==EMPTY){
          clear_place(x,y);
        }
        else if(figure_type==MONSTER){
          paint_monster(x, y,color,color,color);
        }else if(figure_type==PACMAN){
          paint_pacman(x, y ,color,color,color);
        }
        else if(figure_type==BRICK){
          paint_brick(x,y);//orange
        }
        else if(figure_type==CHERRY){
            paint_cherry(x,y);//red
        }
        else if(figure_type==LEMON){
            paint_lemon(x,y);//yellow
        }
        printf("new event received\n");
      }
    }
  }

  printf("fim\n");
  close_board_windows();
  exit(0);
}
