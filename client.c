#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "client.h"
#include "UI_library.h"

/* REMOVER */
//gcc ex.c UI_library.c -o ex -lSDL2 -lSDL2_image -lpthread

/** GLOBAL VARIABLES **/
Uint32 Event_ShowFigure; // Variable used by funtions from the SDL library to push a visual event
int sock_fd;             // File descriptor, used to communicate with the server
int color;               // Color of the player, Hue value in HSV color model
int size_x = 0;          // Board size along the x axis
int size_y = 0;          // Board size along the y axis
player_info player;      // Strucure with the client/player information

/**
 * Name:               init_player
 * Purpose:            Initilize client/player mutexes, in order to ensure
 *                     syncronization on acess to the player_info structure.
 * Inputs:
 *   Parameters:       None
 *   Globals:
 *       (player_info) player - acessed to intialize its mutexes
 * Outputs:
 *   Parameters:       None
 *   Globals:
 *       (player_info) player - initilized mutexes
 *   Return:           None
 */
void init_player(){
  if (pthread_mutex_init(&player.pacman_lock, NULL) != 0) {
    printf("\n mutex init has failed\n");
    exit(-1);
  }
  if (pthread_mutex_init(&player.monster_lock, NULL) != 0) {
    printf("\n mutex init has failed\n");
    exit(-1);
  }
  return;
}

/**
 * Name:               clientThread
 * Purpose:            Receive and parse/process messages from the server, and
 *                     pushes an event in order to display using the SDL library.
 *                     Depending on the type of the message from the server, the
 *                     function will process the message accordingly.
 * Inputs:
 *   Parameters:       None
 *   Globals:
 *       (player_info) player
 *               (int) sock_fd - file descriptor used to receive messages from the server
 *        (server_msg) msg - message from the server
 * Outputs:
 *   Parameters:
 *         (SDL_Event) * new_event - structure with the information (msg, type)
 *                                   to update the SDL window.
 *   Globals:
 *               (int) size_x, size_y - board size
 *       (player_info) player - general player information updates
 *   Return:           None
 * Notes:              When pushing the new_event, memory is allocated. This
 *                     memory will have to be deallocated/freed later.
 */
void *clientThread(void *arg){
  server_message msg;
  int err_rcv;
  server_message *event_data;
  SDL_Event new_event;

  while((err_rcv = recv(sock_fd, &msg , sizeof(msg), 0)) >0 ){
    if(msg.type == INIT){//first message, board initialization
      size_x = msg.x;
      size_y = msg.y;
      player.player_id=msg.player_id;
      continue;
    }
    else if(msg.type==MONSTER){
      if(msg.player_id==player.player_id){
        pthread_mutex_lock(&player.monster_lock);
        player.x_monster = msg.x;
        player.y_monster = msg.y;
        pthread_mutex_unlock(&player.monster_lock);
      }
    }
    else if(msg.type==PACMAN||msg.type==POWER_PACMAN){
      if(msg.player_id==player.player_id){
        pthread_mutex_lock(&player.pacman_lock);
        player.x_pacman = msg.x;
        player.y_pacman = msg.y;
        pthread_mutex_unlock(&player.pacman_lock);
      }
    }
    else if(msg.type==SCOREBOARD){
      printf("\n  SCOREBOARD\n");
    }
    else if(msg.type==SCORE_MSG){
      printf("PLAYER:%d SCORE:%d\n",msg.player_id,msg.score);
    }
    else if(msg.type==BOARD_FULL)
    {
      printf("\nBOARD IS FULL TRY AGAIN LATER!\n");
      close(sock_fd);
      exit(0);
    }

    event_data = (server_message*)malloc(sizeof(server_message));
    *event_data = msg;
    SDL_zero(new_event);
    new_event.type = Event_ShowFigure;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);
  }
  return (NULL);
}

/**
 * Name:               close_sockets_exit
 * Purpose:            Closes sockets, SDL window and exits.(self explanatory)
 * Inputs:
 *   Parameters:       None
 *   Globals:
 *               (int) sock_fd - file descriptor connected to the server.
 * Outputs:            None
 * Notes:              *
 */
void close_sockets_exit(){
  close(sock_fd);
  close_board_windows();
  printf("EXITED SUCCESSFULLY!\n");
  exit(0);
}

/**
 * MAIN
 * Input: argv[1] - server IP address
 *        argv[2] - server port number
 *        argv[3] - color chosen by client (Hue value in HSV color model)
 */
int main(int argc , char* argv[]){
  /* Auxiliary variables */ //remove?
  SDL_Event event;     // used to receive SDL events from the clientThread, keyboard,mouse and SDL window
  int done = 0;        // used to check if the program has been terminated by closing the SDL window
  pthread_t thread_id; // identifier for the clientThread, which will receive and process messages from the server
  struct sockaddr_in server_addr; // used to store the address and port of the server

  Event_ShowFigure = SDL_RegisterEvents(1);

  if(argc != 4){
    printf("invalid arguments\n");
    exit(-1);
  }

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1){
    perror("socket: ");
    exit(-1);
  }

  server_addr.sin_family = AF_INET;
  int port_number;
  if(sscanf(argv[2], "%d", &port_number)!=1){
    printf("argv[2] is not a number\n");
    exit(-1);
  }

  server_addr.sin_port= htons(port_number);
  if(inet_aton(argv[1], &server_addr.sin_addr) == 0){
    printf("argv[1]is not a valid address\n");
    exit(-1);
  }

  if(sscanf(argv[3], "%d", &color)!=1){
    printf("argv[3] is not a valid color\n");
    exit(-1);
  }
  if(color<0||color>360){
    printf("Enter a color within [0,360]\n");
    exit(-1);
  }

  // Connect to the server, using the parsed main arguments
  if( -1 == connect(sock_fd,
                    (const struct sockaddr *) &server_addr, sizeof(server_addr))){
    printf("Error connecting\n");
    exit(-1);
  }

  // Creation of the clientThread that will receive messages from the server,
  // since the recv function would have blocked the program if called here
  pthread_create(&thread_id, NULL, clientThread, NULL);

  // Wait for the Initilization (INIT) message clientThread
  while(size_x == 0 && size_y == 0){
    sleep(1);
  }

  // Sending the player color.
  send(sock_fd, &color, sizeof(int), 0);

  //creates SDL window/board
  create_board_window(size_x, size_y);

  // auxiliary variables: position and typeof figure to display in the board
  int x;
  int y;
  int figure_type;

  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }

      // Received event from the clientThread
      if(event.type == Event_ShowFigure){
        // Conversion of the event information to update the board window
        server_message * data_ptr;
        data_ptr = event.user.data1;
        figure_type=data_ptr->type;
        x = data_ptr->x;
        y = data_ptr->y;
        color = data_ptr->c;

        // Conversion of the color hue value to its corresponding RGB values
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
        else if(figure_type==LEMON){
          paint_lemon(x,y);//red
        }
        else if(figure_type==CHERRY){
          paint_cherry(x,y);//yellow
        }
        // Dealocation of the extra event data [ref. clientThread]
        free(data_ptr);
      }
      // Reciving input from the keyboard and create message with the proposed position.
      if(event.type == SDL_KEYDOWN){
        client_message msg;
        msg.figure_type = PACMAN;
        pthread_mutex_lock(&player.pacman_lock);
        if (event.key.keysym.sym == SDLK_LEFT ){
          msg.y = player.y_pacman;
          msg.x = player.x_pacman-1;
        }
        if (event.key.keysym.sym == SDLK_RIGHT ){
          msg.x=player.x_pacman+1;
          msg.y=player.y_pacman;
        }
        if (event.key.keysym.sym == SDLK_UP ){
          msg.x=player.x_pacman;
          msg.y=player.y_pacman-1;
        }
        if (event.key.keysym.sym == SDLK_DOWN ){
          msg.x=player.x_pacman;
          msg.y=player.y_pacman+1;
        }
        pthread_mutex_unlock(&player.pacman_lock);
        send(sock_fd, &msg, sizeof(msg), 0);
      }
      // Reciving input from the window/mouse and create message with the proposed position.
      if(event.type == SDL_MOUSEMOTION){
        int x_new, y_new;
        get_board_place(event.motion.x, event.motion.y, &x_new, &y_new);

        pthread_mutex_lock(&player.monster_lock);
        client_message msg;
        msg.figure_type = MONSTER;
        msg.x = x_new;
        msg.y = y_new;
        send(sock_fd, &msg, sizeof(msg), 0);
        pthread_mutex_unlock(&player.monster_lock);
      }
    }
  }

  close_sockets_exit();
}
