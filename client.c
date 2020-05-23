#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "client.h"
#include "UI_library.h"

//gcc ex.c UI_library.c -o ex -lSDL2 -lSDL2_image -lpthread


Uint32 Event_ShowFigure;
int sock_fd;
int server_socket;
int currSize=10;
int color; //add a hsv to rgb converter
int size_x = 0;
int size_y = 0;
player_info player;

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
//CLIENT THREAD
void *clientThread(void *arg){
  server_message msg;
  int err_rcv;
  server_message *event_data;
  SDL_Event new_event;

  //printf("just connected to the server\n");
  while((err_rcv = recv(sock_fd, &msg , sizeof(msg), 0)) >0 ){
    //printf("received %d byte %d %d %d \n", err_rcv, msg.type, msg.x, msg.y);

    if (msg.type ==INIT){//first message, board initialization
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


    event_data = (server_message*)malloc(sizeof(server_message));
    *event_data = msg;
    SDL_zero(new_event);
    new_event.type = Event_ShowFigure;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);
  }
  return (NULL);
}

int main(int argc , char* argv[]){

  SDL_Event event;
  int done = 0;
  pthread_t thread_id;

  Event_ShowFigure = SDL_RegisterEvents(1);

  //invalid arguments
  if(argc != 4){
    printf("invalid arguments\n");
    exit(-1);
  }

  // CLIENT
  struct sockaddr_in server_addr;

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

  //printf("connecting to %s %d\n", argv[1], server_addr.sin_port );

  if( -1 == connect(sock_fd,
                    (const struct sockaddr *) &server_addr, sizeof(server_addr))){
    printf("Error connecting\n");
    exit(-1);
  }

  if(sscanf(argv[3], "%d", &color)!=1){
    printf("argv[3] is not a valid color\n");
    exit(-1);
  }

  pthread_create(&thread_id, NULL, clientThread, NULL);

  while(size_x == 0 && size_y == 0){
    sleep(1);
  }

  send(sock_fd, &color, sizeof(int), 0);//send color of monsters

  //creates a windows and a board with 50x20 cases
  create_board_window(size_x, size_y);

  //monster and packman position
  int x;
  int y;
  int figure_type;

  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }

      /*RECEIVE FROM SERVER*/
      if(event.type == Event_ShowFigure){
        server_message * data_ptr;
        data_ptr = event.user.data1;
        figure_type=data_ptr->type;
        x = data_ptr->x;
        y = data_ptr->y;
        color = data_ptr->c;

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
        //printf("new event received\n");
      }
      /* SEND TO SERVER*/
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
      //when the mouse mooves the monster also moves
      if(event.type == SDL_MOUSEMOTION){
        int x_new, y_new;

        //this fucntion return the place cwher the mouse cursor is
        get_board_place(event.motion.x, event.motion.y,
                        &x_new, &y_new);

        pthread_mutex_lock(&player.monster_lock);
        //if the mouse moved to another place
        //if(((x_new == (player.x_monster+1) || x_new == (player.x_monster-1)) && (y_new == player.y_monster))
        //  || ((y_new == (player.y_monster+1) || y_new == (player.y_monster-1)) && (x_new == player.x_monster))){

          client_message msg;
          //if(!is_server)
          msg.figure_type = MONSTER;
          msg.x = x_new;
          msg.y = y_new;
          //msg.c = color;?????????

          send(sock_fd, &msg, sizeof(msg), 0);
        //}
        pthread_mutex_unlock(&player.monster_lock);
      }
      //printf("monster_pos: (%d,%d)\n", player.x_monster,player.y_monster);
      //printf("pacman_pos: (%d,%d)\n", player.x_pacman,player.y_pacman);
    }
  }

  //printf("fim\n");
  close_board_windows();
  exit(0);
}
