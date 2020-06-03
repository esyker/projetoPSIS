/** FRUIT RELATED FUNCTIONS ***************************************************/

#include "fruit_operations.h"

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




void destroyFruit(void* _fruit){
  fruit_info* fruit=(fruit_info*)_fruit;
  pthread_mutex_lock(&fruit->mutex);
  pthread_cancel(fruit->thread_id);
  pthread_mutex_unlock(&fruit->mutex);
  pthread_mutex_destroy(&fruit->mutex);
  sem_destroy(&fruit->sem_fruit);
  free(fruit);
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
 *   Global:
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
        pthread_mutex_lock(&fruit->mutex);
        sem_destroy(&fruit->sem_fruit);
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
      pthread_mutex_lock(&fruit->mutex);

      if(!firstime) sleep(FRUIT_T);
      else firstime=0;

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
    if(fruit1 == NULL){
      printf("Unable to allocate memory for fruit1.");
      exit(-1);
    }
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
    if(fruit2 == NULL){
      printf("Unable to allocate memory for fruit2.");
      exit(-1);
    }
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
