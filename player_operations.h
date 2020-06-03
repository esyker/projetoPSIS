#ifndef PLAYER_OPERATIONS_H
#define PLAYER_OPERATIONS_H

#define INACTIVITY_T 30

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <SDL2/SDL.h>

#include "linked_list.h"
#include "list_operations.h"
#include "fruit_operations.h"
#include "board_operations.h"
#include "score_operations.h"

extern board game_board;
extern Uint32 Event_ShowFigure;
extern LinkedList *fruits;
extern score_info score;
extern int server_socket;

void player_disconect(player_info* player);
void * monsterEaten(void * argv);
void * pacmanEaten(void * argv);
void * playerInactivity(void * argv);
void * playerThread(void * argv);
void * serverThread(void * argv);

void destroyPlayer(void* _player);

#endif /* PLAYER_OPERATIONS_H */
