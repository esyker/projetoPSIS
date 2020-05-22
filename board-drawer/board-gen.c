#include <SDL2/SDL.h>
#include <pthread.h>
//gcc teste.c UI_library.c -o teste-UI -lpthread -lSDL2 -lSDL2_image

#include "UI_library.h"



int main(int argc, char * argv[]){

	SDL_Event event;
	int done = 0;

	int n_lines, n_cols;
	if (argc <3){
		printf("usage: board_drawer n_cols n_lines\n");
		exit(-1);
	}
	if(sscanf(argv[1], "%d", &n_cols) == 0){
		printf("usage: board_drawer n_cols n_lines\nn_cols should be an integer\n");
		exit(-1);
	}
	if(sscanf(argv[2], "%d", &n_lines) == 0){
		printf("usage: board_drawer n_cols n_lines\nn_lines should be an integer\n");
		exit(-1);
	}

	//creates a windows with 50x20 cases
	create_board_window( n_cols, n_lines);

	// create a board_drawer

	char ** board;
	int i, j;
	board = malloc(sizeof(char *) * n_lines);
	for ( i = 0 ; i < n_lines; i++){
		board[i] = malloc (sizeof(char) * (n_cols+1));
		for (j = 0; j < n_cols; j++){
			board[i][j] = ' ';
		}
		board[i][j] = '\0';

	}

	while (!done){
		while (SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT){
				done = SDL_TRUE;
			}
			// if the event is of type Event_ShowLemon
			if(event.type == SDL_MOUSEBUTTONDOWN){
				int board_x, board_y;
				int window_x, window_y;
				window_x = event.button.x;
				window_y = event.button.y;
				printf("clicked on pixel x-%d y-%d\n", window_x, window_y);
				get_board_place(window_x, window_y, & board_x, &board_y);
				printf("clicked on board x-%d y-%d\n", board_x, board_y);

				if (event.button.button == SDL_BUTTON_LEFT){
					board[board_y][board_x]= 'B';
					printf("left x-%d y-%d\n", board_x, board_y);
					paint_brick(board_x, board_y);
				}
				if (event.button.button == SDL_BUTTON_RIGHT){
					board[board_y][board_x]= ' ';
					printf("right x-%d y-%d\n", board_x, board_y);
					clear_place(board_x, board_y);
				}

			}
		}
	}
	close_board_windows();
	printf("fim\n");
	printf("%d %d\n", n_cols, n_lines);
	for ( i = 0 ; i < n_lines; i++){
		printf("%2d %s\n", i, board[i]);
	}
	FILE * fp;
	fp = fopen("board.txt", "w");
	fprintf(fp, "%d %d\n", n_cols, n_lines);
	for ( i = 0 ; i < n_lines; i++){
		fprintf(fp, "%s\n", board[i]);
	}
	fclose(fp);
}
