/** SCORE FUNCTIONS ***********************************************************/
#include "score_operations.h"
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
