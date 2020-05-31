/**** BOARD FUNCTIONS *********************************************************/
// THIS MODULE IS RELATED TO LOADING, UPDATING, DESTROYING AND LOGIC OF THE BOARD

#include "board_operations.h"

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
