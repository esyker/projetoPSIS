#ifndef BOARD_OPERATIONS_H
#define BOARD_OPERATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "server.h"
#include "linked_list.h"

extern board game_board;
extern Uint32 Event_ShowFigure;

void load_board(char * file_name);
server_message assignRandCoords(player_info* player, int figure_type, int init);
server_message board_to_message(int x, int y);
server_message change_board(int x, int y, int figure_type, player_info* player, fruit_info* fruit, int color, int id);
server_message* validate_play_get_answer(client_message input, player_info* player);

void destroyBoard();

#endif /* LIST_OPERATIONS_H */
