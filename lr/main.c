#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/time.h>
#include <SDL.h>

#include "hex_credits.h"

#define FPS           60
#define TICKS_PER_SEC 1000000UL
#define F             (TICKS_PER_SEC/FPS)
#define AUDIO_SIZE    4096

#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11
#define RETRO_DEVICE_ID_JOYPAD_L2      12
#define RETRO_DEVICE_ID_JOYPAD_R2      13
#define RETRO_DEVICE_ID_JOYPAD_L3      14
#define RETRO_DEVICE_ID_JOYPAD_R3      15

#define RETRO_ENVIRONMENT_EXPERIMENTAL                    0x10000
#define RETRO_ENVIRONMENT_PRIVATE                         0x20000
#define RETRO_ENVIRONMENT_SET_ROTATION                    1
#define RETRO_ENVIRONMENT_GET_OVERSCAN                    2
#define RETRO_ENVIRONMENT_GET_CAN_DUPE                    3
#define RETRO_ENVIRONMENT_SET_MESSAGE                     6
#define RETRO_ENVIRONMENT_SHUTDOWN                        7
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL           8
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY            9
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT                10
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS           11
#define RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK           12
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE      13
#define RETRO_ENVIRONMENT_SET_HW_RENDER                   14
#define RETRO_ENVIRONMENT_GET_VARIABLE                    15
#define RETRO_ENVIRONMENT_SET_VARIABLES                   16
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE             17
#define RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME             18
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH               19
#define RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK              22
#define RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK         21
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE            23
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES   24
#define RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE            (25 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE            (26 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE               27
#define RETRO_ENVIRONMENT_GET_PERF_INTERFACE              28
#define RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE          29
#define RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY           30
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY              31
#define RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO              32
#define RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK       33
#define RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO              34
#define RETRO_ENVIRONMENT_SET_CONTROLLER_INFO             35
#define RETRO_ENVIRONMENT_SET_MEMORY_MAPS                 (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_SET_GEOMETRY                    37
#define RETRO_ENVIRONMENT_GET_USERNAME                    38 
#define RETRO_ENVIRONMENT_GET_LANGUAGE                    39

enum retro_log_level {
  RETRO_LOG_DEBUG = 0, 
  RETRO_LOG_INFO,
  RETRO_LOG_WARN,
  RETRO_LOG_ERROR,
  RETRO_LOG_DUMMY = INT_MAX
};

typedef void (*retro_input_poll_t)(void);
typedef bool (*retro_environment_t)(unsigned cmd, void *data);
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *data, size_t frames);
typedef void (*retro_log_printf_t)(enum retro_log_level level, const char *fmt, ...);
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);
typedef void (*retro_video_refresh_t)(const void *data, unsigned width, unsigned height, size_t pitch);

struct retro_game_geometry {
  unsigned base_width;
  unsigned base_height;
  unsigned max_width;
  unsigned max_height;
  float aspect_ratio;
};

struct retro_system_info {
  const char *library_name;
  const char *library_version;
  const char *valid_extensions;
  bool need_fullpath;     
  bool block_extract;     
};

struct retro_game_info {
  const char *path;
  const void *data;
  size_t size;
  const char *meta;
};

struct retro_log_callback {
  retro_log_printf_t log; 
};

void (*f_run_core)();
void (*f_init_core)();
void (*f_unload_game)();
void (*f_deinit_core)();
void (*f_set_poll)(retro_input_poll_t);
void (*f_set_env)(retro_environment_t);
void (*f_set_input)(retro_input_state_t);
void (*f_get_info)(struct retro_system_info*);
void (*f_refresh_video)(retro_video_refresh_t);
bool (*f_load_game)(const struct retro_game_info*);
void (*f_sample_audio_batch)(retro_audio_sample_batch_t);

static int loop=1;
static int zoom=0;
static int ox=0, oy=0;
static int cw=0, ch=0;
static int key[16]={0};
static size_t audio_len=0;
static size_t audio_end=0;
static size_t audio_first=0;
static SDL_Joystick *joy=NULL;
static SDL_Surface* screen={0};
static struct timeval init_tv={0};
static size_t audio_size=AUDIO_SIZE+1;
static uint8_t audio_buf[AUDIO_SIZE+1]={0};
static struct retro_game_info r_game={0};
static struct retro_system_info r_info={0};

uint32_t get_ticks(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);
  if(init_tv.tv_sec == 0) {
    init_tv = tv;
  }
  return (tv.tv_sec - init_tv.tv_sec) * TICKS_PER_SEC + tv.tv_usec - init_tv.tv_usec;
}

static size_t fifo_read_avail(void)
{
  return (audio_end + ((audio_end < audio_first) ? audio_size : 0)) - audio_first;
}

static size_t fifo_write_avail(void)
{
  return (audio_size - 1) - ((audio_end + ((audio_end < audio_first) ? audio_size : 0)) - audio_first);
}

void fifo_write(const void *in_buf, size_t size)
{
  size_t rest_write = 0;
  size_t first_write = size;

  if(audio_end + size > audio_size){
    first_write = audio_size - audio_end;
    rest_write = size - first_write;
  }
  memcpy(audio_buf + audio_end, in_buf, first_write);
  memcpy(audio_buf, (const uint8_t*)in_buf + first_write, rest_write);
  audio_end = (audio_end + size) % audio_size;
}

void fifo_read(void *in_buf, size_t size)
{
  size_t rest_read = 0;
  size_t first_read = size;

  if(audio_first + size > audio_size){
    first_read = audio_size - audio_first;
    rest_read = size - first_read;
  }
  memcpy(in_buf, (const uint8_t*)audio_buf + audio_first, first_read);
  memcpy((uint8_t*)in_buf + first_read, audio_buf, rest_read);
  audio_first = (audio_first + size) % audio_size;
}

void sdl_audio_cb(void *data, uint8_t *stream, int len)
{
  size_t avail = fifo_read_avail();
  size_t write_size = len > (int)avail ? avail : len;

  fifo_read(stream, write_size);
  memset(stream + write_size, 0, len - write_size);
}

static void my_video(const void *data, unsigned width, unsigned height, size_t pitch)
{
  int x, y, index=0;
  uint16_t *s = ((uint16_t*)data + ox) + (oy * (pitch >> 1));
  uint16_t *d = screen->pixels;

  if(zoom == 2){
    d+= 9;
    for(y=0; y<240; y++){
      for(x=0; x<300; x++){
        *d++ = *s++;
        s+= 1;
      }
      s+= (pitch >> 1) - 600;
      s+= (pitch >> 1);
      d+= 20;

      if(y == 120){
        s+= ((pitch >> 1) * 140);
      }
    }
  }
  else if(zoom == 3){
    for(y=0; y<240; y++){
      for(x=0; x<320; x++){
        *d++ = *s++;
        s+= 1;

        if(x == 160){
          s+= 160;
        }
      }
      s+= (pitch >> 1) - 640 - 160;
      s+= (pitch >> 1);
    }
  }
  else if(zoom == 4){
    for(y=0; y<240; y++){
      for(x=0; x<320; x++){
        *d++ = *s++;
        s+= 1;

        if(x == 160){
          s+= 220;
        }
      }
      s+= (pitch >> 1) - 640 - 220;
      s+= (pitch >> 1);
    }
  }
  else{
    for(y=0; y<240; y++){
      memcpy(d, s, 640);
      d+= 320;
      s+= 320;
      s+= (pitch >> 1) - 320;
    }
  }
  SDL_Flip(screen);
}

static size_t my_audio(const int16_t *data, size_t samples)
{
  size_t written = 0;
	int size = samples << 2;

  while (written < size){
    size_t avail;

    SDL_LockAudio();
    avail = fifo_write_avail();
    if(avail == 0){
      SDL_UnlockAudio();
    }
    else{
      size_t write_amt = size - written > avail ? avail : size - written;
      fifo_write((const char*)data + written, write_amt);
      SDL_UnlockAudio();
      written+= write_amt;
    }
  }
}

static int16_t my_input(unsigned port, unsigned device, unsigned index, unsigned id)
{
  if((port == 0) && (device == 1) && (index == 0)){
    if(zoom == 0){
      if(id == RETRO_DEVICE_ID_JOYPAD_SELECT){
        zoom = 1;
        return 1;
      }
    }
    return key[id];
  }
  return 0;
}

static void my_poll(void)
{
}

static void my_log(enum retro_log_level level, const char* fmt, ...)
{
}

static bool my_env(unsigned cmd, void *data)
{
  //printf("cmd: %d, data: 0x%x\n", cmd, data);
  switch(cmd){
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    ((struct retro_log_callback*)data)->log = my_log;
    break;
  case RETRO_ENVIRONMENT_SET_GEOMETRY:
    cw = ((struct retro_game_geometry*)data)->base_width;
    ch = ((struct retro_game_geometry*)data)->base_height;
    //printf("width:%d, height:%d\n", cw, ch);

    ox = (cw - 320) / 2;
    oy = (ch - 240) / 2;

    // Hippo Teeth (VTech, Sporty Time & Fun)
    if((cw == 480) && (ch == 650)){
      zoom = 1;
      oy = 325;
    }
    // Hot Line (VTech, Sporty Time & Fun)
    if((cw == 480) && (ch == 656)){
      zoom = 1;
      oy = 325;
    }
    // Snoopy (Nintendo, Panorama Screen)
    if((cw == 498) && (ch == 770)){
      zoom = 1;
      oy = 80;
    }
    // Donkey Kong (Nintendo, Multi Screen)
    if((cw == 606) && (ch == 748)){
      zoom = 2;
      ox = 0;
      oy = 60;
    }
    // Donkey Kong II (Nintendo, Multi Screen)
    if((cw == 605) && (ch == 748)){
      zoom = 2;
      ox = 0;
      oy = 60;
    }
    // Lifeboat (Nintendo, Multi Screen)
    if((cw == 974) && (ch == 532)){
      zoom = 3;
      ox = 86;
      oy = 0;
    }
    // Mario Bros. (Nintendo, Multi Screen)
    if((cw == 973) && (ch == 532)){
      zoom = 3;
      ox = 86;
      oy = 0;
    }
    // Penguin Land (Bandai, LSI Game Double Play)
    if((cw == 1073) && (ch == 580)){
      zoom = 4;
      ox = 110;
      oy = 20;
    }
    break;
  }
  return true;
}

int main(int argc, char**argv)
{
  int fd=-1;
  int val=0;
  FILE *fp=NULL;
  void *dl=NULL;
  void *buf=NULL;
  uint32_t rfd;
  uint32_t target;
  SDL_Event event;
  SDL_AudioSpec spec;

  val = strlen(argv[1]);
  if(strcasecmp(&argv[1][val-4], ".mgw")){
    //printf("invalid extension name\n");
    return -1;
  }

  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0){
    //printf("failed to init sdl\n");
    return -1;
  }
  
  screen = SDL_SetVideoMode(320, 240, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
  if(screen == NULL){
    //printf("failed to set video mode\n");
    SDL_Quit();
    return -1;
  }
  SDL_ShowCursor(0);
	joy = SDL_JoystickOpen(0);

  spec.freq = 44100;
  spec.format = AUDIO_S16SYS;
  spec.channels = 2;
  spec.samples = 512;
  spec.callback = sdl_audio_cb;
  spec.userdata = NULL;
  if(SDL_OpenAudio(&spec, NULL) < 0){
    //printf("failed to open audio device\n");
    SDL_Quit();
    return -1;
  }
  SDL_PauseAudio(0);
 
  dl = dlopen ("./gw_libretro.so", RTLD_LAZY);
  if(dl == NULL){
    //printf("failed to so library\n");
    SDL_Quit();
    return -1;
  }

  f_get_info = dlsym(dl, "retro_get_system_info");
  f_get_info(&r_info);
  //printf("info:\n");
  //printf("  name: %s\n", r_info.library_name);
  //printf("  version: %s\n", r_info.library_version);
  //printf("  extension: %s\n", r_info.valid_extensions);
  if(strcmp(r_info.library_name, "Game & Watch")){
    SDL_Quit();
    return -1;
  }
  if(strcmp(r_info.library_version, "1.6.3")){
    SDL_Quit();
    return -1;
  }
  if(strcmp(r_info.valid_extensions, "mgw")){
    SDL_Quit();
    return -1;
  }

  f_set_env = dlsym(dl, "retro_set_environment");
  f_set_env(my_env);

  f_init_core = dlsym(dl, "retro_init");
  f_init_core();

  f_load_game = dlsym(dl, "retro_load_game");
  r_game.path = argv[1];

  fp = fopen(r_game.path, "rb");
  if(fp == NULL){
    //printf("failed to open roms for checking size\n");
    dlclose(dl);
    SDL_Quit();
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  r_game.size = ftell(fp);
  fclose(fp);
  
  buf = malloc(r_game.size);
  if(buf == NULL){
    //printf("failed to allocate memory\n");
    dlclose(dl);
    SDL_Quit();
    return -1;
  }
  r_game.data = buf;

  fd = open(r_game.path, O_RDONLY);
  if(fd < 0){
    //printf("failed to load game\n");
    free(buf);
    dlclose(dl);
    SDL_Quit();
    return -1;
  }
  read(fd, buf, r_game.size); 
  f_load_game(&r_game);

  f_refresh_video = dlsym(dl, "retro_set_video_refresh");
  f_refresh_video(my_video);

  f_sample_audio_batch = dlsym(dl, "retro_set_audio_sample_batch");
  f_sample_audio_batch(my_audio);

  f_set_input = dlsym(dl, "retro_set_input_state");
  f_set_input(my_input);

  f_set_poll = dlsym(dl, "retro_set_input_poll");
  f_set_poll(my_poll);

  f_run_core = dlsym(dl, "retro_run");

  target+= F;
  init_tv.tv_sec = 0;   
  init_tv.tv_usec = 0;
  while(loop){
    f_run_core();
    if(SDL_PollEvent(&event)){
      val = 0;
      if(event.type == SDL_KEYDOWN){
        val = 1;
      }
      if(event.key.keysym.sym == SDLK_HOME){
        break;
      }
      if(event.key.keysym.sym == SDLK_UP){
        key[RETRO_DEVICE_ID_JOYPAD_UP] = val;
      }
      if(event.key.keysym.sym == SDLK_DOWN){
        key[RETRO_DEVICE_ID_JOYPAD_DOWN] = val;
      }
      if(event.key.keysym.sym == SDLK_LEFT){
        key[RETRO_DEVICE_ID_JOYPAD_LEFT] = val;
      }
      if(event.key.keysym.sym == SDLK_RIGHT){
        key[RETRO_DEVICE_ID_JOYPAD_RIGHT] = val;
      }
      if(event.key.keysym.sym == SDLK_LALT){ // SDLK_LCTRL){
        key[RETRO_DEVICE_ID_JOYPAD_A] = val;
      }
      if(event.key.keysym.sym == SDLK_LCTRL){ //SDLK_LALT){
        key[RETRO_DEVICE_ID_JOYPAD_B] = val;
      }
      if(event.key.keysym.sym == SDLK_LSHIFT){ // SDLK_SPACE){
        key[RETRO_DEVICE_ID_JOYPAD_X] = val;
      }
      if(event.key.keysym.sym == SDLK_SPACE){ // SDLK_LSHIFT){
        key[RETRO_DEVICE_ID_JOYPAD_Y] = val;
      }
      if(event.key.keysym.sym == SDLK_RETURN){
        key[RETRO_DEVICE_ID_JOYPAD_START] = val;
      }
      if(event.key.keysym.sym == SDLK_ESCAPE){
        break;
      }
      if(event.key.keysym.sym == SDLK_TAB){
        key[RETRO_DEVICE_ID_JOYPAD_L] = val;
      }
      if(event.key.keysym.sym == SDLK_BACKSPACE){
        key[RETRO_DEVICE_ID_JOYPAD_R] = val;
      }
      if(event.key.keysym.sym == SDLK_PAGEUP){
        key[RETRO_DEVICE_ID_JOYPAD_L2] = val;
      }
      if(event.key.keysym.sym == SDLK_PAGEDOWN){
        key[RETRO_DEVICE_ID_JOYPAD_R2] = val;
      }
      
			if(event.type == SDL_JOYAXISMOTION){
        const int min_axis = 10000;
        static int cnt=0;
        static int jk = -1;
        if((int)SDL_JoystickGetAxis(joy, 0) > min_axis){
          if(cnt++ > 5){
            cnt = 0;
        		key[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 1;
          }
          else{
        		key[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 0;
          }
        }
        if((int)SDL_JoystickGetAxis(joy, 0) < -min_axis){
          if(cnt++ > 5){
            cnt = 0;
        		key[RETRO_DEVICE_ID_JOYPAD_LEFT] = 1;
          }
          else{
        		key[RETRO_DEVICE_ID_JOYPAD_LEFT] = 0;
          }
        }
				if((int)SDL_JoystickGetAxis(joy, 1) < -min_axis){
          if(cnt++ > 5){
            cnt = 0;
        		key[RETRO_DEVICE_ID_JOYPAD_UP] = 1;
          }
          else{
        		key[RETRO_DEVICE_ID_JOYPAD_UP] = 0;
          }
        }
        if((int)SDL_JoystickGetAxis(joy, 1) > min_axis){
          if(cnt++ > 5){
            cnt = 0;
        		key[RETRO_DEVICE_ID_JOYPAD_DOWN] = 1;
          }
          else{
        		key[RETRO_DEVICE_ID_JOYPAD_DOWN] = 0;
          }
        }
			}
    }
  
    rfd = get_ticks();
    target+= F;
    if(rfd < target){
      while(get_ticks() < target) {
        usleep(5);
      }
    }
  }

  f_unload_game = dlsym(dl, "retro_unload_game");
  f_unload_game();

  f_deinit_core = dlsym(dl, "retro_deinit");
  f_deinit_core();

  dlclose(dl);
  free(buf);

  while(audio_len > 0){
    SDL_Delay(100);
  }
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  if(joy){
    SDL_JoystickClose(joy);
  }

  {
    int x, y, index=0;
    uint16_t *d = screen->pixels;
    extern uint16_t hex_credits[];

    for(y=0; y<240; y++){
      for(x=0; x<320; x++){
        *d++ = hex_credits[index++];
      }
    }
    SDL_Flip(screen);
    while(1){
      if(SDL_PollEvent(&event)){
        if(event.type == SDL_KEYDOWN){
          break;
        }
      }
      usleep(1000);
    }
  }
  SDL_Quit();
  return 0;
}

