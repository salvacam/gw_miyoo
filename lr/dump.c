#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
  
int main(int argc, char* args[])
{
  if(SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("%s, failed to SDL_Init\n", __func__);
    return -1;
  }
  
  SDL_Surface* screen;
  screen = SDL_SetVideoMode(320, 240, 16, SDL_HWSURFACE);
  if(screen == NULL){
    printf("%s, failed to SDL_SetVideMode\n", __func__);
    return -1;
  }
  
  SDL_Surface* png = IMG_Load(args[1]);
  if(png == NULL){
    printf("%s, failed to load image\n", __func__);
    return -1;
  }
  
  SDL_ShowCursor(0);
  SDL_BlitSurface(png, NULL, screen, NULL);
  SDL_Flip(screen);
  SDL_Delay(3000);

	int x, y, z;
	uint16_t *s = screen->pixels;
  printf("uint16_t buf[] = {\n");
	for(y=0; y<240; y++){
    for(x=0; x<20; x++){
      printf("  ");
      for(z=0; z<16; z++){
        printf("0x%x, ", *s++);
      }
      printf("\n");
	  }
  }
  printf("};\n");

  SDL_FreeSurface(png);
  SDL_Quit();
  return 0;    
}

