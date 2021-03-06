/** PLAYER RELATED FUNCTIONS **************************************************/
#include "player_operations.h"
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
  if(event_data1 == NULL){
    printf("Unable to allocate memory in player_disconect.");
    exit(-1);
  }
  event_data2=(server_message*)malloc(sizeof(server_message));
  if(event_data2 == NULL){
    printf("Unable to allocate memory in player_disconect.");
    exit(-1);
  }
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
      pthread_mutex_lock(&player->mutex);
      if((player->exit)==1){//player exits
        pthread_mutex_unlock(&player->mutex);
        break;
      }
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
      pthread_mutex_lock(&player->mutex);
      if((player->exit)==1){//player exits
        pthread_mutex_unlock(&player->mutex);
        break;
      }
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
      pthread_mutex_lock(&(player->mutex));
      if(errno==ETIMEDOUT){
        SDL_zero(new_event);
        new_event.type = Event_ShowFigure;
        //lock_player_mutex(player,0);
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
        timeout.tv_sec=time(NULL)+INACTIVITY_T;
        pthread_mutex_unlock(&(player->mutex));
        send_to_players(&msg2);
      }
    }
    else{
      if((player->exit)==1){//player exits
        pthread_mutex_unlock(&(player->mutex));
        break;
      }
      timeout.tv_sec=time(NULL)+INACTIVITY_T;
      pthread_mutex_unlock(&(player->mutex));
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
    if(my_color<0||my_color>360){//invalid color
      close(client_sock_fd);
      return NULL;
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
  free(player);
  return NULL;
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
  pthread_mutex_lock(&player->mutex);
  player->exit=1;
  pthread_cancel(player->playerThread_id);
  pthread_mutex_unlock(&player->mutex);
  sem_post(&(player->sem_inact));
  sem_post(&(player->sem_monster_eaten));
  sem_post(&(player->sem_pacman_eaten));
  pthread_barrier_wait(&player->barrier);
  pthread_mutex_destroy(&(player->mutex));
  close(player->client_fd);
  free(player);
}
