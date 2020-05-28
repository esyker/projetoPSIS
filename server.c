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

#include "server.h"
#include "UI_library.h"
#include "linked_list.h"

//gcc server.c linked_list.c UI_library.c -o server -lSDL2 -lSDL2_image -lpthread -Wall -g

Uint32 Event_ShowFigure;
int server_socket;
LinkedList* players;
LinkedList *fruits;
board game_board;
score_info score;
server_info server;


/*******************************************************************************/
/*******                   LIST OPERATIONS                         *************/
/*****************************************************************************/
/*****************************************************************************/
/*   THIS MODULE CONTAINS                                                    */
/*    AUXILIARY FUNCTIONS TO OVERRIDE LIST OPERATIONS                       */
/*    THAT ARE SPECIFIC TO THIS PROJECT,  SUCH AS TRASVERSE OR NODE REMOVAL*/
/****************************************************************************/

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

void send_to_players_no_lock(void *msg){
  trasverse_no_lock(players,msg,send_to_player);
}


void send_to_players_2_messages(void *msg){
  trasverse(players,msg,send_to_player_2_messages);
}

/********************************************************************/
/********************************************************************/
/*                      BOARD FUNCTIONS                             */
/********************************************************************/
/*********************************************************************/
/* THIS MODULE IS RELATED TO LOADING BOARD,  DESTROYING BOARD AND   */
/* BOARD LOGIC                                                       */
/*********************************************************************/
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
  printf("%s",line);

  game_board.array=(board_square**)malloc(game_board.size_y*sizeof(board_square*));
  for(int i=0;i<game_board.size_y;i++){
    game_board.array[i]=(board_square*)malloc(game_board.size_x*sizeof(board_square));
  }

  if (pthread_mutex_init(&game_board.numb_players_mutex, NULL) != 0) {
        printf("\n board mutex for players init has failed\n");
        exit(-1);
  }
  game_board.numb_bricks=0;
  game_board.numb_players=0;

  for(int j =0;j<game_board.size_y;j++){
    if(fgets(line,1024,fptr)==NULL){
      printf("Error reading file!");
      exit(-1);
    }
    printf("%s",line);
    for(int i=0;i<game_board.size_x;i++){
      game_board.array[j][i].player_id=-1;
      if(line[i]=='B'){
        game_board.array[j][i].figure_type=BRICK;
        game_board.numb_bricks++;
      }
      else{
        game_board.array[j][i].figure_type=EMPTY;
      }
      game_board.array[j][i].player_id=-1;
      game_board.array[j][i].player=NULL;
      game_board.array[j][i].fruit=NULL;
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

server_message assignRandCoords(player_info* player,int figure_type,int init){
  int color,player_id;

  color=player->color;
  player_id=player->client_fd;

  int i,j;
  while(1){
    j=rand()%game_board.size_y;
    i=rand()%game_board.size_x;
    if(init||(i!=player->pacman_x&&i!=player->monster_x))
      pthread_mutex_lock(&game_board.line_lock[i]);
    if(init||(j!=player->pacman_y&&j!=player->monster_y))
      pthread_mutex_lock(&game_board.column_lock[j]);
    if(game_board.array[j][i].figure_type==EMPTY){
      game_board.array[j][i].figure_type=figure_type;
      game_board.array[j][i].color=color;
      game_board.array[j][i].player_id=player_id;
      game_board.array[j][i].player=player;
      if(init||(i!=player->pacman_x&&i!=player->monster_x))
        pthread_mutex_unlock(&game_board.line_lock[i]);
      if(init||(j!=player->pacman_y&&j!=player->monster_y))
        pthread_mutex_unlock(&game_board.column_lock[j]);
      break;
    }
    if(init||(i!=player->pacman_x&&i!=player->monster_x))
      pthread_mutex_unlock(&game_board.line_lock[i]);
    if(init||(j!=player->pacman_y&&j!=player->monster_y))
      pthread_mutex_unlock(&game_board.column_lock[j]);
  }
  if(figure_type==MONSTER){
    player->monster_x=i;
    player->monster_y=j;
    player->monster_eaten=0;
  }
  else{
    player->pacman_x=i;
    player->pacman_y=j;
    player->pacman_eaten=0;
  }
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

server_message board_to_message(int x, int y){
  server_message msg;
  msg.type=game_board.array[y][x].figure_type;
  msg.c=game_board.array[y][x].color;
  msg.x=x;
  msg.y=y;
  return msg;
}

server_message change_board(int x, int y,int figure_type,
  player_info* player,fruit_info* fruit,int color,int id){
  server_message msg;
  msg.type=figure_type;
  msg.x=x;
  msg.y=y;
  msg.player_id=id;
  msg.c=color;
  game_board.array[y][x].figure_type=figure_type;
  game_board.array[y][x].color=color;
  game_board.array[y][x].player_id=id;
  game_board.array[y][x].player=player;
  game_board.array[y][x].fruit=fruit;
  return msg;
}

server_message* validate_play_get_answer(client_message input, player_info* player){

  server_message* output;
  server_message msg;
  int send_message=0;
  fruit_info* new_fruit;
  player_info* new_player;
  int fruit_locked=0, player_locked=0;

  pthread_mutex_lock(&(player->mutex));

  int new_x=input.x;
  int new_y=input.y;
  int current_x,current_y;
  //find the position in the board
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

  if((player->pacman_eaten==1&&input.figure_type==PACMAN)||
  (player->monster_eaten==1&&input.figure_type==MONSTER)){
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  if(abs(current_x-new_x)>1||abs(current_y-new_y)>1
  ||(abs(current_x-new_x)>0&&abs(current_y-new_y)>0)
  ||(new_x<0&&current_x!=0)
  ||(new_x>=game_board.size_x&&current_x!=game_board.size_x-1)
  ||(new_y<0&&current_y!=0)
  ||(new_y>=game_board.size_y&&current_y!=game_board.size_y-1)
  ||(new_x==current_x&&new_y==current_y)){
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  if(new_x<0) new_x=1;
  if(new_y<0) new_y=1;
  if(new_x>=game_board.size_x) new_x=game_board.size_x-2;
  if(new_y>=game_board.size_y) new_y=game_board.size_y-2;

  //CHARACTER MOVES TO BRICK ->BOUNCE
  if(game_board.array[new_y][new_x].figure_type==BRICK){
    int bounce_x=current_x,bounce_y=current_y;
    if(new_x<current_x)bounce_x++;
    else if(new_x>current_x)bounce_x--;
    if(new_y<current_y)bounce_y++;
    else if(new_y>current_y)bounce_y--;
    if(bounce_x>=game_board.size_x||bounce_x<0||bounce_y>=game_board.size_y
      ||bounce_y<0)
    {
      pthread_mutex_unlock(&(player->mutex));
      return 0;
    }
    new_x=bounce_x;
    new_y=bounce_y;
  }

  pthread_mutex_lock(&game_board.line_lock[current_x]);
  pthread_mutex_lock(&game_board.column_lock[current_y]);
  if(game_board.array[new_y][new_x].player!=NULL&&game_board.array[new_y][new_x].player!=player){
    pthread_mutex_lock(&game_board.array[new_y][new_x].player->mutex);
    new_player=game_board.array[new_y][new_x].player;
    player_locked=1;
  }
  else if(game_board.array[new_y][new_x].fruit!=NULL){
    pthread_mutex_lock(&game_board.array[new_y][new_x].fruit->mutex);
    new_fruit=game_board.array[new_y][new_x].fruit;
    fruit_locked=1;
  }
  if(new_x!=current_x){
    pthread_mutex_lock(&game_board.line_lock[new_x]);
  }
  if(new_y!=current_y){
    pthread_mutex_lock(&game_board.column_lock[new_y]);
  }

  int new_figure_type=game_board.array[new_y][new_x].figure_type;
  int new_id=game_board.array[new_y][new_x].player_id;
  int new_color=game_board.array[new_y][new_x].color;
  int current_figure=game_board.array[current_y][current_x].figure_type;

  //CHARACTERS OF THE SAME PLAYER-> CHANGE POSITION
  if(game_board.array[new_y][new_x].player_id==player->client_fd){
    output=(server_message*)malloc(2*sizeof(server_message));
    msg=change_board(new_x,new_y,current_figure,player,NULL,player->color,player->client_fd);
    output[0]=msg;
    msg=change_board(current_x,current_y,new_figure_type,player,NULL,player->color,player->client_fd);
    output[1]=msg;
    int aux1=player->monster_x;
    int aux2=player->monster_y;
    player->monster_x=player->pacman_x;
    player->monster_y=player->pacman_y;
    player->pacman_x=aux1;
    player->pacman_y=aux2;
    send_message=1;
  }

  //CHARACTER MOVES TO EMPTY POSITION -> CHANGE POSITION
  else if(game_board.array[new_y][new_x].figure_type==EMPTY){
    output=(server_message*)malloc(2*sizeof(server_message));
    msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
    output[0]=msg;
    msg=change_board(new_x,new_y,current_figure,player,NULL,player->color,player->client_fd);
    output[1]=msg;
    if(input.figure_type==MONSTER){
      player->monster_x=new_x;
      player->monster_y=new_y;
    }
    else{
      player->pacman_x=new_x;
      player->pacman_y=new_y;
    }
    send_message=1;
  }

  //PACMAN MOVES INTO PACMAN ->CHANGE POSITION
  //MONSTER MOVES INTO MONSTER -> CHANGE POSITION
  else if((game_board.array[new_y][new_x].figure_type==input.figure_type||
  (game_board.array[new_y][new_x].figure_type==POWER_PACMAN&&input.figure_type==PACMAN))
  &&(game_board.array[new_y][new_x].player_id!=player->client_fd)){
    output=(server_message*)malloc(2*sizeof(server_message));
    msg=change_board(current_x,current_y,new_figure_type,new_player,NULL,new_color,new_id);
    output[0]=msg;
    msg=change_board(new_x,new_y,current_figure,player,NULL,player->color,player->client_fd);
    output[1]=msg;
    if(input.figure_type==MONSTER){
      player->monster_x=new_x;
      player->monster_y=new_y;
      new_player->monster_x=current_x;
      new_player->monster_y=current_y;
    }
    else{
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      new_player->pacman_x=current_x;
      new_player->pacman_y=current_y;
    }
    send_message=1;
  }

  else if(input.figure_type==PACMAN)//move_pacman
  {
    //PACMAN INTO FRUIT -> FRUIT IS EATEN AND MOVED TO RANDOM POSITION, SCORE INCREASES
    if(game_board.array[new_y][new_x].figure_type==LEMON||
      game_board.array[new_y][new_x].figure_type==CHERRY){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      msg=change_board(new_x,new_y,POWER_PACMAN,player,NULL,player->color,player->client_fd);
      output[1]=msg;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      player->score+=1;
      player->super_powered_pacman=1;
      player->monster_eat_count=2;
      sem_post(&(new_fruit->sem_fruit));
      send_message=1;
    }
    //PACMAN INTO MONSTER-> MONSTER EATEN AND MOVED TO RANDOM POSITION, OPPONENT SCORE INCREASES
    else if(game_board.array[new_y][new_x].figure_type==MONSTER){
      output=(server_message*)malloc(2*sizeof(server_message));
      if(player->super_powered_pacman){
        player->monster_eat_count--;
        msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
        output[0]=msg;
        if(player->monster_eat_count==0){
          msg=change_board(new_x,new_y,PACMAN,player,NULL,player->color,player->client_fd);
          player->super_powered_pacman=0;
        }
        else{
          msg=change_board(new_x,new_y,POWER_PACMAN,player,NULL,player->color,player->client_fd);
        }
        output[1]=msg;
        player->pacman_x=new_x;
        player->pacman_y=new_y;
        player->score+=1;
        new_player->monster_eaten=1;
        sem_post(&(new_player->sem_monster_eaten));
      }
      else{
        msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
        output[0]=msg;
        output[1]=msg;
        player->pacman_eaten=1;
        new_player->score+=1;
        sem_post(&(player->sem_pacman_eaten));
      }
      send_message=1;
    }
  }
  else if(input.figure_type==MONSTER)//move_monster
  {
    //MONSTER INTO FRUIT -> FRUIT IS EATEN AND MOVED TO RANDOM POSITION, SCORE INCREASES
    if(game_board.array[new_y][new_x].figure_type==LEMON||
      game_board.array[new_y][new_x].figure_type==CHERRY){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      msg=change_board(new_x,new_y,MONSTER,player,NULL,player->color,player->client_fd);
      output[1]=msg;
      player->monster_x=new_x;
      player->monster_y=new_y;
      player->score+=1;
      sem_post(&(new_fruit->sem_fruit));
      send_message=1;
    }
    //MONSTER AGAINST SUPERPOWERED PACMAN -> MONSTER EATEN AND MOVED TO RANDOM POSITION, OPPONENT SCORE INCREASES
    else if(game_board.array[new_y][new_x].figure_type==POWER_PACMAN){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      new_player->monster_eat_count--;
      if(new_player->monster_eat_count==0){
        msg=change_board(new_x,new_y,PACMAN,new_player,NULL,new_player->color,new_player->client_fd);
        player->super_powered_pacman=0;
      }
      else{
        msg=change_board(new_x,new_y,POWER_PACMAN,new_player,NULL,new_player->color,new_player->client_fd);
      }
      output[1]=msg;
      player->monster_eaten=1;
      new_player->score+=1;
      sem_post(&(player->sem_monster_eaten));
      send_message=1;
    }
    //MONSTER AGAINST NORMAL PACMAN -> PACMAN EATEN AND MOVED TO RANDOM POSITION, SCORE INCREASES
    else if(game_board.array[new_y][new_x].figure_type==PACMAN){
      output=(server_message*)malloc(2*sizeof(server_message));
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      msg=change_board(new_x,new_y,MONSTER,player,NULL,player->color,player->client_fd);
      output[1]=msg;
      player->monster_x=new_x;
      player->monster_y=new_y;
      player->score+=1;
      new_player->pacman_eaten=1;
      sem_post(&(new_player->sem_pacman_eaten));
      send_message=1;
    }
  }
  pthread_mutex_unlock(&game_board.line_lock[current_x]);
  pthread_mutex_unlock(&game_board.column_lock[current_y]);
  if(new_x!=current_x)
    pthread_mutex_unlock(&game_board.line_lock[new_x]);
  if(new_y!=current_y)
    pthread_mutex_unlock(&game_board.column_lock[new_y]);

  if(player_locked)pthread_mutex_unlock(&(new_player->mutex));
  if(fruit_locked)pthread_mutex_unlock(&(new_fruit->mutex));
  pthread_mutex_unlock(&(player->mutex));
  if(send_message)
    return output;
  return NULL;
}

/*******************************************************************/
/*******************************************************************/
/*                            SCORE FUNCTIONS                      */
/*******************************************************************/
/*******************************************************************/
/*      FUNCTIONS USED TO SEND SCORE                               */
/*******************************************************************/

void get_score_send(void* player_inf,void* message){
  player_info *player=(player_info*)player_inf;
  server_message* msg=(server_message*)message;
  msg->type=SCORE_MSG;
  msg->player_id=player->client_fd;
  msg->score=player->score;
  printf("PLAYER:%d SCORE:%d\n",msg->player_id,msg->score);
  send_to_players_no_lock(msg);
}

void* scoreThread(void* argv){
  struct timespec timeout;
  timeout.tv_sec=time(NULL)+7;
  timeout.tv_nsec=0;

  server_message* msg=(server_message*)malloc(sizeof(server_message));
  while(1){
    if(sem_timedwait(&score.sem_score,&timeout)==-1){
      if(errno==ETIMEDOUT){
        printf("\n  SCOREBOARD:\n");
        msg->type=SCOREBOARD;
        pthread_mutex_lock(&players->mutex);
        send_to_players_no_lock(msg);
        trasverse_no_lock(players,msg,get_score_send);
        pthread_mutex_unlock(&players->mutex);
        timeout.tv_sec=time(NULL)+7;
      }
    }
    else{
      break;
    }
  }
  pthread_barrier_wait(&score.barrier);
  return NULL;
}
/*******************************************************************/
/*******************************************************************/
/***                 FRUIT RELATED FUNCTIONS                       **/
/*******************************************************************/
/*******************************************************************/
/**     FUNCTIONS USED FOR THREADS                                 */
/*     SUCH AS PLAYER THREAD OR SERVER THREAD                      */
/*    OR THREADS THAT ASSIGN NEW POSITION WHEN THE MONSTER OR PACMAN*/
/*    ARE EATEN                                                      */
/*********************************************************************/


void removeFirstFruitInfo(LinkedList* list){
  fruit_info* fruit;
  Node* aux;
  if (list->_size > 0)
  {
    if (list->_size == 1){ //clear(list)
      fruit=(fruit_info*)list->root->data;
      fruit->exit=1;
      sem_post(&(fruit->sem_fruit));
    	list->root->data=NULL;
    	free(list->root);
      list->root = NULL;
      list->tail = NULL;
      list->_size = 0;
    }
    else
    {
      aux=list->root;
      list->root = list->root->next;
      list->root->prev = NULL;
      list->_size--;
      fruit=(fruit_info*)aux->data;
      fruit->exit=1;
      sem_post(&(fruit->sem_fruit));
    	aux->data=NULL;
    	free(aux);
    }
  }

}

void fruit_score_player_disconnect(LinkedList* players, LinkedList* fruits){
  removeFirstFruitInfo(fruits);
  removeFirstFruitInfo(fruits);
  if(players->_size==1){
    player_info* last_player=(player_info*)players->root->data;
    last_player->score=0;
  }
}

void * fruitGenerator(void * argv){
  fruit_info * fruit=(fruit_info*)argv;
  int i,j;
  server_message *event_data;
  server_message output;
  SDL_Event new_event;
  int firstime=1;

  while(1){
        if(sem_wait(&(fruit->sem_fruit))==0){
            if((fruit->exit)==1){
              pthread_mutex_lock(&fruit->mutex);
              pthread_mutex_lock(&game_board.line_lock[fruit->x]);
              pthread_mutex_lock(&game_board.column_lock[fruit->y]);
              game_board.array[fruit->y][fruit->x].figure_type=EMPTY;
              game_board.array[fruit->y][fruit->x].player_id=-1;
              game_board.array[fruit->y][fruit->x].player=NULL;
              game_board.array[fruit->y][fruit->x].fruit=NULL;
              sem_destroy(&fruit->sem_fruit);
              pthread_mutex_unlock(&game_board.line_lock[fruit->x]);
              pthread_mutex_unlock(&game_board.column_lock[fruit->y]);
              pthread_mutex_unlock(&fruit->mutex);
              pthread_mutex_destroy(&fruit->mutex);
              output.x=fruit->x;
              output.y=fruit->y;
              pthread_mutex_destroy(&(fruit->mutex));
              free(fruit);
              output.type=EMPTY;
              send_to_players(&output);
              event_data = (server_message*)malloc(sizeof(server_message));
              *event_data = output;
              SDL_zero(new_event);
              new_event.type = Event_ShowFigure;
              new_event.user.data1 = event_data;
              SDL_PushEvent(&new_event);
              break;
            }
            if(!firstime) sleep(2);
            else firstime=0;

            pthread_mutex_lock(&fruit->mutex);
            fruit->type=(rand()%2)+4;// 4 or 5
            while(1){
              j=rand()%game_board.size_y;
              i=rand()%game_board.size_x;
              pthread_mutex_lock(&game_board.line_lock[i]);
              pthread_mutex_lock(&game_board.column_lock[j]);
              if(game_board.array[j][i].figure_type==EMPTY){
                game_board.array[j][i].figure_type=fruit->type;
                game_board.array[j][i].fruit=fruit;
                pthread_mutex_unlock(&game_board.line_lock[i]);
                pthread_mutex_unlock(&game_board.column_lock[j]);
                break;
              }
              pthread_mutex_unlock(&game_board.line_lock[i]);
              pthread_mutex_unlock(&game_board.column_lock[j]);
            }
            fruit->x=i;
            fruit->y=j;
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
            pthread_mutex_unlock(&fruit->mutex);
        }
  }
  return NULL;
}

void fruit_new_player(LinkedList* players, LinkedList* fruits){
  pthread_mutex_lock(&(players->mutex));
  pthread_mutex_lock(&(fruits->mutex));
  if(players->_size==1){//do not add fruit with only one player
    pthread_mutex_unlock(&(fruits->mutex));
    pthread_mutex_unlock(&(players->mutex));
    return;
  }
  else if((players->_size)>1){//remove two fruit threads fromm the fruit list
    fruit_info * fruit1=(fruit_info*)malloc(sizeof(fruit_info));
    if (pthread_mutex_init(&(fruit1->mutex), NULL) != 0) {
          printf("\nfruit mutex init has failed\n");
          exit(-1);
    }
    sem_init(&(fruit1->sem_fruit),0,1);
    add_no_lock(fruits,fruit1);
    pthread_create(&fruit1->thread_id,NULL,fruitGenerator,(void*)fruit1);
    fruit_info * fruit2=(fruit_info*)malloc(sizeof(fruit_info));
    if (pthread_mutex_init(&(fruit2->mutex), NULL) != 0) {
          printf("\nfruit mutex init has failed\n");
          exit(-1);
    }
    sem_init(&(fruit2->sem_fruit),0,1);
    add_no_lock(fruits,fruit2);
    pthread_create(&fruit2->thread_id,NULL,fruitGenerator,(void*)fruit2);
  }
  pthread_mutex_unlock(&(fruits->mutex));
  pthread_mutex_unlock(&(players->mutex));
}

/*******************************************************************/
/*******************************************************************/
/***                 PLAYER RELATED FUNCTIONS                       **/
/*******************************************************************/
/*******************************************************************/
/**     FUNCTIONS USED FOR THREADS                                 */
/*     SUCH AS PLAYER THREAD OR SERVER THREAD                      */
/*    OR THREADS THAT ASSIGN NEW POSITION WHEN THE MONSTER OR PACMAN*/
/*    ARE EATEN                                                      */
/*********************************************************************/

void player_disconect(player_info* player){
  server_message* event_data1, *event_data2;
  event_data1=(server_message*)malloc(sizeof(server_message));
  event_data2=(server_message*)malloc(sizeof(server_message));
  server_message msg;

  int monster_x,monster_y,pacman_x,pacman_y;
  pthread_mutex_lock(&(player->mutex));
  monster_x=player->monster_x;
  monster_y=player->monster_y;
  pacman_x=player->pacman_x;
  pacman_y=player->pacman_y;
  pthread_mutex_lock(&game_board.numb_players_mutex);
  game_board.numb_players--;
  pthread_mutex_unlock(&game_board.numb_players_mutex);
  pthread_mutex_lock(&game_board.line_lock[monster_x]);
  pthread_mutex_lock(&game_board.column_lock[monster_y]);
  game_board.array[monster_y][monster_x].figure_type=EMPTY;
  game_board.array[monster_y][monster_x].player_id=-1;
  game_board.array[monster_y][monster_x].player=NULL;
  game_board.array[monster_y][monster_x].fruit=NULL;
  pthread_mutex_unlock(&game_board.line_lock[monster_x]);
  pthread_mutex_unlock(&game_board.column_lock[monster_y]);
  pthread_mutex_lock(&game_board.line_lock[pacman_x]);
  pthread_mutex_lock(&game_board.column_lock[pacman_y]);
  game_board.array[pacman_y][pacman_x].figure_type=EMPTY;
  game_board.array[pacman_y][pacman_x].player_id=-1;
  game_board.array[pacman_y][pacman_x].player=NULL;
  game_board.array[pacman_y][pacman_x].fruit=NULL;
  pthread_mutex_unlock(&game_board.line_lock[pacman_x]);
  pthread_mutex_unlock(&game_board.column_lock[pacman_y]);
  pthread_mutex_unlock(&(player->mutex));
  msg.type=EMPTY;
  msg.x=monster_x;
  msg.y=monster_y;
  *event_data1=msg;
  msg.x=pacman_x;
  msg.y=pacman_y;
  *event_data2=msg;
  pthread_mutex_lock(&(players->mutex));
  pthread_mutex_lock(&(fruits->mutex));
  removeNode_no_lock(players,player);
  fruit_score_player_disconnect(players,fruits);
  pthread_mutex_unlock(&(fruits->mutex));
  pthread_mutex_unlock(&(players->mutex));
  player->exit=1;
  sem_post(&(player->sem_inact));
  sem_post(&(player->sem_monster_eaten));
  sem_post(&(player->sem_pacman_eaten));
  send_to_players(event_data1);
  send_to_players(event_data2);
  pthread_barrier_wait(&player->barrier);
  pthread_mutex_destroy(&(player->mutex));
  close(player->client_fd);
  free(player);
  SDL_Event new_event;
  SDL_zero(new_event);
  new_event.type = Event_ShowFigure;
  new_event.user.data1 = event_data1;
  SDL_PushEvent(&new_event);
  SDL_zero(new_event);
  new_event.type = Event_ShowFigure;
  new_event.user.data1 = event_data2;
  SDL_PushEvent(&new_event);
  return;
}

void * monsterEaten(void * argv){
  player_info* player =(player_info*) argv;
  server_message msg;

  while(1){
    if(sem_wait(&(player->sem_monster_eaten))==0){
      if((player->exit)==1){//player exits
        break;
      }
      pthread_mutex_lock(&player->mutex);
      msg=assignRandCoords(player,MONSTER,NOT_INIT);
      pthread_mutex_unlock(&player->mutex);
      send_to_players(&msg);
    }
  }
  sem_destroy(&(player->sem_monster_eaten));
  pthread_barrier_wait(&player->barrier);
  return NULL;
}

void * pacmanEaten(void * argv){
  player_info* player =(player_info*) argv;
  server_message msg;

  while(1){
    if(sem_wait(&(player->sem_pacman_eaten))==0){
      if((player->exit)==1){//player exits
        break;
      }
      pthread_mutex_lock(&player->mutex);
      msg=assignRandCoords(player,PACMAN,NOT_INIT);
      pthread_mutex_unlock(&player->mutex);
      send_to_players(&msg);
    }
  }
  sem_destroy(&(player->sem_pacman_eaten));
  pthread_barrier_wait(&player->barrier);
  return NULL;
}

void * playerInactivity(void * argv){
  player_info* player =(player_info*) argv;
  server_message msg1,msg2;
  struct timespec timeout;
  timeout.tv_sec=time(NULL)+8;
  timeout.tv_nsec=0;

  int pacman_x, pacman_y;
  SDL_Event new_event;
  server_message* event_data;
  int current_figure;

  while(1){
    if(sem_timedwait(&(player->sem_inact),&timeout)==-1){
        if(errno==ETIMEDOUT){
          SDL_zero(new_event);
          new_event.type = Event_ShowFigure;
          //lock_player_mutex(player,0);
          pthread_mutex_lock(&(player->mutex));
          pacman_x=player->pacman_x;
          pacman_y=player->pacman_y;
          msg1.type=EMPTY;
          msg1.x=pacman_x;
          msg1.y=pacman_y;
          pthread_mutex_lock(&(game_board.line_lock[player->pacman_x]));
          pthread_mutex_lock(&(game_board.column_lock[player->pacman_y]));
          current_figure=game_board.array[pacman_y][pacman_x].figure_type;
          game_board.array[pacman_y][pacman_x].figure_type=EMPTY;
          game_board.array[pacman_y][pacman_x].player_id=-1;
          game_board.array[pacman_y][pacman_x].player=NULL;
          game_board.array[pacman_y][pacman_x].fruit=NULL;
          pthread_mutex_unlock(&(game_board.line_lock[player->pacman_x]));
          pthread_mutex_unlock(&(game_board.column_lock[player->pacman_y]));
          event_data=(server_message*)malloc(sizeof(server_message));
          *event_data=msg1;
          new_event.user.data1 = event_data;
          SDL_PushEvent(&new_event);
          send_to_players(&msg1);
          msg2=assignRandCoords(player,current_figure,NOT_INIT);
          pthread_mutex_unlock(&(player->mutex));
          send_to_players(&msg2);
          timeout.tv_sec=time(NULL)+8;
        }
    }
    else{
      if((player->exit)==1){//player exits
        break;
      }
      timeout.tv_sec=time(NULL)+8;
    }
  }

  sem_destroy(&(player->sem_inact));
  pthread_barrier_wait(&player->barrier);
  return NULL;
}

//SERVER THREADs
void * playerThread(void * argv){

  player_info* player=(player_info*)argv;

  int err_rcv;

  server_message* result;
  server_message msg;
  server_message *event_data1,*event_data2;
  SDL_Event new_event;
  client_message client_msg;

  int client_sock_fd=player->client_fd;
  int my_color=0;

  //establish board size and player_id
  msg.type = INIT;
  msg.x = game_board.size_x;
  msg.y = game_board.size_y;
  msg.player_id=client_sock_fd;

  send(client_sock_fd, &msg, sizeof(msg), 0);

  if((err_rcv = recv(client_sock_fd, &my_color,sizeof(int), 0)) > 0 ){
    if(my_color<0||my_color>360){
        close(client_sock_fd);
        return NULL; //exit?
    }
  }

  player->color=my_color;

  for(int i=0;i<game_board.size_y;i++){
    for(int j=0;j<game_board.size_x;j++){
      pthread_mutex_lock(&game_board.line_lock[i]);
      pthread_mutex_lock(&game_board.column_lock[j]);
      /**/
      msg=board_to_message(j,i);
      send(client_sock_fd, &msg, sizeof(msg), 0);
      /**/
      pthread_mutex_unlock(&game_board.column_lock[j]);
      pthread_mutex_unlock(&game_board.line_lock[i]);
    }
  }
  pthread_mutex_lock(&player->mutex);
  msg=assignRandCoords(player,MONSTER,INITIALIZATION);
  send_to_players(&msg);
  msg=assignRandCoords(player,PACMAN,INITIALIZATION);
  pthread_mutex_unlock(&player->mutex);
  send_to_players(&msg);
  fruit_new_player(players,fruits);

  int message_available=2;
  time_t last_time=time(NULL);

  pthread_t thread_id;
  sem_init(&(player->sem_inact),0,0);
  pthread_create(&thread_id,NULL,playerInactivity,(void*)player);
  sem_init(&(player->sem_monster_eaten),0,0);
  pthread_create(&thread_id,NULL,monsterEaten,(void*)player);
  sem_init(&(player->sem_pacman_eaten),0,0);
  pthread_create(&thread_id,NULL,pacmanEaten,(void*)player);

  while((err_rcv = recv(client_sock_fd, &client_msg , sizeof(client_msg), 0))>0){
    if((time(NULL)-last_time)>=1){
      last_time=time(NULL);
      message_available=2;
    }
    if(message_available>0){
      if((result=validate_play_get_answer(client_msg,player))!=NULL){
        send_to_players_2_messages(result);
        SDL_zero(new_event);
        new_event.type = Event_ShowFigure;
        event_data1 = (server_message*)malloc(sizeof(server_message));
        *event_data1 = result[0];
        new_event.user.data1 = event_data1;
        SDL_PushEvent(&new_event);
        SDL_zero(new_event);
        new_event.type = Event_ShowFigure;
        event_data2 = (server_message*)malloc(sizeof(server_message));
        *event_data2 = result[1];
        new_event.user.data1 = event_data2;
        SDL_PushEvent(&new_event);
        free(result);
        message_available--;
        sem_post(&(player->sem_inact));
      }
    }
  }
  player_disconect(player);
  return NULL;
}

void *serverThread(void * argc){
  server_message msg;
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);

  players=constructList();
  fruits=constructList();

  pthread_t thread_id;

  int new_client_fd=0;
  player_info* new_player_info;

  sem_init(&(score.sem_score),0,0);
  if(pthread_barrier_init(&score.barrier,NULL,2)!=0){
        printf("\n score barrier init has failed\n");
        exit(-1);
  }
  pthread_create(&thread_id,NULL,scoreThread,NULL);

  while(1){
    //Server waits for a new client to connect
     new_client_fd = accept(server_socket,(struct sockaddr *)&client_addr,&size_addr);

     pthread_mutex_lock(&game_board.numb_players_mutex);
     if((game_board.size_x*game_board.size_y-game_board.numb_bricks
       -(game_board.numb_players-1)*2
       -(game_board.numb_players*2))<4){
      msg.type=BOARD_FULL;
      send(new_client_fd,&msg, sizeof(server_message), 0);
      close(new_client_fd);
      pthread_mutex_unlock(&game_board.numb_players_mutex);
      continue;
    }
    game_board.numb_players++;
    pthread_mutex_unlock(&game_board.numb_players_mutex);

     if(new_client_fd == -1){
       perror("accept");
       exit(EXIT_FAILURE);
     }
     new_player_info=(player_info*)malloc(sizeof(player_info));
     new_player_info->client_fd=new_client_fd;
     new_player_info->exit=0;
     new_player_info->score=0;
     if (pthread_mutex_init(&(new_player_info->mutex), NULL) != 0) {
           printf("\n player mutex init has failed\n");
           exit(-1);
     }
     if(pthread_barrier_init(&new_player_info->barrier,NULL,4)!=0){
           printf("\n player barrier init has failed\n");
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

/******************************************************/
/*****************************************************/
/*              FUNCTIONS USED TO FREE MEMORY AND EXIT */
/******************************************************/
/******************************************************/

void destroyBoard(){
  if(game_board.line_lock!=NULL){
    for(int i=0;i<game_board.size_x;i++){
      if(&game_board.line_lock[i]!=NULL)
        pthread_mutex_destroy(&game_board.line_lock[i]);
    }
  free(game_board.line_lock);
  }

  if(game_board.column_lock!=NULL){
    for(int i=0;i<game_board.size_y;i++){
      if(&game_board.column_lock[i]!=NULL)
        pthread_mutex_destroy(&game_board.column_lock[i]);
    }
  free(game_board.column_lock);
  }

  for(int i=0;i<game_board.size_y;i++){
    if(game_board.array[i]!=NULL){
      free(game_board.array[i]);
    }
  }
  if(game_board.array!=NULL){
    free(game_board.array);
  }
}

void destroyPlayer(void* _player){
  player_info* player= (player_info*)_player;
  pthread_mutex_destroy(&(player->mutex));
  close(player->client_fd);
  player->exit=1;
  sem_post(&(player->sem_monster_eaten));
  sem_post(&(player->sem_pacman_eaten));
  sem_post(&player->sem_inact);
  pthread_barrier_wait(&player->barrier);
  close(player->client_fd);
  pthread_mutex_destroy(&(player->mutex));
  free(player);
}

void destroyFruit(void* _fruit){
  fruit_info* fruit= (fruit_info*)_fruit;
  fruit->exit=1;
  sem_post(&fruit->sem_fruit);
}

void free_game_memory_exit(){
  close_board_windows();
  close(server_socket);
  pthread_join(server.thread_id,NULL);
  sem_post(&score.sem_score);
  pthread_barrier_wait(&score.barrier);
  if(players!=NULL)
    destroy(players,destroyPlayer);
  if(fruits!=NULL)
    destroy(fruits,destroyFruit);
  destroyBoard();
  printf("Memory freed and sockets closed!\nEND\n");
  exit(0);
}

/*****************************************************/
/******************************************************/
/*               MAIN THREAD                          */
/********************************************************/
/*********************************************************/
/**********************************************************/
/* INITIALIZES OTHER THREADS AND RENDERS GRAPHICS         */


int main(int argc , char* argv[]){

  srand(time(NULL));
  SDL_Event event;
  int done = 0;

  Event_ShowFigure = SDL_RegisterEvents(1);

  //do not abort program for broken pipe
  signal(SIGPIPE, SIG_IGN);
  //signal(SIGINT,free_game_memory_exit);

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
  for(int j=0;j<game_board.size_y;j++){
    printf("\n");
    for(int i=0;i<game_board.size_x;i++)
    {
      printf("%d",game_board.array[j][i].figure_type);
    }
  }
  create_board_window(game_board.size_x,game_board.size_y);

  //game_board mutexes aren't used because only main thread is running at this point
  for(int j=0;j<game_board.size_y;j++){
    for(int i=0;i<game_board.size_x;i++){
        if(game_board.array[j][i].figure_type==BRICK)
          paint_brick(i,j);
    }
  }

  pthread_create(&server.thread_id, NULL, serverThread, NULL);
  pthread_detach(server.thread_id);


  int x,y,figure_type,color;

  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }
      if(event.type == Event_ShowFigure){
        server_message * data_ptr;
        data_ptr = event.user.data1;
        figure_type = data_ptr->type;
        x=data_ptr->x;
        y=data_ptr->y;
        color=data_ptr->c;
        int r,g,b;
        rgb_360(color, &r, &g, &b);

        if(figure_type==EMPTY){
          clear_place(x,y);
        }
        else if(figure_type==MONSTER){
          paint_monster(x, y, r, g, b);
        }
        else if(figure_type==PACMAN){
          paint_pacman(x, y, r, g, b);
        }
        else if(figure_type==POWER_PACMAN){
          paint_powerpacman(x,y, r, g, b);
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
        free(data_ptr);
      }
    }
  }

  free_game_memory_exit();
}
