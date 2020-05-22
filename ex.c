#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "ex.h"
#include "UI_library.h"

//gcc ex.c UI_library.c -o ex -lSDL2 -lSDL2_image -lpthread


Uint32 Event_ShowCharacter;
int sock_fd;
int server_socket;
player_info* players;
int currSize=10;
int color; //add a hsv to rgb converter
int size_x = 0;
int size_y = 0;


//CLIENT THREAD
void *clientThread(void *arg){
  exe_message msg;
  int err_rcv;
  exe_message *event_data;
  SDL_Event new_event;
  printf("just connected to the server\n");

  while((err_rcv = recv(sock_fd, &msg , sizeof(msg), 0)) >0 ){
    printf("received %d byte %d %d %d %d\n", err_rcv, msg.c ,msg.character, msg.x, msg.y);

    if (msg.c ==-1){
      size_x = msg.x;
      size_y = msg.y;
      continue;
    }

    event_data = malloc(sizeof(exe_message));
    *event_data = msg;
    SDL_zero(new_event);
    new_event.type = Event_ShowCharacter;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);
  }
  return (NULL);
}

//SERVER THREADs
void * playerThread(void * argv){
  //player_info client =*((player_info*)argv);
  int client_sock_fd=*((int*)argv);
  int err_rcv;

  exe_message msg;
  exe_message *event_data;
  SDL_Event new_event;

  //establish board size
  msg.c = -1;
  msg.x = size_x;
  msg.y = size_y;

  send(client_sock_fd, &msg, sizeof(msg), 0);

  while((err_rcv = recv(client_sock_fd, &msg , sizeof(msg), 0)) > 0 ){
    printf("received %d byte %d %d %d %d\n", err_rcv, msg.c ,msg.character, msg.x, msg.y);

    for(int i=0;i<currSize;i++){
      if(players[i].client_fd!=client_sock_fd)
        send(players[i].client_fd, &msg, sizeof(msg), 0);
    }

    event_data = malloc(sizeof(exe_message));
    *event_data = msg;
    SDL_zero(new_event);
    new_event.type = Event_ShowCharacter;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);
  }
}

void *serverThread(void * argc){
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);

  int i=0;
  players=(player_info*)malloc(currSize*sizeof(int));
  pthread_t thread_id;

  for(i=0;;i++){
   if(i==currSize){
     currSize*=2;
     players=(player_info*)realloc(players,currSize*sizeof(int));
   }

   //Server waits for a new client to connect
   players[i].client_fd = accept(server_socket,(struct sockaddr *)&client_addr,&size_addr);
   if(players[i].client_fd == -1){
     perror("accept");
     exit(EXIT_FAILURE);
   }

   //Create a new workThread
   pthread_create(&thread_id,NULL,playerThread,&players[i].client_fd);
   pthread_detach(thread_id);
  }

  return (NULL);
}

int main(int argc , char* argv[]){

  int is_server;
  SDL_Event event;
  int done = 0;
  pthread_t thread_id;

  Event_ShowCharacter = SDL_RegisterEvents(1);

  //SERVER
  if (argc == 3){
    struct sockaddr_in server_local_addr;

    is_server = 1;

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

    if(sscanf(argv[1], "%d", &size_x)!=1){
      printf("argv[1] is not a valid size\n");
      exit(-1);
    }
    if(sscanf(argv[2], "%d", &size_y)!=1){
      printf("argv[2] is not a valid size\n");
      exit(-1);
    }
    // can we do the accept here?
    // no the accept should be on the thread
    pthread_create(&thread_id, NULL, serverThread, NULL);
  }

  // CLIENT
  if(argc == 4){
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

    printf("connecting to %s %d\n", argv[1], server_addr.sin_port );

    if( -1 == connect(sock_fd,
                      (const struct sockaddr *) &server_addr, sizeof(server_addr))){
      printf("Error connecting\n");
      exit(-1);
    }

    if(sscanf(argv[3], "%d", &color)!=1){
      printf("argv[3] is not a valid color\n");
      exit(-1);
    }

    is_server = 0;
    pthread_create(&thread_id, NULL, clientThread, NULL);

    while(size_x == 0 && size_y == 0){
      printf("sleep\n");
      usleep(20000);
    }
  }
  //invalid arguments
  if((argc!= 3) && (argc != 4)){
    printf("invalid arguments\n");
    exit(-1);
  }

  //creates a windows and a board with 50x20 cases
  create_board_window(size_x, size_y);

  //monster and packman position
  int x = 0;
  int y = 0;

  int x_other = 0;
  int y_other = 0;
  int character=0;

  while (!done){
    while (SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        done = SDL_TRUE;
      }
      if(event.type == Event_ShowCharacter){
        exe_message * data_ptr;
        data_ptr = event.user.data1;

        clear_place(x_other, y_other);
        x_other = data_ptr->x;
        y_other = data_ptr->y;
        character = data_ptr->character;

        if(character==3){
          paint_monster(x_other, y_other , 7, 200, 100);
        }else if(character==2){
          paint_pacman(x_other, y_other , 7, 100, 200);
        }
        printf("new event received\n");
      }

      //when the mouse mooves the monster also moves
      if(event.type == SDL_MOUSEMOTION){
        int x_new, y_new;

        //this fucntion return the place cwher the mouse cursor is
        get_board_place(event.motion.x, event.motion.y,
                        &x_new, &y_new);
        //if the mouse moved to another place
        if((x_new != x) || (y_new != y)){
          //the old place is cleared
          clear_place(x, y);
          x = x_new;
          y = y_new;
          //decide what color to paint the monster
          //and paint it
          exe_message msg;
          if(is_server){
            paint_pacman(x, y , 200, 100, 7);
            msg.character = 2;
          }else{
            paint_monster(x, y , 7, 200, 100);
            msg.character = 3;
          }
          printf("move x-%d y-%d c-%d\n", x,y,color);
          msg.x = x;
          msg.y = y;
          msg.c = color;

          if(is_server){
            for(int i=0;i<currSize;i++){
              send(players[i].client_fd, &msg, sizeof(msg), 0);
            }
          } else {
            send(sock_fd, &msg, sizeof(msg), 0);
          }

        }
      }
    }
  }

  printf("fim\n");
  close_board_windows();
  exit(0);
}
