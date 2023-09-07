/* Put all of the routines that call SDL functions in this module */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_cpuinfo.h>
#include <SDL_platform.h>
#include <SDL_version.h>

#include "port.h"
#include "prototyp.h"
#include "fractype.h"

/* SDL global variables */
SDL_Surface *mainscrn = NULL;
SDL_Surface *backscrn = NULL;
SDL_Surface *textscrn = NULL;
SDL_Window *sdlWindow = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Texture *sdlTexture = NULL;
SDL_Texture *sdlTextTexture = NULL;

Uint32 sdlPixelfmt;
int rowbytes;
int textrowbytes;
SDL_PixelFormat *sdlPixelFormat;
static int display_in_use = 0;
int x_close = 0;
int file_IO = 0; /* kludge to indicate saving/restoring an image */

TTF_Font *font = NULL;
SDL_Color cols[DACSIZE];
int SDL_init_flags = SDL_INIT_VIDEO|SDL_INIT_TIMER;
int SDL_video_flags = /*SDL_WINDOW_RESIZABLE |*/ SDL_WINDOW_ALLOW_HIGHDPI;
int SDL_renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE;
SDL_Cursor *mousecurser = NULL;

struct dac_color_info {
  int sizex;        /* This is the image width. */
  int sizey;        /* This is the image height */
  long *color_info; /* This is the iteration count for each pixel. */
} Image_Data;

long *save_color_info = NULL;

SDL_Color XlateText[] =
{
#if BYTE_ORDER == BIG_ENDIAN
/* R, G, B, A */
  {  0,  0,  0,255}, /* black */
  {  0,  0,205,255}, /* Blue */
  {  0,205,  0,255}, /* Green */
  {  0,205,205,255}, /* Cyan */
  {205,  0,  0,255}, /* Red */
  {205,  0,205,255}, /* Magenta */
  {205,205,  0,255}, /* Brown */
  {205,205,205,255}, /* White */
  {100,100,100,255}, /* Gray */
  {  0,  0,255,255}, /* L_Blue */
  {  0,255,  0,255}, /* L_Green */
  {  0,255,255,255}, /* L_Cyan */
  {255,  0,  0,255}, /* L_Red */
  {255,  0,255,255}, /* L_Magenta */
  {255,255,  0,255}, /* Yellow */
  {255,255,255,255}  /* L_White */
#else
/* B, G, R, A */
  {  0,  0,  0,255}, /* black */
  {205,  0,  0,255}, /* Blue */
  {  0,205,  0,255}, /* Green */
  {205,205,  0,255}, /* Cyan */
  {  0,  0,205,255}, /* Red */
  {205,  0,205,255}, /* Magenta */
  {  0,205,205,255}, /* Brown */
  {205,205,205,255}, /* White */
  {100,100,100,255}, /* Gray */
  {255,  0,  0,255}, /* L_Blue */
  {  0,255,  0,255}, /* L_Green */
  {255,255,  0,255}, /* L_Cyan */
  {255,  0,  0,255}, /* L_Red */
  {255,  0,255,255}, /* L_Magenta */
  {  0,255,255,255}, /* Yellow */
  {255,255,255,255}  /* L_White */
#endif
};

static int mousefkey[4][4] /* [button][dir] */ =
{
  {RIGHT_ARROW,LEFT_ARROW,DOWN_ARROW,UP_ARROW},
  {0,0,PAGE_DOWN,PAGE_UP},
  {CTL_PLUS,CTL_MINUS,CTL_DEL,CTL_INSERT},
  {CTL_END,CTL_HOME,CTL_PAGE_DOWN,CTL_PAGE_UP}
};

int resize_flag = 0;
int window_is_fullscreen = 0;
int updatewindow = 1;
static int screenctr = -1;
int txt_ht;  /* text letter height = 2^txt_ht pixels */
int txt_wt;  /* text letter width = 2^txt_wt pixels */

char text_screen[TEXT_HEIGHT][TEXT_WIDTH];
int  text_attr[TEXT_HEIGHT][TEXT_WIDTH];
static double mouse_scale_x = 1.0;
static double mouse_scale_y = 1.0;

/* Routines in this module */
void Slock(SDL_Surface *);
void Sulock(SDL_Surface *);
void outtext(int, int, int);
int check_mouse(SDL_Event);
void set_mouse_scale(void);
void updateimage(void);
void SetupSDL(void);
void CleanupSDL(void);

void Slock(SDL_Surface *screen)
{
  if ( SDL_MUSTLOCK(screen) )
    {
      if ( SDL_LockSurface(screen) < 0 )
        {
          return;
        }
    }
}

void Sulock(SDL_Surface *screen)
{
  if ( SDL_MUSTLOCK(screen) )
    {
      SDL_UnlockSurface(screen);
    }
}

void SetupSDL(void)
{
  /* called by main() routine */

  char msg[80];
  SDL_version compiled;
  SDL_version linked;

  sdl_check_for_windows();

  if ( SDL_Init(SDL_init_flags) < 0 )
    {
      sprintf(msg, "Unable to initialize SDL: %s\n", SDL_GetError());
      popup_error(4, msg);
      exit(1);
    }

  SDL_VERSION(&compiled);
  SDL_GetVersion(&linked);
  if (compiled.major > linked.major)
    {
      sprintf(msg, "Compiled version: %d\nLinked version: %d\n",
              compiled.major, linked.major);
      popup_error(4, msg);
      exit(1);
    }

  if (TTF_Init() < 0)
    {
      sprintf(msg, "Unable to initialize SDL TTF: %s\n", SDL_GetError());
      popup_error(4, msg);
      exit(1);
    }

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
      sprintf(msg, "Unable to initialize SDL audio: %s\n", SDL_GetError());
      popup_error(3, msg);
    }

  setup_sdl_audio();

// NOTE (jonathan#1#): May not need this once png support is added.
//  if ( IMG_Init(IMG_INIT_PNG) < 0 )
//    {
//      fprintf(stderr, "Unable to init IMG: %s\n", SDL_GetError());
//      exit(1);
//    }

}

#define USE_THREAD 1

#ifdef USE_THREAD
static int RemoveImageData(void *ptr)
{
  free(Image_Data.color_info);
  if (save_color_info != NULL)
    free(save_color_info);
  return (0);
}
#endif

void CleanupSDL(void)
{
  /*
   * Quit SDL so we can release the fullscreen
   * mode and restore the previous video settings,
   * etc.  Called by goodbye() routine.
   */
#ifdef USE_THREAD
  SDL_Thread *CleanupThread;

  CleanupThread = SDL_CreateThread(RemoveImageData, "CleanupThread", (void*)NULL);

  SDL_DetachThread(CleanupThread);
#else
  free(Image_Data.color_info);
  if (save_color_info != NULL)
    free(save_color_info);
#endif
  SDL_DestroyRenderer(sdlRenderer);
  if (sdlTexture != NULL)
    SDL_DestroyTexture(sdlTexture);
  if (sdlTextTexture != NULL)
    SDL_DestroyTexture(sdlTextTexture);
  SDL_FreeSurface(mainscrn);
  SDL_FreeSurface(backscrn);
  SDL_FreeSurface(textscrn);

  SDL_FreeCursor(mousecurser);

  cleanup_sdl_audio();

  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  TTF_CloseFont(font);
  font = NULL;
  TTF_Quit();

  SDL_Quit();
  delay(250);
}

void popup_error (int num, char *msg)
{
  switch (num)
  {
   case 0:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "Creation Error",
                         msg,
                         NULL);
      break;
   case 1:
   default:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "Error reading parameters/sstools.ini",
                         msg,
                         NULL);
      break;
   case 2:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                         "Warning",
                         msg,
                         NULL);
      break;
   case 3:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                         "Information",
                         msg,
                         NULL);
      break;
   case 4:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "Critical error",
                         msg,
                         NULL);
      break;
  }
}

void set_mouse_scale(void)
{
  /* Mouse motion is based on window (renderer) size.
   * This routine scales the mouse motion to the image size.
  */
  int rend_size_w;
  int rend_size_h;

  SDL_GetRendererOutputSize(sdlRenderer, &rend_size_w, &rend_size_h);

  mouse_scale_x = (double)sxdots / (double)rend_size_w;
  mouse_scale_y = (double)sydots / (double)rend_size_h;
}

static int doneonce = 0;
int winsize = SF7;

void SetupWindow(void)
{
  SDL_Surface *WinIcon;
  Uint32 rmask, gmask, bmask, amask;
  char msg[80];
  int rend_size_w;
  int rend_size_h;
  int bpp; /* bits per pixel for graphics mode */
  int fontsize;
#if DEBUG
  int win_size_w;
  int win_size_h;
#endif

  if (doneonce)
     return;
  else
     doneonce = 1;

  switch (winsize)
  {
     case SF6:
        sdlWindow = SDL_CreateWindow(0, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    800, 600, SDL_video_flags);
        break;
     case SF7:
     default:
        sdlWindow = SDL_CreateWindow(0, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    1024, 768, SDL_video_flags);
        break;
     case SF9:
        sdlWindow = SDL_CreateWindow(0, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    1280, 1024, SDL_video_flags);
        break;
  }

  if ( sdlWindow == NULL ) /* No luck, bail out */
    {
      sprintf(msg, "Window creation fail : %s\n", SDL_GetError());
      popup_error(4, msg);
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation fail : %s\n",SDL_GetError());
      exit(1);
    }

  WinIcon = SDL_LoadBMP("Fractint.bmp");
  if (WinIcon != NULL)
  {
    SDL_SetWindowIcon( sdlWindow, WinIcon );
    SDL_FreeSurface( WinIcon );
  }
  Image_Data.color_info = NULL;

#if SDL_VERSION_ATLEAST(2, 0, 16)
  /*  this next needs SDL 2.0.16 */
  SDL_SetWindowKeyboardGrab(sdlWindow, SDL_TRUE);
#endif

  sdlPixelfmt = SDL_GetWindowPixelFormat(sdlWindow);
  bpp = SDL_BYTESPERPIXEL(sdlPixelfmt) * 8;

#if DEBUG
  SDL_GetWindowSize(sdlWindow, &win_size_w, &win_size_h);
  sprintf(msg, "%s at %dx%dx%d", Fractint, win_size_w, win_size_h, bpp);
#elif PRODUCTION
  sprintf(msg, Fractint);
#else
  sprintf(msg, "%s SDL Developmental Alpha", Fractint);
#endif /* 1 */

  SDL_SetWindowTitle(sdlWindow, msg);

  if ( sdlRenderer != NULL ) /* Don't create two with same name */
      SDL_DestroyRenderer(sdlRenderer);
  sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_renderer_flags);

  if ( sdlRenderer == NULL )
    {
      sprintf(msg, "Render creation for window surface fail : %s\n", SDL_GetError());
      popup_error(4, msg);
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render creation fail : %s\n",SDL_GetError());
      exit(1);
    }

  SDL_GetRendererOutputSize(sdlRenderer, &rend_size_w, &rend_size_h);

  /* initialize screen to black */
  SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
  SDL_RenderClear(sdlRenderer);
  SDL_RenderPresent(sdlRenderer);

  textrowbytes = SDL_BYTESPERPIXEL(sdlPixelfmt) * rend_size_w;

  if ( sdlTextTexture != NULL ) /* Don't create two with same name */
      SDL_DestroyTexture(sdlTextTexture);
  sdlTextTexture = SDL_CreateTexture(sdlRenderer, sdlPixelfmt, SDL_TEXTUREACCESS_STREAMING, rend_size_w, rend_size_h);

  if ( sdlTextTexture == NULL )
      {
      sprintf(msg, "%s\n", SDL_GetError());
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Text Texture creation fail : %s\n",SDL_GetError());
      popup_error(4, msg);
      exit(1);
      }

#if BYTE_ORDER == BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

  if (textscrn == NULL)
  {
    textscrn = SDL_CreateRGBSurface(0, rend_size_w, rend_size_h, bpp, rmask, gmask, bmask, amask);
  }

#if DEBUG
  if (textscrn == NULL )
      {
      sprintf(msg, "No textscrn.\n");
      popup_error(0, msg);
      }
#endif

  SDL_FillRect(textscrn, NULL, 0);

  if (font != NULL)
    {
      TTF_CloseFont(font);
      font = NULL;
    }
  fontsize = (int)(rend_size_h / 34) + 1; /* arbitrary font size, not too big */
  font = TTF_OpenFont("crystal.ttf", fontsize);
  if ( font == NULL )
    {
      sprintf(msg, "Missing file crystal.ttf or couldn't set font.  Fractint will crash.\n");
      popup_error(4, msg);
      exit(1);
    }

  /* get text height and width in pixels */
  TTF_SizeText(font,"H",&txt_wt,&txt_ht);

  return;
}


void ResizeScreen(void)
{
  Uint32 rmask, gmask, bmask, amask;
  char msg[80];
  int bpp; /* bits per pixel for graphics mode */

  /*
   * Initialize the display to 1024x768,
   */
  if (adapter < 0)   /* make sure we have something reasonable */
  {
      adapter = check_vidmode_key(0, check_vidmode_keyname("SF7"));  /* Default to SF7 1024x768 */
         memcpy((char *)&videoentry,(char *)&videotable[adapter],
            sizeof(videoentry));  /* the selected entry now in videoentry */
  }
  if (calc_status != 4) /* if not complete */
      calc_status = 0;  /* can't resume anyway */

  if ( sdlTexture != NULL )
      SDL_DestroyTexture(sdlTexture);
  sdlTexture = NULL;
  if (mainscrn != NULL)
      SDL_FreeSurface(mainscrn);
  mainscrn = NULL;
  if (backscrn != NULL)
      SDL_FreeSurface(backscrn);
  backscrn = NULL;
  if (Image_Data.color_info != NULL)
      free(Image_Data.color_info);
  Image_Data.color_info = NULL;

  sxdots = videoentry.xdots;
  sydots = videoentry.ydots;
  screenaspect = (float)sydots / (float)sxdots;
  finalaspectratio = screenaspect;
  dotmode = videoentry.dotmode;
  colors = videoentry.colors;
  rotate_hi = colors - 1;
  if (Image_Data.color_info == NULL)
  {
      Image_Data.sizex = sxdots;
      Image_Data.sizey = sydots;
      Image_Data.color_info = (long *)malloc(sxdots * sydots * sizeof(long));
  }

  bpp = SDL_BYTESPERPIXEL(sdlPixelfmt) * 8;

  rowbytes = SDL_BYTESPERPIXEL(sdlPixelfmt) * sxdots;
  if (dotmode == 0)
    videoentry.dotmode = bpp;

  sdlTexture = SDL_CreateTexture(sdlRenderer, sdlPixelfmt, SDL_TEXTUREACCESS_STREAMING, sxdots, sydots);

  if ( sdlTexture == NULL )
      {
      sprintf(msg, "%s\n", SDL_GetError());
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Image Texture creation fail : %s\n",SDL_GetError());
      popup_error(4, msg);
      exit(1);
      }

#if BYTE_ORDER == BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

  mainscrn = SDL_CreateRGBSurface(0, sxdots, sydots, bpp, rmask, gmask, bmask, amask);
  backscrn = SDL_CreateRGBSurface(0, sxdots, sydots, bpp, rmask, gmask, bmask, amask);

#if DEBUG
  if (mainscrn == NULL )
      {
      sprintf(msg, "No mainscrn.\n");
      popup_error(4, msg);
      exit(1);
      }
  if (backscrn == NULL )
      {
      sprintf(msg, "No backscrn.\n");
      popup_error(4, msg);
      exit(1);
      }
#endif

  SDL_FillRect(mainscrn, NULL, 0);
  SDL_FillRect(backscrn, NULL, 0);

  sdlPixelFormat = mainscrn->format;
  if (mousecurser == NULL)
    {
      mousecurser = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
      SDL_SetCursor(mousecurser);
    }
  set_mouse_scale();

  return;
}

void showfreemem(void)
{
char msg[300];
char *msgptr;
int msglen = 0;
SDL_version compiled;
SDL_version linked;
SDL_Rect max_win_size;
SDL_DisplayMode current;

msgptr = msg + msglen;
sprintf(msgptr, "Logical cores = %d.\n", SDL_GetCPUCount());

msglen = strlen(msg);
msgptr = msg + msglen;
sprintf(msgptr, "System RAM is %d MB.\n", SDL_GetSystemRAM());

msglen = strlen(msg);
msgptr = msg + msglen;
SDL_GetDisplayUsableBounds(display_in_use, &max_win_size);
sprintf(msgptr, "Usable window = %d X %d.\n", max_win_size.w, max_win_size.h);

msglen = strlen(msg);
msgptr = msg + msglen;
SDL_GetCurrentDisplayMode(display_in_use, &current);
sprintf(msgptr, "Desktop = %d X %d @ %d Hz.\n", current.w, current.h, current.refresh_rate);

msglen = strlen(msg);
msgptr = msg + msglen;
SDL_VERSION(&compiled);
sprintf(msgptr, "SDL compiled version %d.%d.%d.\n",
       compiled.major, compiled.minor, compiled.patch);

msglen = strlen(msg);
msgptr = msg + msglen;
SDL_GetVersion(&linked);
sprintf(msgptr, "SDL linked version %d.%d.%d.\n",
       linked.major, linked.minor, linked.patch);

msglen = strlen(msg);
msgptr = msg + msglen;
sprintf(msgptr, "Video driver: %s.\n", SDL_GetCurrentVideoDriver());

msglen = strlen(msg);
msgptr = msg + msglen;
sprintf(msgptr, "Audio driver: %s.\n", SDL_GetCurrentAudioDriver());

msglen = strlen(msg);
msgptr = msg + msglen;
sprintf(msgptr, "Size of Long: %d.\n", sizeof(long));


SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                         "System Information",
                         msg,
                         NULL);

return;
}

/*
; adapter_detect:
;       This routine gets the video modes supported by SDL
;       and puts the compatible ones in vidtbl.
;       It sets variable dotmode.
*/
static int done_detect = 0;

void adapter_detect(void)
{
  int wdth;
  int hgth;
  int desktop_w, desktop_h;
  int i, j, display_mode_count;
  SDL_DisplayMode mode;
  SDL_Rect max_win_size;

  if (done_detect)
    return;

  SDL_GetDesktopDisplayMode(display_in_use, &mode);
  desktop_w = mode.w;
  desktop_h = mode.h;

  SDL_GetDisplayUsableBounds(display_in_use, &max_win_size);

  display_mode_count = SDL_GetNumDisplayModes(display_in_use);
  if (display_mode_count > MAXVIDEOTABLE)
    display_mode_count = MAXVIDEOTABLE;

  memset((char *)vidtbl,0,sizeof(*vidtbl)*display_mode_count);

  i = 0;
  j = 0;
  do {
      if (SDL_GetDisplayMode(display_in_use, i, &mode) != 0)
        {
         char msg[40];
         sprintf(msg, "SDL_GetDisplayMode failed: %s", SDL_GetError());
         popup_error(2, msg);
        }
      if (mode.refresh_rate == 60 && mode.w >= 320) {
        if (mode.w == desktop_w && mode.h == desktop_h)
          strncpy(vidtbl[j].comment, "Full Desktop Mode", 25);
        else
          strncpy(vidtbl[j].comment, SDL_GetPixelFormatName(mode.format), 25);
        vidtbl[j].dotmode = SDL_BYTESPERPIXEL(sdlPixelfmt) * 8;
        sprintf(vidtbl[j].name, "%i-bit True-Color", vidtbl[j].dotmode);
        vidtbl[j].xdots = mode.w;
        vidtbl[j].ydots = mode.h;
        vidtbl[j].colors = 256; /* this will need to be fixed */
        switch (mode.w)
        {
          case 640:
            if (mode.h == 400)
               vidtbl[j].keynum = check_vidmode_keyname("SF4");
            if (mode.h == 480)
               vidtbl[j].keynum = check_vidmode_keyname("SF5");
            break;
          case 800:
            if (mode.h == 600)
              vidtbl[j].keynum = check_vidmode_keyname("SF6");
            break;
          case 1024:
            if (mode.h == 768)
              vidtbl[j].keynum = check_vidmode_keyname("SF7");
            break;
          case 1280:
            if (mode.h == 1024)
              vidtbl[j].keynum = check_vidmode_keyname("SF9");
            break;
          default:
            break;
        }
        j++;
      }
      i++;
  } while (i < display_mode_count);
  vidtbllen = j;

  SDL_GetWindowSize(sdlWindow, &wdth, &hgth);
  xdots = wdth;
  ydots = hgth;
  sxdots  = xdots;
  sydots  = ydots;
  done_detect = 1;
  dotmode = SDL_BYTESPERPIXEL(sdlPixelfmt) * 8;
/*  update_fractint_cfg(); */
}

int checkwindowsize(int x, int y)
{
  int fits_in_window = FALSE;
  SDL_DisplayMode DTmode;

  SDL_GetDesktopDisplayMode(display_in_use, &DTmode);
  if (DTmode.w >= x && DTmode.h >= y)
    fits_in_window = TRUE;
  return (fits_in_window);
}

void startvideo(void)
{
  int Bpp = SDL_BYTESPERPIXEL(sdlPixelfmt);

  if (Bpp == 1) /* 8-bits per pixel => uses a palette */
    {
      gotrealdac = 1;
      istruecolor = 0;
      colors = videoentry.colors;
    }
  else /* truecolor modes */
    {
      gotrealdac = 0;
      istruecolor = 0;  /* seems this is no longer needed */
      fake_lut = 1;
// FIXME (jonathan#1#): Need to have more colors for truecolor modes
      colors = videoentry.colors;
    }

/* initialize mainscrn and backscrn surfaces to inside color */
  SDL_FillRect(mainscrn, NULL, map_to_pixel(inside & andcolor));
  SDL_FillRect(backscrn, NULL, map_to_pixel(inside & andcolor));

  memset(Image_Data.color_info, 0, Image_Data.sizex * Image_Data.sizey * sizeof(long));
  /* initialize window to black */
  SDL_UpdateTexture( sdlTexture, NULL, mainscrn->pixels, rowbytes );
  SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
  SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
  SDL_RenderClear(sdlRenderer);
  SDL_RenderPresent(sdlRenderer);
  screenctr = -1;
}

U32 map_to_pixel(BYTE color)
{
  /* returns the pixel value corresponding to the truemode selection */
  BYTE red, green, blue;

  dac_to_rgb(color, &red, &green, &blue);
  return((U32)SDL_MapRGB(sdlPixelFormat, red, green, blue));
}

/*
 * Return the pixel value at (x, y)
 * This is the DAC index value.
 * Need to change this to allow more colors.
 */
BYTE readvideo(int x, int y)
{
  long *p;

  p = (long *)Image_Data.color_info + Image_Data.sizex * y + x;

  return ((BYTE) *p & andcolor);
}

#if 0
void gettruecolor_SDL(SDL_Surface *screen, int x, int y, Uint8 *red, Uint8 *green, Uint8 *blue)
{
/* NOT USED */
  /* Extracting color components from a 32-bit color value */
  Uint32 *pixel;

  Slock(screen);
  pixel = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
  Sulock(screen);

  SDL_GetRGB(*pixel, sdlPixelFormat, (Uint8 *)red, (Uint8 *)green, (Uint8 *)blue);
}
#endif

void gettruecolor(int x, int y, BYTE *R, BYTE *G, BYTE *B)
{
  dac_to_rgb(readvideo(x, y), R, G, B);

/*  gettruecolor_SDL(mainscrn, x, y, (Uint8 *)R, (Uint8 *)G, (Uint8 *)B); */
}

void puttruecolor(int x, int y, BYTE R, BYTE G, BYTE B)
{
  Uint32 color = SDL_MapRGB(mainscrn->format, R, G, B);
  SDL_Rect dst;

  dst.x = x;
  dst.y = y;
  dst.w = 1;
  dst.h = 1;

  /* put the pixel color on the screen */
  SDL_FillRect(mainscrn, &dst, color);

  if (show_orbit && calc_status == 1 && soundflag&6) /* Do it slow, update after each pixel */
  {
       updateimage();
  }
}

/*
 * Set the pixel at (x, y) to the given value
 */
void writevideo(int x, int y, U32 pixel)
{
  BYTE red, green, blue;
  long *bufp;
  char msg[80];

#if DEBUG
  if (x > sxdots || y > sydots)
  {
     sprintf(msg, "Plotting x = %li and y = %li.\n", x, y);
     popup_error(2, msg);
  }
#endif /* DEBUG */

  bufp = (long *)Image_Data.color_info + Image_Data.sizex * y + x;
  *bufp = pixel;
  dac_to_rgb((BYTE)(pixel & andcolor), &red, &green, &blue);
  puttruecolor(x, y, red, green, blue);
}

void updateimage(void)
{
  if (dotmode == 11 || screenctr != -1)
  {
    SDL_UpdateTexture( sdlTextTexture, NULL, textscrn->pixels, textrowbytes );
    SDL_RenderCopy(sdlRenderer, sdlTextTexture, NULL, NULL);
  }
  else
  {
    void *pixels;

    SDL_LockTexture(sdlTexture, NULL, &pixels, &rowbytes);
    memcpy(pixels, mainscrn->pixels, rowbytes*sydots);
    SDL_UnlockTexture(sdlTexture);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
  }
  SDL_RenderPresent(sdlRenderer);
}

int saveimagedata(void)
{
  long data_length;
  int ret = 1;

  data_length = sxdots * sydots * sizeof(long);
  if (save_color_info == NULL)
    save_color_info = (long *)malloc(data_length);
  else       /* already exists, probably the wrong size */
    save_color_info = (long *)realloc(save_color_info, data_length);
  if (save_color_info == NULL) /* we failed */
    ret = 0;
  else  /* we won */
    memcpy(save_color_info, Image_Data.color_info, data_length);
  return (ret);
}

void restoreimagedata(void)
{
  long data_length;

  data_length = sxdots * sydots * sizeof(long);
  memcpy(Image_Data.color_info, save_color_info, data_length);
  free(save_color_info);
  save_color_info = NULL;
  refreshimage();
  updateimage();
}

/*
 * Refresh image with new DAC information
 */
void refreshimage(void)
{
  BYTE red, green, blue;
  long *bufp;
  int x, y;

  bufp = (long *)Image_Data.color_info;
  for (y = 0; y < Image_Data.sizey; y++)
    for (x = 0; x < Image_Data.sizex; x++)
    {
      bufp = (Image_Data.color_info + (Image_Data.sizex * y) + x);
      dac_to_rgb((BYTE)(*bufp & andcolor), &red, &green, &blue);
      puttruecolor(x, y, red, green, blue);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * writevideoline --
 *
 *  Write a line of pixels to the screen.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Draws pixels.
 *
 *----------------------------------------------------------------------
 */
void writevideoline(int y, int x, int lastx, BYTE *pixels)
{
  int width;
  int i;
#if 0
  int pixel;
  BYTE red, green, blue;
  long *bufp;
  SDL_Rect line;

  bufp = (long *)Image_Data.color_info + Image_Data.sizex * y + x;
#endif

  width = lastx-x+1;

  for (i=0; i<width; i++)
    {
#if 0
      *bufp++ = (U32)pixels;
#else
      writevideo(x+i, y, (U32)pixels[i]);
#endif
    }
#if 0
  line.x = x;
  line.y = y;
  line.h = 1;
  line.w = width;
  pixel = (int) *pixels;

  dac_to_rgb((BYTE)(pixel & andcolor), &red, &green, &blue);

  SDL_FillRect(mainscrn, &line, SDL_MapRGB(mainscrn->format, red, green, blue));
#endif
}
/*
 *----------------------------------------------------------------------
 *
 * readvideoline --
 *
 *  Reads a line of pixels from the screen.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Gets pixels
 *
 *----------------------------------------------------------------------
 */
void readvideoline(int y, int x, int lastx, BYTE *pixels)
{
  int i,width;
  width = lastx-x+1;
  for (i=0; i<width; i++)
    {
      pixels[i] = (BYTE)readvideo(x+i,y);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * readvideopalette --
 *  Reads the current video palette into dacbox.
 *
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Fills in dacbox.
 *
 *----------------------------------------------------------------------
 */
void readvideopalette(void)
{
#if 0
  int i;
  for (i=0; i<colors; i++)
    {
      dacbox[i][0] = cols[i].r >> 2;
      dacbox[i][1] = cols[i].g >> 2;
      dacbox[i][2] = cols[i].b >> 2;
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * writevideopalette --
 *  Writes dacbox into the video palette.
 *
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Changes the displayed colors.
 *
 *----------------------------------------------------------------------
 */
void writevideopalette(void)
{
  int i;

  for (i = 0; i < colors; i++)
    {
#if BYTE_ORDER == BIG_ENDIAN
        {
          cols[i].r = dacbox[i][2] << 2;
          cols[i].g = dacbox[i][1] << 2;
          cols[i].b = dacbox[i][0] << 2;
        }
#else
        {
          cols[i].r = dacbox[i][0] << 2;
          cols[i].g = dacbox[i][1] << 2;
          cols[i].b = dacbox[i][2] << 2;
        }
#endif
    }
  /* Set palette */
  SDL_SetPaletteColors(mainscrn->format->palette, cols, 0, DACSIZE);
  SDL_SetPaletteColors(backscrn->format->palette, cols, 0, DACSIZE);
  refreshimage();
}

void savevideopalette(void)
{
  memcpy(olddacbox, dacbox, DACSIZE*3);
}

void restorevideopalette(void)
{
  memcpy(dacbox, olddacbox, DACSIZE*3);
}

/* start of text processing routines */

void setclear (void)
{
  /* clears the text screen to black and zeroes the text arrays */
  int r, c;

  /*  Fill the screen with black  */
  SDL_FillRect( textscrn, NULL, SDL_MapRGB( textscrn->format, 0, 0, 0 ) );

  for (r = 0; r < TEXT_HEIGHT; r++)
    {
      for (c = 0; c < TEXT_WIDTH; c++)
        {
          text_attr[r][c] = 0;
          text_screen[r][c] = ' ';
        }
    }
}

void starttext(void)
{
  /* Setup text palette */
  int i;

  if (textscrn->format->BitsPerPixel == 8)
    {
      savevideopalette();
      for (i = 0; i < 16; i++)
        {
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
          {
            dacbox[i][2] = XlateText[i].r;
            dacbox[i][1] = XlateText[i].g;
            dacbox[i][0] = XlateText[i].b;
          }
        else
          {
            dacbox[i][0] = XlateText[i].r;
            dacbox[i][1] = XlateText[i].g;
            dacbox[i][2] = XlateText[i].b;
          }
        }
      spindac(0,1);
      SDL_SetPaletteColors(textscrn->format->palette, XlateText, 0, 16);
    }
}

void outtext(int row, int col, int max_c)
{
  /* Takes the text in text_screen[row][] with */
  /*     attributes in text_attr[row][] */
  /* and puts it on the text screen */

  SDL_Color color, bgcolor;
  SDL_Surface *text_surface = NULL;
  SDL_Rect text_rect;

  int i, j;
  int attr    = text_attr[row][col];
  int foregnd = attr & 15;
  int backgnd = (attr >> 4) & 15;
  char buf[TEXT_WIDTH+1]; /* room for text and null terminator */
#if 0
  fprintf(stderr, "outtext, %d, %d, %d\n", row, col, max_c);
  fprintf(stderr, "  attributes, %d, %d, %d\n", attr, foregnd, backgnd);
{
      char errortxt[40];
      sprintf(errortxt, "outtext, %d, %d, %d\nattributes, %d, %d, %d", row, col, max_c, attr, foregnd, backgnd);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "OutText",
                         errortxt,
                         NULL);
}
#endif
  if (attr & BLINK)
  {
    foregnd = (attr >> 4) & 15;
    backgnd = attr & 15;
  }
  text_rect.x = col * txt_wt;   /* starting column */
  text_rect.y = row * txt_ht;   /* starting row */
#if 0
  /* for SDL_BlitSurface, the destination width and height are ignored */
  text_rect.w = max_c * txt_wt; /* output this many columns */
  text_rect.h = 1 * txt_ht;     /* output one row at a time */
#endif

  for (i = 0; i <= TEXT_WIDTH; i++) /* clear the text buffer */
    buf[i] = 0;

  /* put text into text buffer */
  j = 0;
  for (i = col; i < max_c; i++)
    {
      buf[j++] = text_screen[row][i];
    }

  color   = XlateText[foregnd];
  bgcolor = XlateText[backgnd];

  if (!(text_surface = TTF_RenderText_Shaded(font,buf,color,bgcolor)))
    {
      /* Handle error here */
#if 0
      char errortxt[40];
      sprintf(errortxt, "outtext error: %s.", SDL_GetError());
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "Text Error",
                         errortxt,
                         NULL);
#endif
    }
  else
    {
      SDL_BlitSurface(text_surface, NULL, textscrn, &text_rect);
      SDL_FreeSurface(text_surface);
    }
}

void save_screen(void)
{
  SDL_BlitSurface( mainscrn, NULL, backscrn, NULL ); /* save mainscrn */
}

void restore_screen(void)
{
  SDL_BlitSurface( backscrn, NULL, mainscrn, NULL );
}

/*
 * The stackscreen()/unstackscreen() functions were originally
 * ported to Xfractint.
 * These functions are useful for switching between different text screens.
 * For example, looking at a parameter entry using F2.
 * The discardscreen() function dumps the text screen(s) and returns to the video screen.
 */

extern void setfortext (void);
extern void setforgraphics (void);

#define MAXSCREENS 3

struct CURSOR_POSITION
{
  int row;
  int col;
};

static char stack_text_screen[MAXSCREENS][TEXT_HEIGHT][TEXT_WIDTH];
static int  stack_text_attr[MAXSCREENS][TEXT_HEIGHT][TEXT_WIDTH];
static struct CURSOR_POSITION stack_text_cursor[MAXSCREENS];

void stackscreen(void)
{
  int i, r, c;
#if 0
  char msg[40];
  sprintf(msg, "stackscreen, %i screens stacked\n", screenctr+1);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           "Stackscreen Information",
                           msg,
                           NULL);
#endif

  /* video image displayed when screenctr == -1 */
   if(*s_makepar == 0)
      return;

  if (++screenctr) /* already have some stacked */
    {
      static char msg[]={"stackscreen overflow"};
      if ((i = screenctr - 1) >= MAXSCREENS) { /* bug, missing unstack? */
         stopmsg(1,msg);
         exit(1);
         }
      for (r = 0; r < TEXT_HEIGHT; r++)
        for (c = 0; c < TEXT_WIDTH; c++)
          {
            stack_text_screen[i][r][c] = text_screen[r][c];
            stack_text_attr[i][r][c] = text_attr[r][c];
          }
      stack_text_cursor[i].row = textrow;
      stack_text_cursor[i].col = textcol;
      setclear();
    }
   else
    {
      /* don't do anything since screenctr != -1 and text will be displayed */
    }
}

void unstackscreen(void)
{
  int r, c;
#if 0
  char msg[40];
  sprintf(msg, "unstackscreen, %i screens stacked\n", screenctr);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           "Stackscreen Information",
                           msg,
                           NULL);
#endif
   if(*s_makepar == 0)
      return;

  if (--screenctr >= 0) /* unstack */
    {
      for (r = 0; r < TEXT_HEIGHT; r++)
        {
          for (c = 0; c < TEXT_WIDTH; c++)
            {
              text_screen[r][c] = stack_text_screen[screenctr][r][c];
              text_attr[r][c] = stack_text_attr[screenctr][r][c];
              outtext(r, c, c+1); /* restore a character at a time */
            }
        }
      textrow = stack_text_cursor[screenctr].row;
      textcol = stack_text_cursor[screenctr].col;
    }
  else /* video image displayed when screenctr == -1 */
    {
      screenctr = -1;
      setclear();
    }
}

void discardscreen(void)
{
#if 0
  char msg[40];
  sprintf(msg, "discardscreen, %i screens stacked\n", screenctr);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           "Stackscreen Information",
                           msg,
                           NULL);
#endif
   if(*s_makepar == 0)
      return;

  screenctr = -1;   /* unstack all */
  setclear();      /* clear text arrays */
  startvideo();    /* clear video */
}

void putstring (int row, int col, int attr, CHAR *msg)
{
  int r, c, s_c;
  int max_c = 0;
#if 0
  fprintf(stderr, "putstring, %s\n", msg);
/* {
      char errortxt[80];
      sprintf(errortxt, "%s", msg);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "PutString",
                         errortxt,
                         NULL);
} */
#endif

  if (row != -1)
    textrow = row;
  if (col != -1)
    textcol = col;

  r = textrow + textrbase;
  s_c = c = textcol + textcbase;

  while (*msg)
    {
      if (*msg == '\n' || c == TEXT_WIDTH)
        {
          if (c > max_c)
            max_c = c;
          /* output line here, get ready for the next one */
          outtext(r, s_c, max_c);
          textrow++;
          r++;
          textcol = 0;
          c = s_c;
        }
      else
        {
#if 1
          if (c >= TEXT_WIDTH) c = TEXT_WIDTH - 1; /* keep going, but truncate */
          if (r >= TEXT_HEIGHT) r = TEXT_HEIGHT - 1;
#endif
          assert(r < TEXT_HEIGHT);
          assert(c < TEXT_WIDTH);
          text_screen[r][c] = *msg;
          text_attr[r][c] = attr;
          textcol++;
          c++;
        }
      msg++;
    }

  if (c > max_c)
    max_c = c;

  /* output the last line here */
  if (max_c - s_c > 0)
    outtext(r, s_c, max_c);
  return;
}

/*
; setattr(row, col, attr, count) where
;         row, col = row and column to start printing.
;         attr = color attribute.
;         count = number of characters to set
;         This routine works only in real color text mode.
*/
void setattr (int row, int col, int attr, int count)
{
  int i = col;
  int r, c, s_c;
#if 0
  fprintf(stderr, "setattr, row= %d col=%d attr=%d count=%d\n", row, col, attr, count);
#endif

  if (row != -1)
    textrow = row;
  if (col != -1)
    textcol = col;

  r = textrow + textrbase;
  s_c = c = textcol + textcbase;

  assert(count <= TEXT_WIDTH * TEXT_HEIGHT);
  if (count == 0)
    return;
  while (count)
    {
      assert(r < TEXT_HEIGHT);
      assert(i < TEXT_WIDTH);
      text_attr[r][i] = attr;
      c++;
      count--;
      if (++i == TEXT_WIDTH)
        {
          outtext(r, s_c, c);
          i = 0;
          r++;
          c = s_c;
        }
    }
  /* refresh text */
  outtext(r, s_c, c);
  return;
}

/*
; ************* Function scrollup(toprow, botrow) ******************

;       Scroll the screen up (from toprow to botrow)
*/
void scrollup (int top, int bot)
{
  int r, c;
#if 0
  fprintf(stderr, "scrollup(%d, %d)\n", top, bot);
#endif

  assert(bot < TEXT_HEIGHT);
  for (r = top; r < bot; r++)
    {
      for (c = 0; c < TEXT_WIDTH; c++)
        {
          text_attr[r][c] = text_attr[r+1][c];
          text_screen[r][c] = text_screen[r+1][c];
        }
      outtext(r, 0, c);
    }
  for (c = 0; c < TEXT_WIDTH; c++) /* clear bottom line */
    {
      text_attr[bot][c] = 0;
      text_screen[bot][c] = ' ';
    }
  outtext(bot, 0, c);
  /* bottom line is added by intro() */
}

void showcursor(int show)
{ /* hide cursor if show==0, show cursor if show==1 */
    if (show)
        SDL_ShowCursor(SDL_ENABLE);
    else
        SDL_ShowCursor(SDL_DISABLE);
}

static int translate_key(SDL_KeyboardEvent *key)
{
  int tmp = key->keysym.sym & 0x7f; /* 0 - 127 */

#if 0
  fprintf(stderr, "translate_key(%i): ''%c'' with mod %i\n",
          key->keysym.sym, key->keysym.sym, key->keysym.mod);
{
      char errortxt[80];
      sprintf(errortxt, "translate_key(%i): ''%c'' with mod %i", key->keysym.sym, key->keysym.sym, key->keysym.mod);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                         "TranslateKey",
                         errortxt,
                         NULL);
}
#endif

  /* This is the SDL key mapping */
  if (SDL_GetModState() & KMOD_CTRL) /* Control key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_F1:
         if (screenctr == -1)
          return CF1;
         break;
        case SDLK_F2:
         if (screenctr == -1)
          return CF2;
         break;
        case SDLK_F3:
         if (screenctr == -1)
          return CF3;
         break;
        case SDLK_F4:
         if (screenctr == -1)
          return CF4;
         break;
        case SDLK_F5:
         if (screenctr == -1)
          return CF5;
         break;
        case SDLK_F6:
         if (screenctr == -1)
          return CF6;
         break;
        case SDLK_F7:
         if (screenctr == -1)
          return CF7;
         break;
        case SDLK_F8:
         if (screenctr == -1)
          return CF8;
         break;
        case SDLK_F9:
         if (screenctr == -1)
          return CF9;
         break;
        case SDLK_F10:
         if (screenctr == -1)
          return CF10;
         break;
        case SDLK_TAB:
          return CTL_TAB;
        case SDLK_BACKSLASH:
          return CTL_BACKSLASH;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
          return CTL_MINUS;
        case SDLK_EQUALS: /* pretend shift is pressed */
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
          return CTL_PLUS;
        case SDLK_RETURN:
          return CTL_ENTER;
        case SDLK_INSERT:
          return CTL_INSERT;
        case SDLK_DELETE:
          return CTL_DEL;
        case SDLK_HOME:
          return CTL_HOME;
        case SDLK_END:
          return CTL_END;
        case SDLK_PAGEUP:
          return CTL_PAGE_UP;
        case SDLK_PAGEDOWN:
          return CTL_PAGE_DOWN;
        case SDLK_LEFT:
          return LEFT_ARROW_2;
        case SDLK_RIGHT:
          return RIGHT_ARROW_2;
        case SDLK_UP:
          return UP_ARROW_2;
        case SDLK_DOWN:
          return DOWN_ARROW_2;
        case SDLK_KP_ENTER:
          return CTL_ENTER_2;
        default:
          if (tmp >= 'a' && tmp <= 'z')
            return (tmp - 'a' + 1);
        }
    }
  else if (SDL_GetModState() & KMOD_SHIFT) /* Shift key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_BACKQUOTE:
          return '~';
        case SDLK_0:
          return SDLK_RIGHTPAREN;
        case SDLK_1:
          return SDLK_EXCLAIM;
        case SDLK_2:
          return SDLK_AT;
        case SDLK_3:
          return SDLK_HASH;
        case SDLK_4:
          return SDLK_DOLLAR;
        case SDLK_5:
          return '%';
        case SDLK_6:
          return SDLK_CARET;
        case SDLK_7:
          return SDLK_AMPERSAND;
        case SDLK_8:
          return SDLK_ASTERISK;
        case SDLK_9:
          return SDLK_LEFTPAREN;
        case SDLK_MINUS:
          return SDLK_UNDERSCORE;
        case SDLK_EQUALS:
          return SDLK_PLUS;
        case SDLK_LEFTBRACKET:
          return '{';
        case SDLK_RIGHTBRACKET:
          return '}';
        case SDLK_BACKSLASH:
          return '|';
        case SDLK_SEMICOLON:
          return SDLK_COLON;
        case SDLK_QUOTE:
          return SDLK_QUOTEDBL;
        case SDLK_COMMA:
          return SDLK_LESS;
        case SDLK_PERIOD:
          return SDLK_GREATER;
        case SDLK_SLASH:
          return SDLK_QUESTION;
        case SDLK_F1:
         if (screenctr == -1)
          return SF1;
         break;
        case SDLK_F2:
         if (screenctr == -1)
          return SF2;
         break;
        case SDLK_F3:
         if (screenctr == -1)
          return SF3;
         break;
        case SDLK_F4:
         if (screenctr == -1)
          return SF4;
         break;
        case SDLK_F5:
         if (screenctr == -1)
          return SF5;
         break;
        case SDLK_F6:
         if (screenctr == -1)
          return SF6;
         break;
        case SDLK_F7:
         if (screenctr == -1)
          return SF7;
         break;
        case SDLK_F8:
         if (screenctr == -1)
          return SF8;
         break;
        case SDLK_F9:
         if (screenctr == -1)
          return SF9;
         break;
        case SDLK_F10:
         if (screenctr == -1)
          return SF10;
         break;
        case SDLK_TAB:
          return BACK_TAB;
        case SDLK_a:
        case SDLK_b:
        case SDLK_c:
        case SDLK_d:
        case SDLK_e:
        case SDLK_f:
        case SDLK_g:
        case SDLK_h:
        case SDLK_i:
        case SDLK_j:
        case SDLK_k:
        case SDLK_l:
        case SDLK_m:
        case SDLK_n:
        case SDLK_o:
        case SDLK_p:
        case SDLK_q:
        case SDLK_r:
        case SDLK_s:
        case SDLK_t:
        case SDLK_u:
        case SDLK_v:
        case SDLK_w:
        case SDLK_x:
        case SDLK_y:
        case SDLK_z:
          return (tmp - ('a' - 'A'));
        default:
          break;
        }
    }
  else if (SDL_GetModState() & KMOD_ALT) /* Alt key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_F1:
         if (screenctr == -1)
          return AF1;
         break;
        case SDLK_F2:
         if (screenctr == -1)
          return AF2;
         break;
        case SDLK_F3:
         if (screenctr == -1)
          return AF3;
         break;
        case SDLK_F4:
         if (screenctr == -1)
          return AF4;
         break;
        case SDLK_F5:
         if (screenctr == -1)
          return AF5;
         break;
        case SDLK_F6:
         if (screenctr == -1)
          return AF6;
         break;
        case SDLK_F7:
         if (screenctr == -1)
          return AF7;
         break;
        case SDLK_F8:
         if (screenctr == -1)
          return AF8;
         break;
        case SDLK_F9:
         if (screenctr == -1)
          return AF9;
         break;
        case SDLK_F10:
         if (screenctr == -1)
          return AF10;
         break;
        case SDLK_TAB:
          return ALT_TAB;
        case SDLK_a:
          return ALT_A;
        case SDLK_s:
          return ALT_S;
        case SDLK_1:
          return ALT_1;
        case SDLK_2:
          return ALT_2;
        case SDLK_3:
          return ALT_3;
        case SDLK_4:
          return ALT_4;
        case SDLK_5:
          return ALT_5;
        case SDLK_6:
          return ALT_6;
        case SDLK_7:
          return ALT_7;
        default:
          break;
        }
    }
  else if (SDL_GetModState() & KMOD_NUM) /* Num Lock key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_KP_0:
          return '0';
        case SDLK_KP_1:
          return '1';
        case SDLK_KP_2:
          return '2';
        case SDLK_KP_3:
          return '3';
        case SDLK_KP_4:
          return '4';
        case SDLK_KP_5:
          return '5';
        case SDLK_KP_6:
          return '6';
        case SDLK_KP_7:
          return '7';
        case SDLK_KP_8:
          return '8';
        case SDLK_KP_9:
          return '9';
        case SDLK_KP_PERIOD:
          return '.';
        case SDLK_KP_PLUS:
          return '+';
        case SDLK_KP_MINUS:
          return '-';
        case SDLK_KP_MULTIPLY:
          return '*';
        case SDLK_KP_DIVIDE:
          return '/';
        default:
          break;
        }
    }
  /* No modifier key down */
  switch (key->keysym.sym)
    {
    case SDLK_F1:
      return F1;
    case SDLK_F2:
     return F2;
     break;
    case SDLK_F3:
     if (screenctr == -1)
      return F3;
     break;
    case SDLK_F4:
     return F4;
     break;
    case SDLK_F5:
     return F5;
     break;
    case SDLK_F6:
     return F6;
     break;
    case SDLK_F7:
     return F7;
     break;
    case SDLK_F8:
     if (screenctr == -1)
      return F8;
     break;
    case SDLK_F9:
     if (screenctr == -1)
      return F9;
     break;
    case SDLK_F10:
     if (screenctr == -1)
      return F10;
     break;
    case SDLK_INSERT:
      return INSERT;
    case SDLK_DELETE:
      return DELETE;
    case SDLK_HOME:
      return HOME;
    case SDLK_END:
      return END;
    case SDLK_PAGEUP:
      return PAGE_UP;
    case SDLK_PAGEDOWN:
      return PAGE_DOWN;
    case SDLK_LEFT:
      return LEFT_ARROW;
    case SDLK_RIGHT:
      return RIGHT_ARROW;
    case SDLK_UP:
      return UP_ARROW;
    case SDLK_DOWN:
      return DOWN_ARROW;
    case SDLK_ESCAPE:
      return ESC;
    case SDLK_KP_ENTER:
      return ENTER_2;
    case SDLK_RETURN:
      return ENTER;
    case SDLK_KP_PLUS:
      return '+';
    case SDLK_KP_MINUS:
      return '-';
    default:
      break;
    }

  if (key->keysym.sym <= 127)
    return tmp;
  else
    return 0;
}

static int lastx = 0;
static int lasty = 0;
int left_mouse_button_down = 0;
int right_mouse_button_down = 0;
int button_held = 0;
extern int inside_help;
extern int editpal_cursor;
#define ABS(x) ((x) > 0?(x):-(x))
#define MIN(x, y) ((x) < (y)?(x):(y))

int check_mouse(SDL_Event mevent)
{
  static int dx = 0;
  static int dy = 0;
  static int bandx0, bandy0;
  static int bandx1, bandy1;
  static int banding = 0;
  int bnum = 0;
  int keyispressed = 0;

  if (screenctr != -1) /* don't do it, we're on a text screen */
    return (0);

  if (lookatmouse == 0)
    return (0);

  if (editpal_cursor && !inside_help)
  {
    if (mevent.button.state & SDL_BUTTON_MMASK ||
        (mevent.button.state & SDL_BUTTON_LMASK &&
         mevent.button.state & SDL_BUTTON_RMASK))
       bnum = 3;
    else if (mevent.button.state & SDL_BUTTON_LMASK)
       bnum = 1;
    else if (mevent.button.state & SDL_BUTTON_RMASK)
       bnum = 2;
    else
       bnum = 0;

    if (lookatmouse == 3 && bnum != 0)
    {
      dx += (mevent.button.x * mouse_scale_x - lastx);
      dy += (mevent.button.y * mouse_scale_y - lasty);
      lastx = mevent.button.x * mouse_scale_x;
      lasty = mevent.button.y * mouse_scale_y;
    }
    else
    {
      Cursor_SetPos(mevent.button.x * mouse_scale_x, mevent.button.y * mouse_scale_y);
      keyispressed = ENTER;
    }
  }

    if (!keyispressed && editpal_cursor && !inside_help && lookatmouse == 3 &&
    (dx != 0 || dy != 0))
    {
        if (ABS(dx)>ABS(dy))
        {
            if (dx>0)
            {
                keyispressed = mousefkey[bnum][0]; /* right */
                dx--;
            }
            else if (dx<0)
            {
                keyispressed = mousefkey[bnum][1]; /* left */
                dx++;
            }
        }
        else
        {
            if (dy>0)
            {
                keyispressed = mousefkey[bnum][2]; /* down */
                dy--;
            }
            else if (dy<0)
            {
                keyispressed = mousefkey[bnum][3]; /* up */
                dy++;
            }
        }
        return (keyispressed);
    }

  if (left_mouse_button_down && !button_held)
    {
      if (zoomoff == 1)  /* zooming is allowed */
        {
          if (zwidth == 0.0)  /* haven't started zooming yet */
            {
              /* start zoombox */
              zskew = zrotate = 0;
              button_held = 1;
              banding = 0;
              lastx = bandx0 = mevent.button.x * mouse_scale_x;
              lasty = bandy0 = mevent.button.y * mouse_scale_y;
              find_special_colors();
              boxcolor = color_bright;
            }
        }
    }

  if (left_mouse_button_down && button_held && !banding)
    {
       bandx1 = mevent.button.x * mouse_scale_x;
       bandy1 = mevent.button.y * mouse_scale_y;
       if (ABS(bandx1 - bandx0) > 5 || ABS(bandy1 - bandy0) > 5)
        {
          banding = 1;
        }
    }

  if(left_mouse_button_down && button_held && banding)
    {
     /* get current mouse position */
     bandx1 = mevent.button.x * mouse_scale_x;
     bandy1 = mevent.button.y * mouse_scale_y;

     /* sanity checks */
     if (bandx1 == bandx0)
        bandx1 = bandx0 + 1;
     if (bandy1 == bandy0)
        bandy1 = bandy0 + 1;

      /* Get the mouse offset */
      dx = ABS(bandx1 - bandx0);

      /* (zbx,zby) is upper left corner of zoom box */
      /* zwidth & zdepth are deltas to lower right corner */
      zbx = (MIN(bandx0, bandx1) - sxoffs) / dxsize;
      zby = (MIN(bandy0, bandy1) - syoffs) / dysize;
      zwidth = dx / dxsize;
      zdepth = dx * finalaspectratio / dysize; /* maintain aspect ratio here */

      chgboxi(zbx,zby);
      drawbox(1);
    }

  if (right_mouse_button_down)
    (*plot)((mevent.button.x * mouse_scale_x) - sxoffs, (mevent.button.y * mouse_scale_y) - syoffs, 4);
  return (keyispressed);
}

int get_key_event(int block)
{
  SDL_Event event;
  int keyispressed = 0;

  if(screenctr != -1 || editpal_cursor) /* On a text screen or palette editor: */
    SDL_ShowCursor(SDL_DISABLE); /*   Hide the mouse cursor */
  else                           /* On an image screen: */
    SDL_ShowCursor(SDL_ENABLE);  /*   Show the mouse cursor */

  do
    {
      /* look for an event */
      while ( SDL_PollEvent( &event ) != 0 )
        {
          /* an event was found */
          switch (event.type)
            {
             case (SDL_WINDOWEVENT):
              {

            switch (event.window.event) {
               case SDL_WINDOWEVENT_RESIZED:
                 window_is_fullscreen = 0;
                 set_mouse_scale();
                 break;
               case SDL_WINDOWEVENT_EXPOSED:
               case SDL_WINDOWEVENT_SHOWN:
                 updatewindow = 1;
                 break;
               case SDL_WINDOWEVENT_RESTORED:
                 window_is_fullscreen = 0;
                 set_mouse_scale();
                 break;
               case SDL_WINDOWEVENT_MAXIMIZED:
                 window_is_fullscreen = 1;
                 set_mouse_scale();
                 break;
               case SDL_WINDOWEVENT_HIDDEN:
               case SDL_WINDOWEVENT_MINIMIZED:
                 updatewindow = 0;
                 break;
               case SDL_WINDOWEVENT_CLOSE:
                 x_close = 1;
                 goodbye();
                 break;
               default:
                 break;
               }

              }
              break;
            case SDL_MOUSEMOTION:
              keyispressed = check_mouse(event);
              break;
            case SDL_MOUSEBUTTONDOWN:
             if (screenctr == -1)
              {
              if (lookatmouse == 3 || zoomoff == 0)
                {
                  lastx = event.button.x * mouse_scale_x;
                  lasty = event.button.y * mouse_scale_y;
                  break;
                }
              if (event.button.button == SDL_BUTTON_LEFT && left_mouse_button_down == 1)
                button_held = 1;
              if (event.button.button == SDL_BUTTON_LEFT)
                left_mouse_button_down = 1;
              if (event.button.button == SDL_BUTTON_RIGHT)
                right_mouse_button_down = 1;
              }
              break;
            case SDL_MOUSEBUTTONUP:
             if (screenctr == -1)
              {
              if (event.button.button == SDL_BUTTON_LEFT)
                {
                if (left_mouse_button_down == 1 && button_held == 1)
                   {
/*                      init_pan_or_recalc(0); */
                      if(SDL_GetModState() & KMOD_CTRL) /* Control key down */
                        keyispressed = ENTER;
                   }
                left_mouse_button_down = 0;
                button_held = 0;
                }
              if (event.button.button == SDL_BUTTON_RIGHT)
                right_mouse_button_down = 0;
              }
              break;
            case SDL_KEYDOWN:
              keyispressed = translate_key(&event.key);
              break;
            case SDL_QUIT:
              goodbye();
              break;
            default:
              break;
            }
        }
  if (checkautosave())
      keyispressed = 9999; /* time to save, exit */
  if (time_to_update() && updatewindow)
    {
      updateimage();
    }
   }
  while (block && !keyispressed);
  if (time_to_update() && updatewindow)
    {
      updateimage();
    }
  return (keyispressed);
}

/*
; ***************** Function delay(int delaytime) ************************
;
;       performs a delay of 'delaytime' milliseconds
*/
void delay(int delaytime)
{
  SDL_Delay(delaytime);
}

/*
 *----------------------------------------------------------------------
 *
 * clock_ticks --
 *
 *      Return time in CLK_TCK ticks.
 *
 * Results:
 *      Time.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
long clock_ticks(void)
{
  return(SDL_GetTicks64());
}


#define TICK_INTERVAL    50
#define TICK_INTERVAL2   100
#define BLINK_INTERVAL   200
#define BF_MATH_INTERVAL 500

static U64 next_time = 0;

int time_to_update(void)
{
  /* return a 1 every 50 milliseconds if (calculating && !bf_math) or using_jiim */
  /* return a 1 every 100  milliseconds if mousing or saving/restoring image */
  /* return a 1 every 200 milliseconds if on text screen and blink cursor */
  /* return a 1 every 500 milliseconds if calculating && bf_math */

  U64 now;

  if (calc_status == 1 && show_orbit && soundflag&6 && !taborhelp)
    return (0); /* we will update the image at each pixel */

  now = SDL_GetTicks64();
  if ((calc_status == 1 && !bf_math) || using_jiim || fractype == ANT)
    /* calculating or using_jiim or fractype == ANT */
    {
      if (next_time <= now)
        {
          next_time = SDL_GetTicks64() + TICK_INTERVAL;
          return (1);
        }
      else
        return (0);
    }
  else if (calc_status == 1 && bf_math) /* calculating && bf_math */
    {
      if (next_time <= now)
        {
          next_time = SDL_GetTicks64() + BF_MATH_INTERVAL;
          return (1);
        }
      else
        return (0);
    }
  else if (button_held || file_IO)
    {
      if (next_time <= now)
        {
          next_time = SDL_GetTicks64() + TICK_INTERVAL2;
          return (1);
        }
      else
        return (0);
    }
  else if (screenctr != -1)
    {
      if (next_time <= now)
        {
          blink_cursor();
          next_time = SDL_GetTicks64() + BLINK_INTERVAL;
          return (1);
        }
      else
        return (0);
    }
  else
    {
    SDL_Delay(10);
    return (1);
    }
}
