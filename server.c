#include "server.h"
#include "UI_library.h"
#include "linked_list.h"

#include "list_operations.h"
#include "board_operations.h"
#include "score_operations.h"
#include "fruit_operations.h"
#include "player_operations.h"

/** GLOBAL VARIABLES **/
Uint32 Event_ShowFigure; // Variable used by funtions from the SDL library to push a visual event
int server_socket;       // Server file descriptor
LinkedList *players;     // List of players connected to the server
LinkedList *fruits;      // List of fruits (related to the number of players)
board game_board;        // Structure with the information related to the game board and logic
score_info score;        // Structure that contains the score, updated as the games goes on
server_info server;      // Structure with the thread_id of the server Thread

void free_game_memory_exit(){
  close_board_windows();
  shutdown(server_socket,SHUT_RDWR);
  pthread_join(server.thread_id,NULL);
  destroyBoard();
  printf("Memory freed and sockets closed!\nEND\n");
  exit(0);
}


/**
 * Name:               serverThread
 * Purpose:            Thread which initializes the connections for the game to
 *                     start, initializes the requires structures for the game
 *                     and starts threads for each player that connects to the
 *                     server, to read each clients/player messages.
 * Inputs:
 *   Param:
 *             (void) * argv - pointer to the player which messages this thread will recive //WRONG
 * Outputs:
 *   Param:
 *       (LinkedList) * players - affected/updated list
 *   (server_message) msg - messages to send to player
 * Note:
 */
void *serverThread(void * argv){
  // Auxiliary variables
  server_message msg;
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);

  players=constructList();
  fruits=constructList();

  int new_client_fd=0;
  player_info* new_player_info;

  sem_init(&(score.sem_score),0,0);
  pthread_create(&score.thread_id,NULL,scoreThread,NULL);

  while(1){
    //Server waits for a new client to connect
    new_client_fd = accept(server_socket,(struct sockaddr *)&client_addr,&size_addr);
    if(new_client_fd==-1) break;

    pthread_mutex_lock(&game_board.numb_players_mutex);
    if((game_board.size_x*game_board.size_y-game_board.numb_bricks
      - (game_board.numb_players-1)*2 - (game_board.numb_players*2))<4){
      // No more avalible spaces
      msg.type=BOARD_FULL;
      send(new_client_fd,&msg, sizeof(server_message), 0);
      close(new_client_fd);
      pthread_mutex_unlock(&game_board.numb_players_mutex);
      continue;
    }
    game_board.numb_players++;
    pthread_mutex_unlock(&game_board.numb_players_mutex);
    new_player_info=(player_info*)malloc(sizeof(player_info));
    if(new_player_info == NULL){
      printf("Unable to allocate memory for the new player.");
      exit(-1);
    }
    new_player_info->client_fd=new_client_fd;
    new_player_info->exit=0;
    new_player_info->score=0;
    if (pthread_mutex_init(&(new_player_info->mutex), NULL) != 0) {
      printf("\n player mutex init has failed\n");
      exit(-1);
    }
    if(pthread_barrier_init(&new_player_info->barrier,NULL,4)!=0){
      printf("\n player barrier init has failed\n");
      exit(-1);
    }
    add(players,(void*)new_player_info);
    //Create a new workThread, which will recieve the players messages
    pthread_create(&new_player_info->playerThread_id,NULL,playerThread,(void*)new_player_info);
    pthread_detach(new_player_info->playerThread_id);
  }
  sem_post(&score.sem_score);
  pthread_join(score.thread_id,NULL);
  if(fruits!=NULL)
    destroy(fruits,destroyFruit);
  if(players!=NULL)
    destroy(players,destroyPlayer);
  close(server_socket);
  return (NULL);
}

/**
 * MAIN
 * Purpose:            Setting up server communication and serverThread and
 *                     handling SDL events
 * Input:
 *         (.txt file) board
 */
int main(int argc , char* argv[]){
  srand(time(NULL));
  SDL_Event event;
  int done = 0;

  Event_ShowFigure = SDL_RegisterEvents(1);

  //do not abort program for broken pipe
  signal(SIGPIPE, SIG_IGN);

  // Set up server for receiving and sending messages from/to clients
  struct sockaddr_in server_local_addr;
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1){
    perror("socket: ");
    exit(-1);
  }
  server_local_addr.sin_family = AF_INET;
  server_local_addr.sin_addr.s_addr = INADDR_ANY;
  server_local_addr.sin_port = htons(3000); //PORT -> 3000
  int err = bind(server_socket, (struct sockaddr *)&server_local_addr,
     sizeof(server_local_addr));
  if(err == -1) {
    perror("bind");
    exit(-1);
  }
  if(listen(server_socket, 5) == -1) {
    perror("listen");
    exit(-1);
  }

  // Loading up board map from a file in the same directory as the server
  load_board("board.txt");
  create_board_window(game_board.size_x,game_board.size_y);

  //game_board mutexes aren't used because only main thread is running at this point
  for(int j=0;j<game_board.size_y;j++){
    for(int i=0;i<game_board.size_x;i++){
        if(game_board.array[j][i].figure_type==BRICK)
          paint_brick(i,j);
    }
  }

  pthread_create(&server.thread_id, NULL, serverThread, NULL);

  // Auxiliary variables used to draw figures in the window
  int x,y,figure_type,color;
  // While the SDL window hasn't been closed
  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }
      if(event.type == Event_ShowFigure){
        // Message pushed by a valid move from a client
        server_message * data_ptr;
        data_ptr = event.user.data1;
        figure_type = data_ptr->type;
        x=data_ptr->x;
        y=data_ptr->y;
        color=data_ptr->c;
        //Color conversion
        int r,g,b;
        rgb_360(color, &r, &g, &b);

        if(figure_type==EMPTY){
          clear_place(x,y);
        }
        else if(figure_type==MONSTER){
          paint_monster(x, y, r, g, b);
        }
        else if(figure_type==PACMAN){
          paint_pacman(x, y, r, g, b);
        }
        else if(figure_type==POWER_PACMAN){
          paint_powerpacman(x,y, r, g, b);
        }
        else if(figure_type==BRICK){
          paint_brick(x,y);//orange
        }
        else if(figure_type==CHERRY){
            paint_cherry(x,y);//red
        }
        else if(figure_type==LEMON){
            paint_lemon(x,y);//yellow
        }
        free(data_ptr);
      }
    }
  }

  free_game_memory_exit();
}
