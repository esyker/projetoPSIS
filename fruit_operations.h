#ifndef FRUIT_OPERATIONS_H
#define FRUIT_OPERATIONS_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "server.h"
#include "linked_list.h"
#include "list_operations.h"

extern board game_board;
extern Uint32 Event_ShowFigure;

void removeFruitData(void* _fruit);
void fruit_score_player_disconnect(LinkedList* players, LinkedList* fruits);
void * fruitGenerator(void * argv);
void fruit_new_player(LinkedList* players, LinkedList* fruits);

#endif /* FRUIT_OPERATIONS_H */
