/* Put all of the routines that call SDL functions in this module */

#include <stdlib.h>
#include <assert.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "port.h"
#include "prototyp.h"

/* SDL global variables */
SDL_Surface *screen = NULL;
SDL_Surface *backscrn = NULL;
SDL_Surface *textbkgd = NULL;
SDL_Surface *textmsg = NULL;
TTF_Font *font = NULL;
// NOTE (jonathan#1#): Does next need to be global to catch events????
//SDL_Event event;

enum
{
  TEXT_WIDTH = 80,
  TEXT_HEIGHT = 25,
  MOUSE_SCALE = 1
};
// FIXME (jonathan#1#): This will need to change since SDL uses a separate variable for the modifier keys.
static int mousefkey[4][4] /* [button][dir] */ =
{
  {SDLK_RIGHT,SDLK_LEFT,SDLK_DOWN,SDLK_UP},
  {0,0,SDLK_PAGEDOWN,SDLK_PAGEUP},
  {CTL_PLUS,CTL_MINUS,CTL_DEL,CTL_INSERT},
  {CTL_END,CTL_HOME,CTL_PAGE_DOWN,CTL_PAGE_UP}
};

int txt_ht;  /* text letter height = 2^txt_ht pixels */
int txt_wt;  /* text letter width = 2^txt_wt pixels */

char text_screen[TEXT_HEIGHT][TEXT_WIDTH];
int  text_attr[TEXT_HEIGHT][TEXT_WIDTH];
char stack_text_screen[TEXT_HEIGHT][TEXT_WIDTH];
int  stack_text_attr[TEXT_HEIGHT][TEXT_WIDTH];

void apply_surface( int, int, SDL_Surface*);
void puttruecolor_SDL(SDL_Surface*, int, int, Uint8, Uint8, Uint8);


void Slock(void)
{
  if ( SDL_MUSTLOCK(screen) )
    {
      if ( SDL_LockSurface(screen) < 0 )
        {
          return;
        }
    }
}

void Sulock(void)
{
  if ( SDL_MUSTLOCK(screen) )
    {
      SDL_UnlockSurface(screen);
    }
}

void CleanupSDL(void)
{
  /*
   * Quit SDL so we can release the fullscreen
   * mode and restore the previous video settings,
   * etc.  Called by goodbye() routine.
   */
  SDL_FreeSurface(backscrn);
  SDL_FreeSurface(textbkgd);
  SDL_FreeSurface(textmsg);
// NOTE (jonathan#1#): May not need this once png support is added.
  IMG_Quit();

  TTF_CloseFont(font);
  font = NULL;
  TTF_Quit();

  SDL_Quit( );

}

void SetupSDL(void)
{
  /* called by main() routine */
  Uint32 rmask, gmask, bmask, amask;
  int bpp; /* bits per pixel for graphics mode */

  if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0 )
    {
      fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
      exit(1);
    }

// NOTE (jonathan#1#): May not need this once png support is added.
  if ( IMG_Init(IMG_INIT_PNG) < 0 )
    {
      fprintf(stderr, "Unable to init IMG: %s\n", SDL_GetError());
      exit(1);
    }

  if (TTF_Init() < 0)
    {
      fprintf(stderr, "Unable to init TTF: %s\n", SDL_GetError());
      exit(1);
    }

  /*
   * Initialize the display in a 800x600 best available bit mode,
   * requesting a hardware surface and double buffering.
   * Failing to initialize that then falls back to
   * Initialize the display in a 800x600 16-bit mode,
   * requesting a software surface
   */
  xdots = 800;
  ydots = 600;
// FIXME (jonathan#1#): Need to work out changing window size.
//  screen = SDL_SetVideoMode(xdots, ydots, 0, SDL_HWSURFACE|SDL_DOUBLEBUF);
  screen = SDL_SetVideoMode(xdots, ydots, 8, SDL_HWSURFACE|SDL_DOUBLEBUF);
  if (screen == NULL )
    {
      screen = SDL_SetVideoMode(xdots, ydots, 16, SDL_SWSURFACE|SDL_ANYFORMAT);
      /* need flags for no double buffering */
    }
  if ( screen == NULL )
    {
      fprintf(stderr, "Couldn't set %dx%dx16 video mode: %s\n", xdots, ydots,
              SDL_GetError());
      exit(1);
    }
  bpp=screen->format->BitsPerPixel;

#if DEBUG
  printf("Set %dx%d at %d bits-per-pixel mode\n", xdots, ydots, bpp);
#endif

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
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

  backscrn = SDL_CreateRGBSurface(SDL_SWSURFACE, xdots, ydots, bpp,
                                  rmask, gmask, bmask, amask);
  textmsg = SDL_CreateRGBSurface(SDL_SWSURFACE, xdots, ydots, bpp,
                                 rmask, gmask, bmask, amask);
  textbkgd = SDL_CreateRGBSurface(SDL_SWSURFACE, xdots, ydots, bpp,
                                  rmask, gmask, bmask, amask);

  font = TTF_OpenFont("crystal.ttf", 12);
  if ( font == NULL )
    {
      fprintf(stderr, "Couldn't set font: %s\n", SDL_GetError());
      exit(1);
    }

  /* get text height and width in pixels */
  TTF_SizeUTF8(font,"H",&txt_wt,&txt_ht);

  SDL_WM_SetCaption( "Fractint", NULL );
  SDL_WM_SetIcon(SDL_LoadBMP("fractint.ico"), NULL);
  SDL_EnableKeyRepeat(250,30);

}

void startvideo(void)
{
  int Bpp = screen->format->BytesPerPixel;

  if (Bpp == 1) /* 8-bits per pixel => uses a palette */
    {
      gotrealdac = 1;
      istruecolor = 0;
      colors = 256;
    }
  else /* truecolor modes */
    {
      gotrealdac = 0;
      istruecolor = 1;
// FIXME (jonathan#1#): Need to have more colors for truecolor modes
      colors = 256;
    }

  /* initialize screen and backscrn surfaces to inside color */
  SDL_FillRect(screen, NULL, map_to_pixel(inside));
  SDL_FillRect(backscrn, NULL, map_to_pixel(inside));
}

void setclear (void)
{
  /* clears the screen to the text background */
  apply_surface(0, 0, textbkgd);
}

void starttext(void)
{
  /* initialize textbkgd and textmsg surfaces to black */
  SDL_FillRect(textbkgd, NULL, map_to_pixel(WHITE));
  SDL_FillRect(textmsg, NULL, map_to_pixel(WHITE));
}

U32 map_to_pixel(BYTE color)
{
  /* returns the pixel value corresponding to the truemode selection */
  BYTE red, green, blue;

  dac_to_rgb(color, &red, &green, &blue);
  return(SDL_MapRGB(screen->format, red, green, blue));
}

/*
 * Return the pixel value at (x, y)
 * NOTE: The surface must be locked before calling this!
 * Try reading from backscrn, which should match screen
 */
BYTE readvideo(int x, int y)
{
  int Bpp = screen->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to retrieve */
  Uint8 *p = (Uint8 *)screen->pixels + y * screen->pitch + x * Bpp;
// FIXME (jonathan#1#): Read data from buffer?
  Slock();
  switch (Bpp)
    {
    case 1:
      return *p;

    case 2:
      return *(Uint16 *)p;

    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        return p[0] << 16 | p[1] << 8 | p[2];
      else
        return p[0] | p[1] << 8 | p[2] << 16;

    case 4:
      return *(U32 *)p;

    default:
      return 0;       /* shouldn't happen, but avoids warnings */
    }
  Sulock();
}

void gettruecolor_SDL(SDL_Surface *screen, int x, int y, BYTE *red, BYTE *green, BYTE *blue)
{
  /* Extracting color components from a 32-bit color value */
  SDL_PixelFormat *fmt;
  Uint32 pixel;

  fmt = screen->format;
  SDL_LockSurface(screen);
  pixel = *((Uint32*)screen->pixels);
  SDL_UnlockSurface(screen);

  SDL_GetRGB(pixel, fmt, (Uint8 *)red, (Uint8 *)green, (Uint8 *)blue);
}

void gettruecolor(int x, int y, BYTE R, BYTE G, BYTE B)
{
  gettruecolor_SDL(screen, x, y, &R, &G, &B);
}


/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 */
void writevideo(int x, int y, U32 pixel)
{
  int Bpp = screen->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to set */
  Uint8 *p = (Uint8 *)screen->pixels + y * screen->pitch + x * Bpp;
// FIXME (jonathan#1#): write data to buffer also?
  Slock();
  switch (Bpp)
    {
    case 1:
      *p = pixel;
      break;

    case 2:
      *(Uint16 *)p = pixel;
      break;

    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        {
          p[0] = (pixel >> 16) & 0xff;
          p[1] = (pixel >> 8) & 0xff;
          p[2] = pixel & 0xff;
        }
      else
        {
          p[0] = pixel & 0xff;
          p[1] = (pixel >> 8) & 0xff;
          p[2] = (pixel >> 16) & 0xff;
        }
      break;

    case 4:
      *(U32 *)p = pixel;
      break;
    }
  Sulock();
}

void puttruecolor_SDL(SDL_Surface *screen, int x, int y, Uint8 R, Uint8 G, Uint8 B)
{
  Uint32 color = SDL_MapRGB(screen->format, R, G, B);

  Slock();
  switch (screen->format->BytesPerPixel)
    {
    case 1: // Assuming 8-bpp
    {
      Uint8 *bufp;
      bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
      *bufp = color;
    }
    break;
    case 2: // Probably 15-bpp or 16-bpp
    {
      Uint16 *bufp;
      bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
      *bufp = color;
    }
    break;
    case 3: // Slow 24-bpp mode, usually not used
    {
      Uint8 *bufp;
      bufp = (Uint8 *)screen->pixels + y*screen->pitch + x * 3;
      if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
        {
          bufp[0] = color;
          bufp[1] = color >> 8;
          bufp[2] = color >> 16;
        }
      else
        {
          bufp[2] = color;
          bufp[1] = color >> 8;
          bufp[0] = color >> 16;
        }
    }
    break;
    case 4: // Probably 32-bpp
    {
      Uint32 *bufp;
      bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
      *bufp = color;
    }
    break;
    }
  Sulock();
}

void puttruecolor(int x, int y, BYTE R, BYTE G, BYTE B)
{
  puttruecolor_SDL(screen, x, y, (Uint8)R, (Uint8)G, (Uint8)B);
}

void apply_surface( int x, int y, SDL_Surface* source)
{
  SDL_Rect offset;

  offset.x = x;
  offset.y = y;
  SDL_BlitSurface( source, NULL, screen, &offset );
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

  width = lastx-x+1;
  for (i=0;i<width;i++)
    {
      writevideo(x+i, y, (U32)pixels[i]);
    }
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
  for (i=0;i<width;i++)
    {
      pixels[i] = (BYTE)readvideo(x+i,y);
    }
}

SDL_Color cols[256];

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
  int i;
  for (i=0;i<colors;i++)
    {
      dacbox[i][0] = cols[i].r >> 2;
      dacbox[i][1] = cols[i].g >> 2;
      dacbox[i][2] = cols[i].b >> 2;
    }
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
int writevideopalette(void)
{
  int i;

  for (i = 0; i < colors; i++)
    {
      cols[i].r = dacbox[i][0] << 2;
      cols[i].g = dacbox[i][1] << 2;
      cols[i].b = dacbox[i][2] << 2;
    }
  /* Set palette */
  SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, cols, 0, 256);
  SDL_Flip(screen); /* not needed? */
}

/*
 * The stackscreen()/unstackscreen() functions were originally
 * ported to Xfractint.
 * These functions are useful for switching between different text screens.
 * For example, looking at a parameter entry using F2.
 */

int screenctr = 0;
// NOTE (jonathan#1#): Don't need next.  Never checked.
#define MAXSCREENS 3
// May need something if two text screens isn't enough
//static BYTE *savescreen[MAXSCREENS];
//static int saverc[MAXSCREENS+1];

void stackscreen(void)
{
  int r, c;
#if DEBUG
  fprintf(stderr, "stackscreen, %i screens stacked\n", screenctr+1);
#endif
  /* since we double buffer,  */
  /* no need to clear the screen, the text routines do it */
  if (screenctr == 0)
    {
    SDL_BlitSurface( screen, NULL, backscrn, NULL ); /* save screen */
    SDL_BlitSurface( textbkgd, NULL, screen, NULL );
    }
  if (screenctr > 0)
    {
      for (r = 0; r < TEXT_HEIGHT; r++)
        for (c = 0; c < TEXT_WIDTH; c++)
          {
            stack_text_screen[r][c] = text_screen[r][c];
            stack_text_attr[r][c] = text_attr[r][c];
          }
// FIXME (jonathan#1#): Put text in textmsg then blit to screen
//    blit(txt,stack_txt,0,0,0,0,TEXT_WIDTH<<txt_wt,TEXT_HEIGHT<<txt_ht);
// but only the first time???
      SDL_BlitSurface( screen, NULL, textmsg, NULL ); /* save text */

    }
  screenctr++;
}

void unstackscreen(void)
{
  int r, c;
#if DEBUG
  fprintf(stderr, "unstackscreen, %i screens stacked\n", screenctr);
#endif
  if (screenctr > 1)
    {
// FIXME (jonathan#1#): blit textbkgd to screen
//    set_palette(default_palette);
      for (r = 0; r < TEXT_HEIGHT; r++)
        for (c = 0; c < TEXT_WIDTH; c++)
          {
            text_screen[r][c] = stack_text_screen[r][c];
            text_attr[r][c] = stack_text_attr[r][c];
          }
// FIXME (jonathan#1#): Put text in textmsg then blit to screen
//    blit(txt,screen,0,0,0,0,TEXT_WIDTH<<txt_wt,TEXT_HEIGHT<<txt_ht);
    }
  screenctr--;
  if (screenctr == 0)
    {
    SDL_BlitSurface( backscrn, NULL, screen, NULL ); /* restore screen */
    SDL_Flip(screen); // doesn't appear to be needed
    }
}

void discardscreen(void)
{
#if DEBUG
  fprintf(stderr, "discardscreen, %i screens stacked\n", screenctr);
#endif
  screenctr = 0;   /* unstack all */
  SDL_BlitSurface( backscrn, NULL, screen, NULL ); /* restore screen */
  SDL_Flip(screen); // doesn't appear to be needed
}

void putstring (int row, int col, int attr, CHAR *msg)
{
#if DEBUG
  fprintf(stderr, "printstring, %s\n", msg);
#endif
  SDL_Color color = {0,0,0}, bgcolor = {0xff,0xff,0xff};
  SDL_Surface *text_surface;
  SDL_Rect text_rect;

  int r, c, i, k, s_r, s_c;
  int foregnd = attr & 15;
  int backgnd = (attr >> 4) & 15;
  int tmp_attr;
  int max_c = 0;

  if (row != -1)
    textrow = row;
  if (col != -1)
    textcol = col;

  if (attr & BRIGHT && !(attr & INVERSE))   /* bright */
    {
      foregnd += 8;
    }
  if (attr & INVERSE)   /* inverse video */
    {
// FIXME (jonathan#1#): How do we implement next????
//    text_mode(palette_color[foregnd]);
      tmp_attr = backgnd;
    }
  else
    {
//    text_mode(palette_color[backgnd]);
      tmp_attr = foregnd;
    }

  s_r = r = textrow + textrbase;
  s_c = c = textcol + textcbase;

text_rect.x = (textcol + textcbase) * txt_wt;
text_rect.y = (textrow + textrbase) * txt_ht;

if(!(text_surface=TTF_RenderText_Shaded(font,msg,color,bgcolor))) {
//handle error here, perhaps print TTF_GetError at least
} else {
SDL_BlitSurface(text_surface,NULL,screen,&text_rect);
//perhaps we can reuse it, but I assume not for simplicity.
SDL_FreeSurface(text_surface);
}


  while (*msg)
    {
      if (*msg == '\n')
        {
          textrow++;
          r++;
          if (c > max_c)
            max_c = c;
          textcol = 0;
          c = textcbase;
        }
      else
        {
#if DEBUG
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

  i = s_r*txt_ht; /* reuse i for blit */
  k = s_c*txt_wt;
  if (r == 0)
    r = 1;
  if (max_c > TEXT_WIDTH)
    max_c = TEXT_WIDTH;
  c = max_c - s_c;     /* reuse c for blit, now it's max width of msg */
  if (r > TEXT_HEIGHT - s_r)
    r = TEXT_HEIGHT - s_r;

//SDL_BlitSurface(textmsg,NULL,screen,&text_rect);

// FIXME (jonathan#1#): blit to screen here
// blit(txt,screen,k,i,k,i,c<<txt_wt,r<<txt_ht);
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
  int k;
  int r, c, s_r, s_c;
  int s_count = count;
  int foregnd = attr & 15;
  int backgnd = (attr >> 4) & 15;
  int tmp_attr;

  if (attr & BRIGHT && !(attr & INVERSE))   /* bright */
    {
      foregnd += 8;
    }
  if (attr & INVERSE)   /* inverse video */
    {
      // FIXME (jonathan#1#): How do we implement next????
//    text_mode(palette_color[foregnd]);
      tmp_attr = backgnd;
    }
  else
    {
// FIXME (jonathan#1#): How do we implement next????
//    text_mode(palette_color[backgnd]);
      tmp_attr = foregnd;
    }

  if (row != -1)
    textrow = row;
  if (col != -1)
    textcol = col;
  s_r = r = textrow + textrbase;
  s_c = c = textcol + textcbase;

  assert(count <= TEXT_WIDTH * TEXT_HEIGHT);
  while (count)
    {
      assert(r < TEXT_HEIGHT);
      assert(i < TEXT_WIDTH);
      text_attr[r][i] = attr;
      if (++i == TEXT_WIDTH)
        {
          i = 0;
          r++;
        }
      count--;
    }
  /* refresh text */
  if (r == 0)
    r = 1;
  if (r > TEXT_HEIGHT - s_r)
    r = TEXT_HEIGHT - s_r;
  if (s_count > TEXT_WIDTH - s_c)
    s_count = TEXT_WIDTH - s_c;
  i = s_r<<txt_ht; /* reuse i for blit, above i is col, now it's row */
  k = s_c<<txt_wt;

// FIXME (jonathan#1#): blit to screen here
// blit(txt,screen,k,i,k,i,s_count<<txt_wt,r<<txt_ht);
}




/* the following should return a value for success or failure */
void ShowBMP(char *filename)
{
  SDL_Surface* loadedImage = NULL;
  SDL_Surface* optimizedImage = NULL;

  loadedImage = SDL_LoadBMP( filename );
  if ( loadedImage != NULL )
    {
      optimizedImage = SDL_DisplayFormat( loadedImage );
      SDL_FreeSurface( loadedImage );
    }
  if ( optimizedImage != NULL )
    {
      apply_surface( 0, 0, optimizedImage);
      SDL_Flip( screen );
      SDL_FreeSurface( optimizedImage );
    }
}

void ShowGIF(char *filename)
{

  SDL_Surface* loadedImage = NULL;
  SDL_Surface* optimizedImage = NULL;
  SDL_RWops *rwop;

  rwop=SDL_RWFromFile(filename, "rb");
  loadedImage=IMG_LoadGIF_RW(rwop);
  if ( loadedImage != NULL )
    {
      optimizedImage = SDL_DisplayFormat( loadedImage );
      SDL_FreeSurface( loadedImage );
    }
  if ( optimizedImage != NULL )
    {
      apply_surface( 0, 0, optimizedImage);
      SDL_Flip( screen );
      SDL_FreeSurface( optimizedImage );
    }
}

static int translate_key(SDL_KeyboardEvent *key)
{
  int tmp = key->keysym.sym & 0xff;
#if DEBUG
  fprintf(stderr, "translate_key(%i): ``%c''\n", key->keysym.sym, key->keysym.sym);
#endif

  if (tmp >= 'a' && tmp <= 'z')
    return tmp;
  /* This is the SDL key mapping */
  else if (key->keysym.mod & KMOD_CTRL) /* Control key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_MINUS:
          return CTL_MINUS;
        case SDLK_PLUS:
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
        default:
          return tmp;
        }
    }
  else if (key->keysym.mod & KMOD_SHIFT) /* Shift key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_F1:
          return SF1;
        case SDLK_F2:
          return SF2;
        case SDLK_F3:
          return SF3;
        case SDLK_F4:
          return SF4;
        case SDLK_F5:
          return SF5;
        case SDLK_F6:
          return SF6;
        case SDLK_F7:
          return SF7;
        case SDLK_F8:
          return SF8;
        case SDLK_F9:
          return SF9;
        case SDLK_F10:
          return SF10;
        case SDLK_TAB:
          return BACK_TAB;
        default:
          return tmp;
        }
    }
  else  /* No modifier key down */
    {
      switch (key->keysym.sym)
        {
        case SDLK_F1:
          return F1;
        case SDLK_F2:
          return F2;
        case SDLK_F3:
          return F3;
        case SDLK_F4:
          return F4;
        case SDLK_F5:
          return F5;
        case SDLK_F6:
          return F6;
        case SDLK_F7:
          return F7;
        case SDLK_F8:
          return F8;
        case SDLK_F9:
          return F9;
        case SDLK_F10:
          return F10;
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
        case -2:
          return CTL_ENTER_2;
        default:
          return tmp;
        }
    }
  return tmp;
}

#if 0
int get_event(void)
{
  SDL_Event event;
  static int lastx,lasty;
  static int dx,dy;
  int keypressed = 0;

  /* look for an event */
  if ( SDL_PollEvent ( &event ) )
    {
      /* an event was found */
      switch (event.type)
        {
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT)
            {
              /* Get the mouse offsets */
              dx += (event.button.x - lastx) / MOUSE_SCALE;
              dy += (event.button.y - lasty) / MOUSE_SCALE;
              lastx = event.button.x;
              lasty = event.button.y;
              break;
            }
          if (event.button.button == SDL_BUTTON_RIGHT)
            {

            }
          break;
        case SDL_MOUSEBUTTONUP:
// same as above???
          break;

        case SDL_KEYDOWN:
          keypressed = event.key.keysym.sym;
          break;
        case SDL_QUIT:
          exit(0);
          break;
        default:
          break;



        }
    }

}




#endif

int get_key_event(int block)
{
  SDL_Event event;
  int keypressed = 0;

  do
    {
      /* look for an event */
      if ( SDL_PollEvent ( &event ) )
        {
          /* an event was found */
          switch (event.type)
            {
            case SDL_KEYDOWN:
              keypressed = translate_key(&event.key);
              break;
            case SDL_QUIT:
              exit(0);
              break;
            default:
              break;
            }
        }
// FIXME (jonathan#1#): Need to adjust this for bf math.
      /* time_to_update() should work outside of while loop, but doesn't */
      if (time_to_update()) /* set to 200 milli seconds, below */
        {
          SDL_Flip(screen);
        }
    }
  while (block && !keypressed);
  return (keypressed);
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
  return(SDL_GetTicks());
}


#define TICK_INTERVAL    200

static U32 next_time = 0;

int time_to_update(void)
{
  /* return a 1 every 200 milliseconds */
  U32 now;

  now = SDL_GetTicks();
  if (next_time <= now)
    {
      next_time = SDL_GetTicks() + TICK_INTERVAL;
      return (1);
    }
  else
    return (0);
}