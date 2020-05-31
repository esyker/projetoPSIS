#ifndef SCORE_OPERATIONS_H
#define SCORE_OPERATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "server.h"
#include "linked_list.h"
#include "list_operations.h"

#define SCORE_T 10

extern LinkedList *players;
extern score_info score;

void get_score_send(void* player_inf, void* message);
void* scoreThread(void* argv);

#endif /* SCORE_OPERATIONS_H */
