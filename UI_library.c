#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

int screen_width;
int screen_height;
int n_rows;
int n_cols;
	int row_height;
		int col_width;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture *display;
SDL_Surface *screen;
SDL_Texture* monster;
SDL_Texture* pacman;
SDL_Texture* powerpacman;
SDL_Texture* lemon;
SDL_Texture* brick;
SDL_Texture* cherry;

int create_board_window(int dim_x, int dim_y){
	col_width = 25;
	n_cols = dim_x;
	screen_width = dim_x *col_width +1;

	row_height = 25;
	n_rows = dim_y;
	screen_height = dim_y *row_height+1;


	int i, x, y;
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		exit(-1);
	}
	window = SDL_CreateWindow("PacMan",
	SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
	screen_width,screen_height, 0);
	if (window == NULL){
		printf( "Window could not created! SDL_Error: %s\n", SDL_GetError() );
		exit(-1);
	}
	renderer = SDL_CreateRenderer(window, -1,
	/*SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE*/ 0);
	if (renderer == NULL) {
		printf( "Rendered could not created! SDL_Error: %s\n", SDL_GetError() );
		exit(-1);
	}


	 //Initialize PNG loading
   int imgFlags = IMG_INIT_PNG;
   if( !( IMG_Init( imgFlags ) & imgFlags ) ){
       printf( "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError() );
       exit(-1);
   } else {
       //Get window surface
       screen = SDL_GetWindowSurface( window );
   }

//load monster
 	SDL_Surface* loadedSurface = IMG_Load("./monster.png");
   if( loadedSurface == NULL ){
       printf( "Unable to load image %s! SDL_image Error: %s\n", "./monster.png", IMG_GetError() );
 			exit(-1);
   } else {
			 monster = SDL_CreateTextureFromSurface(renderer, loadedSurface);
       SDL_FreeSurface( loadedSurface );
   }

	 //load pacman
	 loadedSurface = IMG_Load("./pacman.png");
 	if( loadedSurface == NULL ){
 			printf( "Unable to load image %s! SDL_image Error: %s\n", "./pacman.png", IMG_GetError() );
 		 exit(-1);
 	} else {
 			pacman = SDL_CreateTextureFromSurface(renderer, loadedSurface);
 			SDL_FreeSurface( loadedSurface );
 	}
	//load powerpacman
	loadedSurface = IMG_Load("./powerpacman.png");
 if( loadedSurface == NULL ){
		 printf( "Unable to load image %s! SDL_image Error: %s\n", "./powerpacman.png", IMG_GetError() );
		exit(-1);
 } else {
		 powerpacman = SDL_CreateTextureFromSurface(renderer, loadedSurface);
		 SDL_FreeSurface( loadedSurface );
 }

	//load lemon
	loadedSurface = IMG_Load("./lemon.png");
 if( loadedSurface == NULL ){
		 printf( "Unable to load image %s! SDL_image Error: %s\n", "./lemon.png", IMG_GetError() );
		exit(-1);
 } else {
		 lemon = SDL_CreateTextureFromSurface(renderer, loadedSurface);
		 SDL_FreeSurface( loadedSurface );
 }

 //load brick
 loadedSurface = IMG_Load("./brick.png");
if( loadedSurface == NULL ){
		printf( "Unable to load image %s! SDL_image Error: %s\n", "./brick.png", IMG_GetError() );
	 exit(-1);
} else {
		brick = SDL_CreateTextureFromSurface(renderer, loadedSurface);
		SDL_FreeSurface( loadedSurface );
}

//load brick
loadedSurface = IMG_Load("./cherry.png");
if( loadedSurface == NULL ){
	 printf( "Unable to load image %s! SDL_image Error: %s\n", "./cherry.png", IMG_GetError() );
	exit(-1);
} else {
	 cherry = SDL_CreateTextureFromSurface(renderer, loadedSurface);
	 SDL_FreeSurface( loadedSurface );
}


	 SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);

	 /* Create texture for display */
	 display = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width,screen_height);

	 SDL_SetRenderTarget(renderer, display);

	 SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	 SDL_RenderClear(renderer);


	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderCopy(renderer, display, NULL, NULL);
	SDL_RenderPresent(renderer);



	 SDL_SetRenderTarget(renderer, display);

	 SDL_SetRenderDrawColor(renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
	 for (i = 0; i <n_rows+1; i++){
		 SDL_RenderDrawLine(renderer, 0, i*row_height, screen_width, i*row_height);
	 }

	 for (i = 0; i <n_cols+1; i++){
		 SDL_RenderDrawLine(renderer, i*col_width, 0, i*col_width, screen_height);
	 }
	 SDL_SetRenderTarget(renderer, NULL);
	 SDL_RenderCopy(renderer, display, NULL, NULL);
	 SDL_RenderPresent(renderer);



	 return 0;

 }


void close_board_windows(){
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	if (window) {
		SDL_DestroyWindow(window);
	}
}




void priv_paint_place(int  board_x, int board_y , int r, int g, int b, SDL_Texture* pic){
	SDL_Rect rect;


	rect.x = board_x * col_width +1;
	rect.y = board_y * row_height+1;
	rect.w = col_width -1;
	rect.h = row_height -1;


	SDL_SetRenderTarget(renderer, display);
	SDL_RenderSetClipRect(renderer, &rect);


	SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(renderer, &rect);
	//SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	//SDL_RenderDrawRect(renderer, &rect);
	if(pic!=NULL){
		SDL_RenderCopy(renderer, pic, NULL, &rect);
	}
	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderCopy(renderer, display, NULL, NULL);
	SDL_RenderPresent(renderer);


}
void paint_pacman(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , r, g, b, pacman);
}
void paint_powerpacman(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , r, g, b, powerpacman);
}
void paint_monster(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , r, g, b, monster);
}
void paint_place(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , r, g, b, monster);
}
void paint_lemon(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , 255, 255, 255, lemon);
}
void paint_cherry(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , 255, 255, 255, cherry);
}

void paint_brick(int  board_x, int board_y , int r, int g, int b){
	priv_paint_place(board_x, board_y , 25, 255, 255, brick);
}
void clear_place(int  board_x, int board_y){
	priv_paint_place(board_x, board_y , 255, 255, 255, NULL);

}
void get_board_place(int mouse_x, int mouse_y, int * board_x, int *board_y){
	*board_x = mouse_x / col_width;
	*board_y = mouse_y / row_height;
}
