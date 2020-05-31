/**** LIST OPERATIONS *********************************************************/
// THIS MODULE CONTAINS AUXILIARY FUNCTIONS TO OVERRIDE LIST OPERATIONS THAT
// ARE SPECIFIC TO THIS PROJECT,  SUCH AS TRASVERSE OR NODE REMOVAL

#include "list_operations.h"

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
