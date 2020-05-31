#ifndef LIST_OPERATIONS_H
#define LIST_OPERATIONS_H

#include <sys/socket.h>

#include "server.h"
#include "linked_list.h"

extern LinkedList *players;

void send_to_player(void* data , void *msg);
void send_to_player_2_messages(void* data , void* msg);
void send_to_players(void *msg);
void send_to_players_no_lock(void *msg);
void send_to_players_2_messages(void *msg);

#endif /* LIST_OPERATIONS_H */
