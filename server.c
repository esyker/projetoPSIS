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

#define SCORE_T 10
#define INACTIVITY_T 8

//gcc server.c linked_list.c UI_library.c -o server -lSDL2 -lSDL2_image -lpthread -Wall -g

/** GLOBAL VARIABLES **/
Uint32 Event_ShowFigure; // Variable used by funtions from the SDL library to push a visual event
int server_socket;       // Server file descriptor
LinkedList *players;     // List of players connected to the server
LinkedList *fruits;      // List of fruits (related to the number of players)
board game_board;        // Structure with the information related to the game board and logic
score_info score;        // Structure that contains the score, updated as the games goes on
server_info server;      // Structure with the thread_id of the server Thread



/**** LIST OPERATIONS *********************************************************/ //might change to list
// THIS MODULE CONTAINS AUXILIARY FUNCTIONS TO OVERRIDE LIST OPERATIONS THAT
// ARE SPECIFIC TO THIS PROJECT,  SUCH AS TRASVERSE OR NODE REMOVAL

/**
 * Name:               send_to_player
 * Purpose:            Send a certain msg to the player (self-explanatory)
 * Inputs:
 *   Parameters:
 *              (void) * data - pointer to the player information to get that players address
 *              (void) * msg - message to send
 *   Globals:          None
 * Outputs:
 *   Parameter:
 *    (server_message) msg - message to sent
 */
void send_to_player(void* data , void *msg){
  player_info player = (*(player_info*)data);
  send(player.client_fd, msg, sizeof(server_message), 0);
}

/**
 * Name:               send_to_player_2_messages
 * Purpose:            Send a two certain messages to the player (self-explanatory)
 * Inputs:
 *   Parameters:
 *              (void) * data - pointer to the player information to get that players address
 *              (void) * msg - messages to send
 *   Globals:          None
 * Outputs:
 *   Parameter:
 *            (void *) msg_array - messages to sent
 */
void send_to_player_2_messages(void* data , void* msg){
  server_message* msg_array=(server_message*)msg;
  player_info player = (*(player_info*)data);
  send(player.client_fd,&msg_array[0], sizeof(server_message), 0);
  send(player.client_fd,&msg_array[1], sizeof(server_message), 0);
}

/**
 * Name:               send_to_players
 * Purpose:            Send a certain message to every player on the player List
 * Inputs:
 *   Parameters:
 *              (void) * msg - message to send
 *   Globals:
 *        (LinkedList) * players - list of connected players
 * Outputs:
 *   Parameter:
 *              (void) msg - message to sent to each client/player
 * Notes:              Syncronization is ensured be the trasverse function.
 */
void send_to_players(void *msg){
  trasverse(players,msg,send_to_player);
}

/**
 * Name:               send_to_players_no_lock
 * Purpose:            Send a certain message to every player on the player List
 * Inputs:
 *   Parameters:
 *              (void) * msg - message to send
 *   Globals:
 *        (LinkedList) * players - list of connected players
 * Outputs:
 *   Parameter:
 *              (void) msg - message to sent to each client/player
 * Notes:              Syncronization isn't ensured.
 */
void send_to_players_no_lock(void *msg){
  trasverse_no_lock(players,msg,send_to_player);
}

/**
 * Name:               send_to_players_2_messages
 * Purpose:            Send two messages to every player on the player List
 * Inputs:
 *   Parameters:
 *              (void) * msg - messages to send
 *   Globals:
 *        (LinkedList) * players - list of connected players
 * Outputs:
 *   Parameter:
 *              (void) msg - messages to sent to each client/player
 * Notes:              Syncronization is ensured be the trasverse function.
 */
void send_to_players_2_messages(void *msg){
  trasverse(players,msg,send_to_player_2_messages);
}

/**** BOARD FUNCTIONS *********************************************************/
// THIS MODULE IS RELATED TO LOADING, UPDATING, DESTROYING AND LOGIC OF THE BOARD

/**
 * Name:               load_board
 * Purpose:            Initializes the game board according to the information of the file.
 * Inputs:
 *   Parameters:
 *              (char) * file_name - path to file with the board size and brick placement.
 *   Globals:
 *             (board) game_board - strucure to initialize with the information of the file
 * Outputs:
 *   Globals:
 *             (board) game_board - updated gameboard
 */
void load_board(char * file_name){
  // Auxiliary variables used to read and parse the file
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
  // The first line of the file contains the board size (width and lenght)
  if(sscanf(line,"%d %d",&game_board.size_x,&game_board.size_y)!=2){
    printf("Board file does not specify x and y dimensions\n!");
    exit(-1);
  }

  // Allocation of the game board
  game_board.array=(board_square**)malloc(game_board.size_y*sizeof(board_square*));
  if(game_board.array == NULL){
    printf("Unable to allocate memory for the game board.");
    exit(-1);
  }
  for(int i=0;i<game_board.size_y;i++){
    game_board.array[i]=(board_square*)malloc(game_board.size_x*sizeof(board_square));
    if(game_board.array[i] == NULL){
      printf("Unable to allocate memory for the game board.");
      exit(-1);
    }
  }

  // initialization of the number of players mutex,
  // used to ensure syncronization when adding players to the board
  if (pthread_mutex_init(&game_board.numb_players_mutex, NULL) != 0) {
    printf("\n board mutex for players init has failed\n");
    exit(-1);
  }

  // Initialization of variables used to check the number of available spaces
  game_board.numb_bricks=0;
  game_board.numb_players=0;

  // Parse of the file and update of the game board
  for(int j =0;j<game_board.size_y;j++){
    if(fgets(line,1024,fptr)==NULL){
      printf("Error reading file!");
      exit(-1);
    }

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

  // Initialization of the line and column mutexes used to ensure syncronization
  // across a line or collumn. [ref. ???]
  game_board.line_lock=(pthread_mutex_t*)malloc(game_board.size_x*sizeof(pthread_mutex_t));
  if(game_board.line_lock == NULL){
    printf("Unable to allocate memory for the game board line_lock mutex.");
    exit(-1);
  }
  game_board.column_lock=(pthread_mutex_t*)malloc(game_board.size_y*sizeof(pthread_mutex_t));
  if(game_board.column_lock == NULL){
    printf("Unable to allocate memory for the game board column_lock mutex.");
    exit(-1);
  }
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
  fclose(fptr);
}

/**
 * Name:               assignRandCoords
 * Purpose:            Assign random coordinates to non static board related
 *                     entities: used for players pacman and monster
 * Inputs:
 *   Parameters:
 *       (player_info) * player - player to assign random coordinates to its monster and pacman
 *               (int) figure_type - figure type/ event type
 *               (int) init - initialization flag, if the player isn't yet on the board
 *   Globals:
 *             (board) game_board - game board to update
 * Outputs:
 *   Globals:
 *             (board) game_board - updated gameboard
 *   Return:
 *        (server_msg) output - message to send to clients
 * Notes:              Syncronization is ensured throught the column and line mutexes of the board
 */
server_message assignRandCoords(player_info* player, int figure_type, int init){
  // Auxiliary variables
  int color,player_id;
  int i,j;

  color=player->color;
  player_id=player->client_fd;

  // While an EMPTY position and different from the current one isn't found
  while(1){
    j=rand()%game_board.size_y;
    i=rand()%game_board.size_x;
    if(init||(i!=player->pacman_x && i!=player->monster_x))
      pthread_mutex_lock(&game_board.line_lock[i]);
    if(init||(j!=player->pacman_y && j!=player->monster_y))
      pthread_mutex_lock(&game_board.column_lock[j]);
    // An EMPTY position has been found
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
  } else{
    player->pacman_x=i;
    player->pacman_y=j;
    player->pacman_eaten=0;
  }
  // Message to send to clients
  server_message output;
  output.type=figure_type;
  output.x=i;
  output.y=j;
  output.c=color;
  output.player_id=player_id;

  server_message *event_data;
  SDL_Event new_event;
  event_data = (server_message*)malloc(sizeof(server_message));
  if(event_data == NULL){
    printf("Unable to allocate memory assignRandCoords event_data.");
    exit(-1);
  }
  *event_data = output;
  SDL_zero(new_event);
  new_event.type = Event_ShowFigure;
  new_event.user.data1 = event_data;
  SDL_PushEvent(&new_event);
  return output;
}

/**
 * Name:               board_to_message
 * Purpose:            Create a message with the entity and its attributes for a
 *                     given position/pair of coordinates
 * Inputs:
 *   Parameters:
 *               (int) x, y - coordinates/position
 *   Globals:
 *             (board) game_board - game board with the entities in each position
 * Outputs:
 *   Return:
 *        (server_msg) output - message to send to clients
 */
server_message board_to_message(int x, int y){
  server_message msg;
  msg.type=game_board.array[y][x].figure_type;
  msg.c=game_board.array[y][x].color;
  msg.x=x;
  msg.y=y;
  return msg;
}

/**
 * Name:               change_board
 * Purpose:            Change the entity/figure type of a given position in the
 *                     board and create a message with the updated position.
 *                     Used to implement the game logic.
 * Inputs:
 *   Parameters:
 *               (int) x, y - coordinates/position
 *               (int) figure_type - entity to put in the position (x,y)
 *       (player_info) * player -pointer to player strucure of the corresponding entity
 *        (fruit_info) * fruit - pointer to fruit sturcure of the corresponding entity
 *               (int) color - color of the pacman or monster
 *               (int) id - identifier of the corresponding entity
 *   Globals:
 *             (board) game_board - game board with the entities in each coord.
 * Outputs:
 *   Return:
 *        (server_msg) output - message to send to clients
 * Notes:              Depending on what we want to change on the board some
 *                     fuction agruments may be NULL or -1
 * Examples:
 *   (empty place): player and fruit are NULL and color and id are -1 (invalid)
 *      msg = change_board(x, y, EMPTY, NULL, NULL, -1, -1);
 *   (swap player characters): fruit is NULL
 *      change_board(new_x,new_y,current_figure,player,NULL,player->color,player->client_fd);
 *      change_board(current_x,current_y,new_figure_type,player,NULL,player->color,player->client_fd);
 */
server_message change_board(int x, int y,int figure_type, player_info* player,
  fruit_info* fruit, int color, int id){
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

/**
 * Name:               validate_play_get_answer
 * Purpose:            According to a received message validate and process game
 *                     play, and create message with the validated play.
 *                     The game logic is implemented in this function.
 * Inputs:
 *   Parameters:
 *    (client_message) input - message received from the client,
 *       (player_info) player - pointer to the player that sent the message
 *   Globals:
 *             (board) game_board - game board with the entities in each coord.
 * Outputs:
 *   Globals:
 *             (board) game_board - updated gameboard with the validated play
 *   Return:
 *        (server_msg) output - message to send to clients
 * Notes:              The board updates are made by the function change_board.
 */
server_message* validate_play_get_answer(client_message input, player_info* player){
  /** Auxiliary variables **/
  server_message* output;  // messages to send to clients
  server_message msg;      // auxiliary server message
  int send_message = 0;    // 1 if the message received is valid, and messages will be sent to clients
  fruit_info* new_fruit;   // auxiliary fruit entity, used on plays that "eat fruit"
  player_info* new_player; // auxiliary monster/pacman entity, used on plays that collide
  int fruit_locked=0, player_locked=0; //auxiliary variables used to unlock mutexes if they were used

  // Parse of the input message for the proposed position
  pthread_mutex_lock(&(player->mutex));
  int new_x=input.x;
  int new_y=input.y;

  // According to the input figure type, find current position of the player entity to change.
  int current_x, current_y;
  if(input.figure_type==PACMAN){
    current_x=player->pacman_x;
    current_y=player->pacman_y;
  } else if(input.figure_type==MONSTER){
    current_x=player->monster_x;
    current_y=player->monster_y;
  } else {
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  if((player->pacman_eaten==1 && input.figure_type==PACMAN) ||
    (player->monster_eaten==1 && input.figure_type==MONSTER)){
    pthread_mutex_unlock(&(player->mutex));
    return 0;
  }

  // If the proposed position invalid or the same as the current one.
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

  // Board border moves
  if(new_x<0) new_x=1;
  if(new_y<0) new_y=1;
  if(new_x>=game_board.size_x) new_x=game_board.size_x-2;
  if(new_y>=game_board.size_y) new_y=game_board.size_y-2;

  // Character moves to a position occupied by a BRICK -> BOUNCE
  if(game_board.array[new_y][new_x].figure_type==BRICK){
    int bounce_x = current_x, bounce_y = current_y;
    if (new_x < current_x) bounce_x++;
    else if(new_x > current_x) bounce_x--;
    if (new_y < current_y) bounce_y++;
    else if(new_y > current_y) bounce_y--;
    if(bounce_x >= game_board.size_x || bounce_x<0 || bounce_y>=game_board.size_y || bounce_y<0){
      // Invalid move
      pthread_mutex_unlock(&(player->mutex));
      return 0;
    }
    // Updated proposed position with the bounce feature
    new_x=bounce_x;
    new_y=bounce_y;
  }

  pthread_mutex_lock(&game_board.line_lock[current_x]);
  pthread_mutex_lock(&game_board.column_lock[current_y]);
  if(game_board.array[new_y][new_x].player!=NULL&&game_board.array[new_y][new_x].player!=player){
    // Colision with a different player
    pthread_mutex_lock(&game_board.array[new_y][new_x].player->mutex);
    new_player=game_board.array[new_y][new_x].player;
    player_locked=1;
  }
  else if(game_board.array[new_y][new_x].fruit!=NULL){
    // Colision with a fruit
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

  // Proposed complete play
  int new_figure_type = game_board.array[new_y][new_x].figure_type;
  int new_id = game_board.array[new_y][new_x].player_id;
  int new_color = game_board.array[new_y][new_x].color;
  int current_figure = game_board.array[current_y][current_x].figure_type;

  // Characters of the same player -> swap position
  if(game_board.array[new_y][new_x].player_id==player->client_fd){
    output=(server_message*)malloc(2*sizeof(server_message));
    if(output == NULL){
      printf("Unable to alocate memory for game logic message");
      exit(-1);
    }
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

  // Character moves to empty position
  else if(game_board.array[new_y][new_x].figure_type==EMPTY){
    output=(server_message*)malloc(2*sizeof(server_message));
    if(output == NULL){
      printf("Unable to alocate memory for game logic message");
      exit(-1);
    }
    msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
    output[0]=msg;
    msg=change_board(new_x,new_y,current_figure,player,NULL,player->color,player->client_fd);
    output[1]=msg;
    if(input.figure_type==MONSTER){
      player->monster_x=new_x;
      player->monster_y=new_y;
    } else {
      player->pacman_x=new_x;
      player->pacman_y=new_y;
    }
    send_message=1;
  }

  // PACMAN moves into PACMAN -> swap position
  // MONSTER moves into MONSTER -> swap position
  else if((game_board.array[new_y][new_x].figure_type==input.figure_type||
  (game_board.array[new_y][new_x].figure_type==POWER_PACMAN&&input.figure_type==PACMAN))
  &&(game_board.array[new_y][new_x].player_id!=player->client_fd)){
    output=(server_message*)malloc(2*sizeof(server_message));
    if(output == NULL){
      printf("Unable to alocate memory for game logic message");
      exit(-1);
    }
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
  } else if(input.figure_type==PACMAN) {
    //PACMAN into FRUIT -> FRUIT is eaten and moved to random position, player score increases
    if(game_board.array[new_y][new_x].figure_type==LEMON||
      game_board.array[new_y][new_x].figure_type==CHERRY){
      output=(server_message*)malloc(2*sizeof(server_message));
      if(output == NULL){
        printf("Unable to alocate memory for game logic message");
        exit(-1);
      }
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      msg=change_board(new_x,new_y,POWER_PACMAN,player,NULL,player->color,player->client_fd);
      output[1]=msg;
      player->pacman_x=new_x;
      player->pacman_y=new_y;
      player->score+=1;
      // PACMAN is super powered, and interactions with monster will be handled differently
      player->super_powered_pacman=1;
      player->monster_eat_count=2;
      new_fruit->eaten=1;
      sem_post(&(new_fruit->sem_fruit));
      send_message=1;
    }
    else if(game_board.array[new_y][new_x].figure_type==MONSTER){
      //PACMAN into MONSTER-> MONSTER eaten and move to random position, opponent score increases
      output=(server_message*)malloc(2*sizeof(server_message));
      if(output == NULL){
        printf("Unable to alocate memory for game logic message");
        exit(-1);
      }
      if(player->super_powered_pacman){
        player->monster_eat_count--;
        msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
        output[0]=msg;
        if(player->monster_eat_count==0){
          msg=change_board(new_x,new_y,PACMAN,player,NULL,player->color,player->client_fd);
          player->super_powered_pacman=0;
        }else{
          msg=change_board(new_x,new_y,POWER_PACMAN,player,NULL,player->color,player->client_fd);
        }
        output[1]=msg;
        player->pacman_x=new_x;
        player->pacman_y=new_y;
        player->score+=1;
        new_player->monster_eaten=1;
        sem_post(&(new_player->sem_monster_eaten));
      }else{
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
  else if(input.figure_type==MONSTER){
    //MONSTER into FRUIT -> FRUIT is eaten and moved to a random position, score increases
    if(game_board.array[new_y][new_x].figure_type==LEMON||
      game_board.array[new_y][new_x].figure_type==CHERRY){
      output=(server_message*)malloc(2*sizeof(server_message));
      if(output == NULL){
        printf("Unable to alocate memory for game logic message");
        exit(-1);
      }
      msg=change_board(current_x,current_y,EMPTY,NULL,NULL,-1,-1);
      output[0]=msg;
      msg=change_board(new_x,new_y,MONSTER,player,NULL,player->color,player->client_fd);
      output[1]=msg;
      player->monster_x=new_x;
      player->monster_y=new_y;
      player->score+=1;
      new_fruit->eaten=1;
      sem_post(&(new_fruit->sem_fruit));
      send_message=1;
    }
    else if(game_board.array[new_y][new_x].figure_type==POWER_PACMAN){
      //MONSTER into SUPERPOWERED PACMAN -> MONSTER eaten and moved to a random position, opponent score increases
      output=(server_message*)malloc(2*sizeof(server_message));
      if(output == NULL){
        printf("Unable to alocate memory for game logic message");
        exit(-1);
      }
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
    //MONSTER into NORMAL PACMAN -> PACMAN eaten and moved to a random position, SCORE INCREASES
    else if(game_board.array[new_y][new_x].figure_type==PACMAN){
      output=(server_message*)malloc(2*sizeof(server_message));
      if(output == NULL){
        printf("Unable to alocate memory for game logic message");
        exit(-1);
      }
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

  if(player_locked)
    pthread_mutex_unlock(&(new_player->mutex));
  if(fruit_locked)
    pthread_mutex_unlock(&(new_fruit->mutex));
  pthread_mutex_unlock(&(player->mutex));
  if(send_message)
    return output;
  return NULL;
}

/** SCORE FUNCTIONS ***********************************************************/

/**
 * Name:               get_score_send
 * Purpose:            Get score of a certain player score and send it to all clients.
 * Inputs:
 *   Parameters:
 *             (void) * player_inf - pointer to players information, including score
 *             (void) * message - pointer to message structure to send
 * Outputs:
 *   Param:
 *   (server_message) * msg - message with score of the player to send to all clients
 */
void get_score_send(void* player_inf, void* message){
  player_info *player=(player_info*)player_inf;
  server_message* msg=(server_message*)message;
  msg->type=SCORE_MSG;
  msg->player_id=player->client_fd;
  msg->score=player->score;
  printf("PLAYER:%d SCORE:%d\n",msg->player_id,msg->score);
  send_to_players_no_lock(msg);
}

/**
 * Name:               scoreThread
 * Purpose:            Send to each player the SCOREBOARD and display it, every SCORE_T seconds.
 * Inputs:
 *   Globals:
 *      (player_info) * players - List containg the information of each player, including score
 * Outputs:
 *   Param:
 *   (server_message) * msg - message with score of the player to send to all clients
 */
void* scoreThread(void* argv){
  struct timespec timeout;
  timeout.tv_sec=time(NULL)+SCORE_T;
  timeout.tv_nsec=0;

  server_message* msg=(server_message*)malloc(sizeof(server_message));
  if(msg == NULL){
    printf("Unable to alocate memory for score message");
    exit(-1);
  }
  while(1){
    if(sem_timedwait(&score.sem_score,&timeout)==-1){
      if(errno==ETIMEDOUT){
        printf("\n  SCOREBOARD:\n");
        msg->type=SCOREBOARD;
        pthread_mutex_lock(&players->mutex);
        send_to_players_no_lock(msg);
        trasverse_no_lock(players,msg,get_score_send);
        pthread_mutex_unlock(&players->mutex);
        timeout.tv_sec=time(NULL)+SCORE_T;
      }
    }
    else{
      break;
    }
  }
  return NULL;
}

/** FRUIT RELATED FUNCTIONS ***************************************************/

/**
 * Name:               removeFruitData
 * Purpose:            Remove fruits from the board. Used when a player quits.
 * Inputs:
 *   Param:
 *             (void) * _fruit - Pointer to fruit list member to mark from removal.
 * Outputs:
 *   Param:
 *   (server_message) * msg - message with score of the player to send to all clients
 */
void removeFruitData(void* _fruit){
  fruit_info* fruit=(fruit_info*)_fruit;
  pthread_mutex_lock(&fruit->mutex);
  fruit->exit=1;
  pthread_mutex_unlock(&fruit->mutex);
  sem_post(&(fruit->sem_fruit));
}

/**
 * Name:               fruit_score_player_disconnect
 * Purpose:            Remove two fruits when a player disconects, and clear
 *                     scores if only one player is left.
 * Inputs:
 *   Param:
 *       (LinkedList) * players, * fruits - Lists to update: mark elements for
 *                    removal, clear score
 * Outputs:
 *   Param:
 *       (LinkedList) * fruits - affected/updated lists
 */
void fruit_score_player_disconnect(LinkedList* players, LinkedList* fruits){
  removeFirst_no_lock(fruits,removeFruitData);
  removeFirst_no_lock(fruits,removeFruitData);
  if(players->_size==1){
    player_info* last_player=(player_info*)players->root->data;
    last_player->score=0;
  }
}

/**
 * Name:               fruitGenerator
 * Purpose:            Thread function responsible for generating fruits, and
 *                     update positions, after a timer, once eaten.
 * Inputs:
 *   Param:
 *             (void) * argv - Pointer to fruit list member to update.
 *             (board) game_board - game board with the entities in each coord.
 * Outputs:
 *   Param:
 *       (LinkedList) * fruits - affected/updated list
 *   (server_message) * output - message updated position of the fruit to send to all clients
 */
void * fruitGenerator(void * argv){
  fruit_info * fruit=(fruit_info*)argv; // conversion of pointer to fruit list member
  // Auxiliary variables
  int i,j;
  server_message *event_data;
  server_message output;
  SDL_Event new_event;
  int firstime=1,fruit_eaten=0;

  while(1){
    if(sem_wait(&(fruit->sem_fruit))==0){
      if((fruit->exit)==1){
        // if fruit is marked for removal
        sem_destroy(&fruit->sem_fruit);
        pthread_mutex_lock(&fruit->mutex);
        if(!fruit->eaten){//assure fruit is in the board in this moment (isn't eaten)
          fruit_eaten = fruit->eaten;
          pthread_mutex_lock(&game_board.line_lock[fruit->x]);
          pthread_mutex_lock(&game_board.column_lock[fruit->y]);
          game_board.array[fruit->y][fruit->x].figure_type=EMPTY;
          game_board.array[fruit->y][fruit->x].player_id=-1;
          game_board.array[fruit->y][fruit->x].player=NULL;
          game_board.array[fruit->y][fruit->x].fruit=NULL;
          pthread_mutex_unlock(&game_board.line_lock[fruit->x]);
          pthread_mutex_unlock(&game_board.column_lock[fruit->y]);
          output.x=fruit->x;
          output.y=fruit->y;
          output.type=EMPTY;
        }
        pthread_mutex_unlock(&fruit->mutex);
        pthread_mutex_destroy(&fruit->mutex);
        free(fruit);
        if(!fruit_eaten){//ensure fruit is in the board in this moment (isn't eaten)
          send_to_players(&output);
          event_data = (server_message*)malloc(sizeof(server_message));
          if(event_data == NULL){
            printf("Unable to allocate memory for the fruitGenerator event");
            exit(-1);
          }
          *event_data = output;
          SDL_zero(new_event);
          new_event.type = Event_ShowFigure;
          new_event.user.data1 = event_data;
          SDL_PushEvent(&new_event);
        }
        break;
      }
      if(!firstime) sleep(2);
      else firstime=0;

      pthread_mutex_lock(&fruit->mutex);
      fruit->type=(rand()%2)+4;// 4 or 5 (CHERRY or LEMON)
      while(1){
        j=rand()%game_board.size_y;
        i=rand()%game_board.size_x;
        pthread_mutex_lock(&game_board.line_lock[i]);
        pthread_mutex_lock(&game_board.column_lock[j]);
        if(game_board.array[j][i].figure_type==EMPTY){
          game_board.array[j][i].figure_type=fruit->type;
          game_board.array[j][i].fruit=fruit;
          fruit->eaten=0;
          pthread_mutex_unlock(&game_board.line_lock[i]);
          pthread_mutex_unlock(&game_board.column_lock[j]);
          break;
        }
        pthread_mutex_unlock(&game_board.line_lock[i]);
        pthread_mutex_unlock(&game_board.column_lock[j]);
      }
      // create message with the updated position and send it to players
      fruit->x=i;
      fruit->y=j;
      output.type=fruit->type;
      output.x=i;
      output.y=j;
      send_to_players(&output);
      event_data = (server_message*)malloc(sizeof(server_message));
      if(event_data == NULL){
        printf("Unable to allocate memory for the fruitGenerator event");
        exit(-1);
      }
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

/**
 * Name:               fruit_new_player
 * Purpose:            Add fruits to the game, once a new player/client joins.
 * Inputs:
 *   Param:
 *       (LinkedList) * players, * fruits - Lists to update
 * Outputs:
 *   Param:
 *       (LinkedList) * fruits - affected/updated list
 */
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
    fruit1->exit=0;
    sem_init(&(fruit1->sem_fruit),0,1);
    add_no_lock(fruits,fruit1);
    pthread_create(&fruit1->thread_id,NULL,fruitGenerator,(void*)fruit1);
    pthread_detach(fruit1->thread_id);
    fruit_info * fruit2=(fruit_info*)malloc(sizeof(fruit_info));
    if (pthread_mutex_init(&(fruit2->mutex), NULL) != 0) {
      printf("\nfruit mutex init has failed\n");
      exit(-1);
    }
    fruit2->exit=0;
    sem_init(&(fruit2->sem_fruit),0,1);
    add_no_lock(fruits,fruit2);
    pthread_create(&fruit2->thread_id,NULL,fruitGenerator,(void*)fruit2);
    pthread_detach(fruit2->thread_id);
  }
  pthread_mutex_unlock(&(fruits->mutex));
  pthread_mutex_unlock(&(players->mutex));
}

/** PLAYER RELATED FUNCTIONS **************************************************/
/**     FUNCTIONS USED FOR THREADS                                 */
/*     SUCH AS PLAYER THREAD OR SERVER THREAD                      */
/*    OR THREADS THAT ASSIGN NEW POSITION WHEN THE MONSTER OR PACMAN*/
/*    ARE EATEN                                                      */
/*********************************************************************/

/**
 * Name:               player_disconect
 * Purpose:            Remove player from the game.
 * Inputs:
 *   Param:
 *       (LinkedList) * player - pointer to the player to remove
 * Outputs:
 *   Param:
 *       (LinkedList) * fruits, *players - affected/updated list
 */
void player_disconect(player_info* player){
  server_message* event_data1, *event_data2; // messages to remove the player from the other clients windows
  event_data1=(server_message*)malloc(sizeof(server_message));
  event_data2=(server_message*)malloc(sizeof(server_message));
  server_message msg;

  // Clear spaces occupied by the player that disconected
  int monster_x, monster_y, pacman_x, pacman_y; //auxiliary players characters positions
  pthread_mutex_lock(&(player->mutex));
  pthread_mutex_lock(&game_board.numb_players_mutex);
  game_board.numb_players--;
  pthread_mutex_unlock(&game_board.numb_players_mutex);
  if(!player->monster_eaten){
    monster_x = player->monster_x;
    monster_y = player->monster_y;
    pthread_mutex_lock(&game_board.line_lock[monster_x]);
    pthread_mutex_lock(&game_board.column_lock[monster_y]);
    game_board.array[monster_y][monster_x].figure_type = EMPTY;
    game_board.array[monster_y][monster_x].player_id = -1;
    game_board.array[monster_y][monster_x].player = NULL;
    game_board.array[monster_y][monster_x].fruit = NULL;
    pthread_mutex_unlock(&game_board.line_lock[monster_x]);
    pthread_mutex_unlock(&game_board.column_lock[monster_y]);
  }
  if(!player->pacman_eaten){
    pacman_x = player->pacman_x;
    pacman_y = player->pacman_y;
    pthread_mutex_lock(&game_board.line_lock[pacman_x]);
    pthread_mutex_lock(&game_board.column_lock[pacman_y]);
    game_board.array[pacman_y][pacman_x].figure_type = EMPTY;
    game_board.array[pacman_y][pacman_x].player_id = -1;
    game_board.array[pacman_y][pacman_x].player = NULL;
    game_board.array[pacman_y][pacman_x].fruit = NULL;
    pthread_mutex_unlock(&game_board.line_lock[pacman_x]);
    pthread_mutex_unlock(&game_board.column_lock[pacman_y]);
  }
  // Message connected players with the updated board positions
  msg.type = EMPTY;
  msg.x = monster_x;
  msg.y = monster_y;
  *event_data1 = msg;
  msg.x = pacman_x;
  msg.y = pacman_y;
  *event_data2 = msg;
  player->exit = 1;
  pthread_mutex_unlock(&(player->mutex));
  pthread_mutex_lock(&(players->mutex));
  pthread_mutex_lock(&(fruits->mutex));
  removeNode_no_lock(players,player);
  fruit_score_player_disconnect(players,fruits);
  pthread_mutex_unlock(&(fruits->mutex));
  pthread_mutex_unlock(&(players->mutex));
  sem_post(&(player->sem_inact));
  sem_post(&(player->sem_monster_eaten));
  sem_post(&(player->sem_pacman_eaten));
  send_to_players(event_data1);
  send_to_players(event_data2);
  pthread_barrier_wait(&player->barrier);
  pthread_mutex_destroy(&(player->mutex));
  // Disconect player and deallocate memory related to the player.
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

/**
 * Name:               monsterEaten
 * Purpose:            Superpowerd pacman and monster interaction. Move monster
 *                     to a new random position.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player of the monster that was eaten
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - message with the new position of the players monster
 */
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

/**
 * Name:               pacmanEaten
 * Purpose:            Pacman and monster interaction. Move pacman to a new
 *                     random position.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player of the pacman that was eaten
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - message with the new position of the players monster
 */
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

/**
 * Name:               playerInactivity
 * Purpose:            Player inactivity action. Move pacman to a new random
 *                     position after INACTIVITY_T seconds.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player that was inactive
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - message with the new position of the players pacman
 */
void * playerInactivity(void * argv){
  player_info* player =(player_info*) argv;
  server_message msg1,msg2;
  struct timespec timeout;
  timeout.tv_sec=time(NULL)+INACTIVITY_T;
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
          if(event_data == NULL){
            printf("Unable to allocate memory for the playerInactivity event message.");
            exit(-1);
          }
          *event_data=msg1;
          new_event.user.data1 = event_data;
          SDL_PushEvent(&new_event);
          send_to_players(&msg1);
          msg2=assignRandCoords(player,current_figure,NOT_INIT);
          pthread_mutex_unlock(&(player->mutex));
          send_to_players(&msg2);
          timeout.tv_sec=time(NULL)+INACTIVITY_T;
        }
    }
    else{
      if((player->exit)==1){//player exits
        break;
      }
      timeout.tv_sec=time(NULL)+INACTIVITY_T;
    }
  }

  sem_destroy(&(player->sem_inact));
  pthread_barrier_wait(&player->barrier);
  return NULL;
}

/**
 * Name:               playerThread
 * Purpose:            Thread which initializes player on the game, receives
 *                     the players proposed moves and checks for inactivity.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player which messages this thread will recive
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - messages to send to player
 * Note:              The connection to the player is first establish by the serverThread
 */
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
  // Send the current board to the player
  for(int j=0;j<game_board.size_y;j++){
    for(int i=0;i<game_board.size_x;i++){
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
  // Initilize players characters on the board
  pthread_mutex_lock(&player->mutex);
  msg=assignRandCoords(player,MONSTER,INITIALIZATION);
  send_to_players(&msg);
  msg=assignRandCoords(player,PACMAN,INITIALIZATION);
  pthread_mutex_unlock(&player->mutex);
  send_to_players(&msg);
  fruit_new_player(players,fruits);

  int message_available=2;
  time_t last_time=time(NULL);

  // Create threads which will handle the various interaction/actions of the players characters
  pthread_t thread_id;
  sem_init(&(player->sem_inact),0,0);
  pthread_create(&thread_id,NULL,playerInactivity,(void*)player);
  pthread_detach(thread_id);
  sem_init(&(player->sem_monster_eaten),0,0);
  pthread_create(&thread_id,NULL,monsterEaten,(void*)player);
  pthread_detach(thread_id);
  sem_init(&(player->sem_pacman_eaten),0,0);
  pthread_create(&thread_id,NULL,pacmanEaten,(void*)player);
  pthread_detach(thread_id);

  while((err_rcv = recv(client_sock_fd, &client_msg , sizeof(client_msg), 0))>0){
    if((time(NULL)-last_time)>=1){
      last_time=time(NULL);
      message_available=2;
    }
    if(message_available>0){
      if((result=validate_play_get_answer(client_msg,player))!=NULL){
        // if the proposed move was valid
        send_to_players_2_messages(result);
        SDL_zero(new_event);
        new_event.type = Event_ShowFigure;
        event_data1 = (server_message*)malloc(sizeof(server_message));
        if(event_data1 == NULL){
          printf("Unable to allocate memory for the playerThread event1 message");
          exit(-1);
        }
        *event_data1 = result[0];
        new_event.user.data1 = event_data1;
        SDL_PushEvent(&new_event);
        SDL_zero(new_event);
        new_event.type = Event_ShowFigure;
        event_data2 = (server_message*)malloc(sizeof(server_message));
        if(event_data2 == NULL){
          printf("Unable to allocate memory for the playerThread event2 message");
          exit(-1);
        }
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

/**
 * Name:               serverThread
 * Purpose:            Thread which initializes the connections for the game to
 *                     start, initializes the requires structures for the game
 *                     and starts threads for each player that connects to the
 *                     server, to read each clients/player messages.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player which messages this thread will recive
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - messages to send to player
 * Note:
 */
void *serverThread(void * argc){
  // Auxiliary variables
  server_message msg;
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);

  players=constructList();
  fruits=constructList();

  pthread_t thread_id;

  int new_client_fd=0;
  player_info* new_player_info;

  sem_init(&(score.sem_score),0,0);
  pthread_create(&score.thread_id,NULL,scoreThread,NULL);

  while(1){
    //Server waits for a new client to connect
    new_client_fd = accept(server_socket,(struct sockaddr *)&client_addr,&size_addr);
    if(new_client_fd==-1) break;

    pthread_mutex_lock(&game_board.numb_players_mutex);
    if((game_board.size_x*game_board.size_y-game_board.numb_bricks
      - (game_board.numb_players-1)*2 - (game_board.numb_players*2))<4){
      // No more avalible spaces
      msg.type=BOARD_FULL;
      send(new_client_fd,&msg, sizeof(server_message), 0);
      close(new_client_fd);
      pthread_mutex_unlock(&game_board.numb_players_mutex);
      continue;
    }
    game_board.numb_players++;
    pthread_mutex_unlock(&game_board.numb_players_mutex);
    new_player_info=(player_info*)malloc(sizeof(player_info));
    if(new_player_info == NULL){
      printf("Unable to allocate memory for the new player.");
      exit(-1);
    }
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
    //Create a new workThread, which will recieve the players messages
    pthread_create(&thread_id,NULL,playerThread,(void*)new_player_info);
    pthread_detach(thread_id);
  }
  return (NULL);
}

/** MEMORY and EXIT HANDLING FUNCTIONS ****************************************/

/**
 * Name:               destroyBoard
 * Purpose:            Free memory allocated for the game_board, and destroy
 *                     mutexes related to it.
 * Inputs:
 *   Global:
 *             (board) game_board - strucuture with the game board arrays and mutexes.
 * Outputs:            None
 */
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

/**
 * Name:               destroyPlayer
 * Purpose:            Mark player for removal, begin destroying/free memory
 *                     allocated/semaphores and mutexes related to the player
 * Inputs:
 *   Global:
 *             (void) * _player - pointer to struture related to the player.
 * Output:             None
 */
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

/**
 * Name:               destroyFruit
 * Purpose:            Mark fruit for removal.
 * Inputs:
 *   Global:
 *             (void) * _fruit - pointer to struture related to the fruit.
 * Outputs:            None
 */
void destroyFruit(void* _fruit){
  fruit_info* fruit= (fruit_info*)_fruit;
  fruit->exit=1;
  sem_post(&fruit->sem_fruit);
}

void free_game_memory_exit(){
  close_board_windows();
  shutdown(server_socket,SHUT_RDWR);
  pthread_join(server.thread_id,NULL);
  close(server_socket);
  sem_post(&score.sem_score);
  pthread_join(score.thread_id,NULL);
  if(players!=NULL)
    destroy(players,destroyPlayer);
  if(fruits!=NULL)
    destroy(fruits,destroyFruit);
  destroyBoard();
  printf("Memory freed and sockets closed!\nEND\n");
  exit(0);
}

/**
 * MAIN
 * Purpose:            Setting up server communication and serverThread and
 *                     handling SDL events
 * Input:
 *         (.txt file) board
 */
int main(int argc , char* argv[]){
  srand(time(NULL));
  SDL_Event event;
  int done = 0;

  Event_ShowFigure = SDL_RegisterEvents(1);

  //do not abort program for broken pipe
  signal(SIGPIPE, SIG_IGN);

  // Set up server for receiving and sending messages from/to clients
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

  // Loading up board map from a file in the same directory as the server
  load_board("board.txt");
  create_board_window(game_board.size_x,game_board.size_y);

  //game_board mutexes aren't used because only main thread is running at this point
  for(int j=0;j<game_board.size_y;j++){
    for(int i=0;i<game_board.size_x;i++){
        if(game_board.array[j][i].figure_type==BRICK)
          paint_brick(i,j);
    }
  }

  pthread_create(&server.thread_id, NULL, serverThread, NULL);

  // Auxiliary variables used to draw figures in the window
  int x,y,figure_type,color;
  // While the SDL window hasn't been closed
  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }
      if(event.type == Event_ShowFigure){
        // Message pushed by a valid move from a client
        server_message * data_ptr;
        data_ptr = event.user.data1;
        figure_type = data_ptr->type;
        x=data_ptr->x;
        y=data_ptr->y;
        color=data_ptr->c;
        //Color conversion
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
