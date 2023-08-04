/* generalasm.c
 * This file contains routines to replace general.asm.
 *
 * This file Copyright 1991 Ken Shirriff.  It may be used according to the
 * fractint license conditions, blah blah blah.
 */

//#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>

#include "port.h"
#include "prototyp.h"

int boxx[10000], boxy[10000];
int boxvalues[10000];
char tstack[4096];
BYTE olddacbox[DACSIZE][3];

/* fill with the default VGA color map */
BYTE dacbox[DACSIZE][3] = {
{  0,   0,   0},
{  0,   0, 168},
{  0, 168,   0},
{  0, 168, 168},
{168,   0,   0},
{168,   0, 168},
{168,  84,   0},
{168, 168, 168},
{ 84,  84,  84},
{ 84,  84, 252},
{ 84, 252,  84},
{ 84, 252, 252},
{252,  84,  84},
{252,  84, 252},
{252, 252,  84},
{252, 252, 252},
{  0,   0,   0},
{ 20,  20,  20},
{ 32,  32,  32},
{ 44,  44,  44},
{ 56,  56,  56},
{ 68,  68,  68},
{ 80,  80,  80},
{ 96,  96,  96},
{112, 112, 112},
{128, 128, 128},
{144, 144, 144},
{160, 160, 160},
{180, 180, 180},
{200, 200, 200},
{224, 224, 224},
{252, 252, 252},
{  0,   0, 252},
{ 64,   0, 252},
{124,   0, 252},
{188,   0, 252},
{252,   0, 252},
{252,   0, 188},
{252,   0, 124},
{252,   0,  64},
{252,   0,   0},
{252,  64,   0},
{252, 124,   0},
{252, 188,   0},
{252, 252,   0},
{188, 252,   0},
{124, 252,   0},
{ 64, 252,   0},
{  0, 252,   0},
{  0, 252,  64},
{  0, 252, 124},
{  0, 252, 188},
{  0, 252, 252},
{  0, 188, 252},
{  0, 124, 252},
{  0,  64, 252},
{124, 124, 252},
{156, 124, 252},
{188, 124, 252},
{220, 124, 252},
{252, 124, 252},
{252, 124, 220},
{252, 124, 188},
{252, 124, 156},
{252, 124, 124},
{252, 156, 124},
{252, 188, 124},
{252, 220, 124},
{252, 252, 124},
{220, 252, 124},
{188, 252, 124},
{156, 252, 124},
{124, 252, 124},
{124, 252, 156},
{124, 252, 188},
{124, 252, 220},
{124, 252, 252},
{124, 220, 252},
{124, 188, 252},
{124, 156, 252},
{180, 180, 252},
{196, 180, 252},
{216, 180, 252},
{232, 180, 252},
{252, 180, 252},
{252, 180, 232},
{252, 180, 216},
{252, 180, 196},
{252, 180, 180},
{252, 196, 180},
{252, 216, 180},
{252, 232, 180},
{252, 252, 180},
{232, 252, 180},
{216, 252, 180},
{196, 252, 180},
{180, 252, 180},
{180, 252, 196},
{180, 252, 216},
{180, 252, 232},
{180, 252, 252},
{180, 232, 252},
{180, 216, 252},
{180, 196, 252},
{  0,   0, 112},
{ 28,   0, 112},
{ 56,   0, 112},
{ 84,   0, 112},
{112,   0, 112},
{112,   0,  84},
{112,   0,  56},
{112,   0,  28},
{112,   0,   0},
{112,  28,   0},
{112,  56,   0},
{112,  84,   0},
{112, 112,   0},
{ 84, 112,   0},
{ 56, 112,   0},
{ 28, 112,   0},
{  0, 112,   0},
{  0, 112,  28},
{  0, 112,  56},
{  0, 112,  84},
{  0, 112, 112},
{  0,  84, 112},
{  0,  56, 112},
{  0,  28, 112},
{ 56,  56, 112},
{ 68,  56, 112},
{ 84,  56, 112},
{ 96,  56, 112},
{112,  56, 112},
{112,  56,  96},
{112,  56,  84},
{112,  56,  68},
{112,  56,  56},
{112,  68,  56},
{112,  84,  56},
{112,  96,  56},
{112, 112,  56},
{ 96, 112,  56},
{ 84, 112,  56},
{ 68, 112,  56},
{ 56, 112,  56},
{ 56, 112,  68},
{ 56, 112,  84},
{ 56, 112,  96},
{ 56, 112, 112},
{ 56,  96, 112},
{ 56,  84, 112},
{ 56,  68, 112},
{ 80,  80, 112},
{ 88,  80, 112},
{ 96,  80, 112},
{104,  80, 112},
{112,  80, 112},
{112,  80, 104},
{112,  80,  96},
{112,  80,  88},
{112,  80,  80},
{112,  88,  80},
{112,  96,  80},
{112, 104,  80},
{112, 112,  80},
{104, 112,  80},
{ 96, 112,  80},
{ 88, 112,  80},
{ 80, 112,  80},
{ 80, 112,  88},
{ 80, 112,  96},
{ 80, 112, 104},
{ 80, 112, 112},
{ 80, 104, 112},
{ 80,  96, 112},
{ 80,  88, 112},
{  0,   0,  64},
{ 16,   0,  64},
{ 32,   0,  64},
{ 48,   0,  64},
{ 64,   0,  64},
{ 64,   0,  48},
{ 64,   0,  32},
{ 64,   0,  16},
{ 64,   0,   0},
{ 64,  16,   0},
{ 64,  32,   0},
{ 64,  48,   0},
{ 64,  64,   0},
{ 48,  64,   0},
{ 32,  64,   0},
{ 16,  64,   0},
{  0,  64,   0},
{  0,  64,  16},
{  0,  64,  32},
{  0,  64,  48},
{  0,  64,  64},
{  0,  48,  64},
{  0,  32,  64},
{  0,  16,  64},
{ 32,  32,  64},
{ 40,  32,  64},
{ 48,  32,  64},
{ 56,  32,  64},
{ 64,  32,  64},
{ 64,  32,  56},
{ 64,  32,  48},
{ 64,  32,  40},
{ 64,  32,  32},
{ 64,  40,  32},
{ 64,  48,  32},
{ 64,  56,  32},
{ 64,  64,  32},
{ 56,  64,  32},
{ 48,  64,  32},
{ 40,  64,  32},
{ 32,  64,  32},
{ 32,  64,  40},
{ 32,  64,  48},
{ 32,  64,  56},
{ 32,  64,  64},
{ 32,  56,  64},
{ 32,  48,  64},
{ 32,  40,  64},
{ 44,  44,  64},
{ 48,  44,  64},
{ 52,  44,  64},
{ 60,  44,  64},
{ 64,  44,  64},
{ 64,  44,  60},
{ 64,  44,  52},
{ 64,  44,  48},
{ 64,  44,  44},
{ 64,  48,  44},
{ 64,  52,  44},
{ 64,  60,  44},
{ 64,  64,  44},
{ 60,  64,  44},
{ 52,  64,  44},
{ 48,  64,  44},
{ 44,  64,  44},
{ 44,  64,  48},
{ 44,  64,  52},
{ 44,  64,  60},
{ 44,  64,  64},
{ 44,  60,  64},
{ 44,  52,  64},
{ 44,  48,  64},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0},
{  0,   0,   0} };


extern int tabmode;

/* ********************** Mouse Support Variables ************************** */

int lookatmouse=0;  /* see notes at mouseread routine */
long savebase=0;    /* base clock ticks */
long saveticks=0;   /* save after this many ticks */
int finishrow=0;    /* save when this row is finished */

int inside_help = 0;

extern int slides;  /* 1 for playback */

# if 0
void fpe_handler(int signum)
{
  signal(SIGFPE, fpe_handler);
  overflow = 1;
}
#endif

/*
; ****************** Function getakey() *****************************
; **************** Function keypressed() ****************************

;       'getakey()' gets a key from either a "normal" or an enhanced
;       keyboard.   Returns either the vanilla ASCII code for regular
;       keys, or 1000+(the scan code) for special keys (like F1, etc)
;       Use of this routine permits the Control-Up/Down arrow keys on
;       enhanced keyboards.
;
;       The concept for this routine was "borrowed" from the MSKermit
;       SCANCHEK utility
;
;       'keypressed()' returns a zero if no keypress is outstanding,
;       and the value that 'getakey()' will return if one is.  Note
;       that you must still call 'getakey()' to flush the character.
;       As a sidebar function, calls 'help()' if appropriate, or
;       'tab_display()' if appropriate.
;       Think of 'keypressed()' as a super-'kbhit()'.
*/
int keybuffer = 0;

int getkeyint(int);

int keypressed(void)
{
  int ch;
  ch = getkeyint(0);
  if (!ch) return 0;
  keybuffer = ch;
  if (ch==F1 && helpmode)
    {
      keybuffer = 0;
      inside_help = 1;
      if (show_orbit)
      {
        scrub_orbit();
        taborhelp = 1;
      }
      help(0);
      inside_help = 0;
      return 0;
    }
  else if (ch==TAB && tabmode)
    {
      keybuffer = 0;
      if (show_orbit)
      {
        scrub_orbit();
        taborhelp = 1;
      }
      tab_display();
      return 0;
    }
  return ch;
}

// NOTE (jonathan#1#): Next does not do what is described here.  There is no delay.
/* Wait for a key.
 * This should be used instead of:
 * while (!keypressed()) {}
 * If timeout=1, waitkeypressed will time out after .5 sec.
 */
int waitkeypressed(int timeout)
{
  while (!keybuffer)
    {
      keybuffer = getkeyint(0);
      if (timeout) break;
    }
  return keypressed();
}

/*
 * This routine returns a key, ignoring F1
 */
int getakeynohelp(void)
{
  int ch;
  while (1)
    {
      ch = getakey();
      if (ch != F1) break;
    }
  return ch;
}
/*
 * This routine returns a keypress
 */
int getakey(void)
{
  int ch;
/*
  ch = soundflag & 7;
  if (ch != 0 || ch != 7)
      mute();
*/
  do
    {
      ch = getkeyint(0);
    }
  while (ch==0);
  return ch;
}

/*
 * This is the low level key handling routine.
 * If block is set, we want to block before returning, since we are waiting
 * for a key press.
 * We also have to handle the slide file, etc.
 */

int getkeyint(int block)
{
  int ch;
  int curkey;

  if (keybuffer)
    {
      ch = keybuffer;
      keybuffer = 0;
      return ch;
    }
  curkey = get_key_event(0);
  if (slides==1 && curkey == ESC)
    {
      stopslideshow();
      return 0;
    }

  if (curkey==0 && slides==1)
    {
      curkey = slideshw();
    }

  if (curkey==0 && block)
    {
      curkey = get_key_event(block);
      if (slides==1 && curkey == ESC)
        {
          stopslideshow();
          return 0;
        }
    }

  if (curkey && slides==2)
    {
      recordshw(curkey);
    }

  return curkey;
}

/*
 *----------------------------------------------------------------------
 *
 * kbhit --
 *
 *      Get a key.
 *
 * Results:
 *      1 if key, 0 otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int kbhit(void)
{
  return 0;
}

/* microsecond timer routine */
static struct timeval tv_start;

void restart_uclock(void)
{
  gettimeofday(&tv_start, NULL);
}

uclock_t usec_clock(void)
{
  uclock_t result;

  struct timeval tv, elapsed;
  gettimeofday(&tv, NULL);

  elapsed.tv_usec  = tv.tv_usec -  tv_start.tv_usec;
  elapsed.tv_sec   = tv.tv_sec -   tv_start.tv_sec;

  if (elapsed.tv_usec < 0)
    {
      /* "borrow */
      elapsed.tv_usec += 1000000;
      elapsed.tv_sec--;
    }
  result  = (unsigned long)(elapsed.tv_sec*1000000 +  elapsed.tv_usec);
  return(result);
}

/*
; long readticker() returns current ticker value
*/
long readticker(void)
{
  return clock_ticks();
}

/*
 * checkautosave() determines when auto save is needed.
 * Returns 1 if yes
 * Returns 0 if no
 */
int checkautosave(void)
{

long timervalue;

if (saveticks != 0) /* timer running */
   {
   /* get current timer value */
   timervalue = readticker() - savebase;
   if (timervalue >= saveticks) /* time to check */
      {

      /* time to save, check for end of row */
      if (finishrow == -1) /* end of row */
         {
          if (calc_status == 1) /* still calculating, check row */
            {
             if (got_status == 0 || got_status == 1) /* 1 or 2 pass, or solid guessing */
                {
                finishrow = currow;
                }
             }
         else /* done calculating */
             timedsave = 1; /* do the save */

         }
      else /* not end of row */
         if (finishrow != currow) /* start of next row */
            timedsave = 1; /* do the save */

      } /* time to check */
   } /* timer running */
   if (timedsave == 1) {
     keybuffer = 9999;
     return(1);
   }
   return(0);
}

long stackavail(void)
{
  return 8192;
}

long normalize(char *ptr)
{
  return (long) ptr;
}

#if 1 /* XFRACT */
/* --------------------------------------------------------------------
 * The following routines are used for encoding/decoding gif images.
 * If we aren't on a PC, things are rough for decoding the fractal info
 * structure in the GIF file.  These routines look after converting the
 * MS_DOS format data into a form we can use.
 * If dir==0, we convert to MSDOS form.  Otherwise we convert from MSDOS.
 */

static void getChar(unsigned char *, unsigned char **, int);
static void getInt(U16 *, unsigned char **, int);
static void getSInt(S16 *, unsigned char **, int);
static void getLong(S32 *, unsigned char **, int);
static void getDouble(double *, unsigned char **, int);
static void getFloat(float *, unsigned char **, int);

void decode_fractal_info(struct fractal_info *info, int dir)
{
  unsigned char *buf;
  unsigned char *bufPtr;
  int i;

  if (dir==1)
    {
      buf = (unsigned char *)malloc(FRACTAL_INFO_SIZE);
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,FRACTAL_INFO_SIZE);
    }
  else
    {
      buf = (unsigned char *)malloc(sizeof(struct fractal_info));
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,sizeof(struct fractal_info));
    }

  if (dir==1)
    {
      strncpy(info->info_id,(char *)bufPtr,8);
    }
  else
    {
      strncpy((char *)bufPtr,info->info_id,8);
    }
  bufPtr += 8;
  getSInt(&info->iterationsold,&bufPtr,dir);
  getSInt(&info->fractal_type,&bufPtr,dir);
  getDouble(&info->xmin,&bufPtr,dir);
  getDouble(&info->xmax,&bufPtr,dir);
  getDouble(&info->ymin,&bufPtr,dir);
  getDouble(&info->ymax,&bufPtr,dir);
  getDouble(&info->creal,&bufPtr,dir);
  getDouble(&info->cimag,&bufPtr,dir);
  getSInt(&info->videomodeax,&bufPtr,dir);
  getSInt(&info->videomodebx,&bufPtr,dir);
  getSInt(&info->videomodecx,&bufPtr,dir);
  getSInt(&info->videomodedx,&bufPtr,dir);
  getSInt(&info->dotmode,&bufPtr,dir);
  getInt(&info->xdots,&bufPtr,dir);
  getInt(&info->ydots,&bufPtr,dir);
  getSInt(&info->colors,&bufPtr,dir);
  getSInt(&info->version,&bufPtr,dir);
  getFloat(&info->parm3,&bufPtr,dir);
  getFloat(&info->parm4,&bufPtr,dir);
  getFloat(&info->potential[0],&bufPtr,dir);
  getFloat(&info->potential[1],&bufPtr,dir);
  getFloat(&info->potential[2],&bufPtr,dir);
  getSInt(&info->rseed,&bufPtr,dir);
  getSInt(&info->rflag,&bufPtr,dir);
  getSInt(&info->biomorph,&bufPtr,dir);
  getSInt(&info->inside,&bufPtr,dir);
  getSInt(&info->logmapold,&bufPtr,dir);
  getFloat(&info->invert[0],&bufPtr,dir);
  getFloat(&info->invert[1],&bufPtr,dir);
  getFloat(&info->invert[2],&bufPtr,dir);
  getSInt(&info->decomp[0],&bufPtr,dir);
  getSInt(&info->decomp[1],&bufPtr,dir);
  getSInt(&info->symmetry,&bufPtr,dir);
  for (i=0;i<16;i++)
    {
      getSInt(&info->init3d[i],&bufPtr,dir);
    }
  getSInt(&info->previewfactor,&bufPtr,dir);
  getSInt(&info->xtrans,&bufPtr,dir);
  getSInt(&info->ytrans,&bufPtr,dir);
  getSInt(&info->red_crop_left,&bufPtr,dir);
  getSInt(&info->red_crop_right,&bufPtr,dir);
  getSInt(&info->blue_crop_left,&bufPtr,dir);
  getSInt(&info->blue_crop_right,&bufPtr,dir);
  getSInt(&info->red_bright,&bufPtr,dir);
  getSInt(&info->blue_bright,&bufPtr,dir);
  getSInt(&info->xadjust,&bufPtr,dir);
  getSInt(&info->eyeseparation,&bufPtr,dir);
  getSInt(&info->glassestype,&bufPtr,dir);
  getSInt(&info->outside,&bufPtr,dir);
  getDouble(&info->x3rd,&bufPtr,dir);
  getDouble(&info->y3rd,&bufPtr,dir);
  getChar(&info->stdcalcmode,&bufPtr,dir);
  getChar(&info->useinitorbit,&bufPtr,dir);
  getSInt(&info->calc_status,&bufPtr,dir);
  getLong(&info->tot_extend_len,&bufPtr,dir);
  getSInt(&info->distestold,&bufPtr,dir);
  getSInt(&info->floatflag,&bufPtr,dir);
  getSInt(&info->bailoutold,&bufPtr,dir);
  getLong(&info->calctime,&bufPtr,dir);
  for (i=0;i<4;i++)
    {
      getChar(&info->trigndx[i],&bufPtr,dir);
    }
  getSInt(&info->finattract,&bufPtr,dir);
  getDouble(&info->initorbit[0],&bufPtr,dir);
  getDouble(&info->initorbit[1],&bufPtr,dir);
  getSInt(&info->periodicity,&bufPtr,dir);
  getSInt(&info->pot16bit,&bufPtr,dir);
  getFloat(&info->faspectratio,&bufPtr,dir);
  getSInt(&info->system,&bufPtr,dir);
  getSInt(&info->release,&bufPtr,dir);
  getSInt(&info->flag3d,&bufPtr,dir);
  getSInt(&info->transparent[0],&bufPtr,dir);
  getSInt(&info->transparent[1],&bufPtr,dir);
  getSInt(&info->ambient,&bufPtr,dir);
  getSInt(&info->haze,&bufPtr,dir);
  getSInt(&info->randomize,&bufPtr,dir);
  getSInt(&info->rotate_lo,&bufPtr,dir);
  getSInt(&info->rotate_hi,&bufPtr,dir);
  getSInt(&info->distestwidth,&bufPtr,dir);
  getDouble(&info->dparm3,&bufPtr,dir);
  getDouble(&info->dparm4,&bufPtr,dir);
  getSInt(&info->fillcolor,&bufPtr,dir);
  getDouble(&info->mxmaxfp,&bufPtr,dir);
  getDouble(&info->mxminfp,&bufPtr,dir);
  getDouble(&info->mymaxfp,&bufPtr,dir);
  getDouble(&info->myminfp,&bufPtr,dir);
  getSInt(&info->zdots,&bufPtr,dir);
  getFloat(&info->originfp,&bufPtr,dir);
  getFloat(&info->depthfp,&bufPtr,dir);
  getFloat(&info->heightfp,&bufPtr,dir);
  getFloat(&info->widthfp,&bufPtr,dir);
  getFloat(&info->distfp,&bufPtr,dir);
  getFloat(&info->eyesfp,&bufPtr,dir);
  getSInt(&info->orbittype,&bufPtr,dir);
  getSInt(&info->juli3Dmode,&bufPtr,dir);
  getSInt(&info->maxfn,&bufPtr,dir);
  getSInt(&info->inversejulia,&bufPtr,dir);
  getDouble(&info->dparm5,&bufPtr,dir);
  getDouble(&info->dparm6,&bufPtr,dir);
  getDouble(&info->dparm7,&bufPtr,dir);
  getDouble(&info->dparm8,&bufPtr,dir);
  getDouble(&info->dparm9,&bufPtr,dir);
  getDouble(&info->dparm10,&bufPtr,dir);
  getLong(&info->bailout,&bufPtr,dir);
  getSInt(&info->bailoutest,&bufPtr,dir);
  getLong(&info->iterations,&bufPtr,dir);
  getSInt(&info->bf_math,&bufPtr,dir);
  getSInt(&info->bflength,&bufPtr,dir);
  getSInt(&info->yadjust,&bufPtr,dir);
  getSInt(&info->old_demm_colors,&bufPtr,dir);
  getLong(&info->logmap,&bufPtr,dir);
  getLong(&info->distest,&bufPtr,dir);
  getDouble(&info->dinvert[0],&bufPtr,dir);
  getDouble(&info->dinvert[1],&bufPtr,dir);
  getDouble(&info->dinvert[2],&bufPtr,dir);
  getSInt(&info->logcalc,&bufPtr,dir);
  getSInt(&info->stoppass,&bufPtr,dir);
  getSInt(&info->quick_calc,&bufPtr,dir);
  getDouble(&info->closeprox,&bufPtr,dir);
  getSInt(&info->nobof,&bufPtr,dir);
  getLong(&info->orbit_interval,&bufPtr,dir);
  getSInt(&info->orbit_delay,&bufPtr,dir);
  getDouble(&info->math_tol[0],&bufPtr,dir);
  getDouble(&info->math_tol[1],&bufPtr,dir);

  for (i=0;i<(int)(sizeof(info->future)/sizeof(short));i++)
    {
      getSInt(&info->future[i],&bufPtr,dir);
    }
  if (bufPtr-buf != FRACTAL_INFO_SIZE)
    {
      char msg[120];
      sprintf(msg, "Warning: loadfile miscount on fractal_info structure.\nComponents add up to %d bytes, but FRACTAL_INFO_SIZE = %d",
             bufPtr-buf, FRACTAL_INFO_SIZE);
      popup_error(2, msg);
    }
  if (dir==0)
    {
      bcopy((char *)buf,(char *)info,FRACTAL_INFO_SIZE);
    }

  free(buf);
}

/*
 * This routine gets a char out of the buffer.
 * It updates the buffer pointer accordingly.
 */
static void getChar(unsigned char *dst, unsigned char **src, int dir)
{
  if (dir==1)
    {
      *dst = **src;
    }
  else
    {
      **src = *dst;
    }
  (*src)++;
}

/*
 * This routine gets an unsigned int out of the buffer.
 * It updates the buffer pointer accordingly.
 */
static void getInt(U16 *dst, unsigned char **src, int dir)
{
  if (dir==1)
    {
      *dst = (*src)[0] + ((((char *)(*src))[1])<<8);
    }
  else
    {
      (*src)[0] = (*dst)&0xff;
      (*src)[1] = ((*dst)&0xff00)>>8;
    }
  (*src) += 2; /* sizeof(int) in MS_DOS */
}

/*
 * This routine gets a signed int out of the buffer.
 * It updates the buffer pointer accordingly.
 */
static void getSInt(S16 *dst, unsigned char **src, int dir)
{
  if (dir==1)
    {
      *dst = (*src)[0] + ((((char *)(*src))[1])<<8);
    }
  else
    {
      (*src)[0] = (*dst)&0xff;
      (*src)[1] = ((*dst)&0xff00)>>8;
    }
  (*src) += 2; /* sizeof(int) in MS_DOS */
}

/*
 * This routine gets a long out of the buffer.
 * It updates the buffer pointer accordingly.
 */
static void getLong(S32 *dst, unsigned char **src, int dir)
{
  if (dir==1)
    {
      *dst = ((U32)((*src)[0])) +
             (((U32)((*src)[1]))<<8) +
             (((U32)((*src)[2]))<<16) +
             (((S32)(((char *)(*src))[3]))<<24);
    }
  else
    {
      (*src)[0] = (*dst)&0xff;
      (*src)[1] = ((*dst)&0xff00)>>8;
      (*src)[2] = ((*dst)&0xff0000)>>16;
#ifdef __SVR4
      (*src)[3] = (unsigned)((*dst)&0xff000000)>>24;
#else
      (*src)[3] = ((*dst)&0xff000000)>>24;
#endif
    }
  (*src) += 4; /* sizeof(long) in MS_DOS */
}

#define P4 16.
#define P7 128.
#define P8 256.
#define P12 4096.
#define P15 32768.
#define P20 1048576.
#define P23 8388608.
#define P28 268435456.
#define P36 68719476736.
#define P44 17592186044416.
#define P52 4503599627370496.


/*
 * This routine gets a double out of the buffer, or puts a double into the
 * buffer;
 * It updates the buffer pointer accordingly.
 */
static void getDouble(double *dst, unsigned char **src, int dir)
{
  int e;
  double f;
  int i;
  if (dir==1)
    {
      for (i=0;i<8;i++)
        {
          if ((*src)[i] != 0) break;
        }
      if (i==8)
        {
          *dst = 0;
        }
      else
        {
#ifdef __SVR4
          e = (((*src)[7]&0x7f)<<4) + ((int)((*src)[6]&0xf0)>>4) - 1023;
          f = 1 + (int)((*src)[6]&0x0f)/P4 + (int)((*src)[5])/P12 +
              (int)((*src)[4])/P20 + (int)((*src)[3])/P28 + (int)((*src)[2])/P36 +
              (int)((*src)[1])/P44 + (int)((*src)[0])/P52;
#else
          e = (((*src)[7]&0x7f)<<4) + (((*src)[6]&0xf0)>>4) - 1023;
          f = 1 + ((*src)[6]&0x0f)/P4 + (*src)[5]/P12 + (*src)[4]/P20 +
              (*src)[3]/P28 + (*src)[2]/P36 + (*src)[1]/P44 + (*src)[0]/P52;
#endif
          f *= pow(2.,(double)e);
          if ((*src)[7]&0x80)
            {
              f = -f;
            }
          *dst = f;
        }
    }
  else
    {
      if (*dst==0)
        {
          bzero((char *)(*src),8);
        }
      else
        {
          int s=0;
          f = *dst;
          if (f<0)
            {
              s = 0x80;
              f = -f;
            }
          e = log(f)/log(2.);
          f = f/pow(2.,(double)e) - 1;
          if (f<0)
            {
              e--;
              f = (f+1)*2-1;
            }
          else if (f>=1)
            {
              e++;
              f = (f+1)/2-1;
            }
          e += 1023;
          (*src)[7] = s | ((e&0x7f0)>>4);
          f *= P4;
          (*src)[6] = ((e&0x0f)<<4) | (((int)f)&0x0f);
          f = (f-(int)f)*P8;
          (*src)[5] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[4] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[3] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[2] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[1] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[0] = (((int)f)&0xff);
        }
    }
  *src += 8; /* sizeof(double) in MSDOS */
}

/*
 * This routine gets a float out of the buffer.
 * It updates the buffer pointer accordingly.
 */
static void getFloat(float *dst, unsigned char **src, int dir)
{
  int e;
  double f;
  int i;
  if (dir==1)
    {
      for (i=0;i<4;i++)
        {
          if ((*src)[i] != 0) break;
        }
      if (i==4)
        {
          *dst = 0;
        }
      else
        {
#ifdef __SVR4
          e = ((((*src)[3]&0x7f)<<1) | ((int)((*src)[2]&0x80)>>7)) - 127;
          f = 1 + (int)((*src)[2]&0x7f)/P7 + (int)((*src)[1])/P15 + (int)((*src)[0])/P23;
#else
          e = ((((*src)[3]&0x7f)<<1) | (((*src)[2]&0x80)>>7)) - 127;
          f = 1 + ((*src)[2]&0x7f)/P7 + (*src)[1]/P15 + (*src)[0]/P23;
#endif
          f *= pow(2.,(double)e);
          if ((*src)[3]&0x80)
            {
              f = -f;
            }
          *dst = f;
        }
    }
  else
    {
      if (*dst==0)
        {
          bzero((char *)(*src),4);
        }
      else
        {
          int s=0;
          f = *dst;
          if (f<0)
            {
              s = 0x80;
              f = -f;
            }
          e = log(f)/log(2.);
          f = f/pow(2.,(double)e) - 1;
          if (f<0)
            {
              e--;
              f = (f+1)*2-1;
            }
          else if (f>=1)
            {
              e++;
              f = (f+1)/2-1;
            }
          e += 127;
          (*src)[3] = s | ((e&0xf7)>>1);
          f *= P7;
          (*src)[2] = ((e&0x01)<<7) | (((int)f)&0x7f);
          f = (f-(int)f)*P8;
          (*src)[1] = (((int)f)&0xff);
          f = (f-(int)f)*P8;
          (*src)[0] = (((int)f)&0xff);
        }
    }
  *src += 4; /* sizeof(float) in MSDOS */
}

/*
 * Fix up the ranges data.
 */
void fix_ranges(S16 *ranges, U16 num, int dir)
{
  unsigned char *buf;
  unsigned char *bufPtr;
  int i;

  if (dir==1)
    {
      buf = (unsigned char *)malloc(num*2);
      bufPtr = buf;
      bcopy((char *)ranges, (char *)buf, num*2);
    }
  else
    {
      buf = (unsigned char *)malloc(num*sizeof(int));
      bufPtr = buf;
      bcopy((char *)ranges, (char *)buf, num*sizeof(int));
    }
  for (i=0;i<num;i++)
    {
      getSInt((short *)&ranges[i],&bufPtr,dir);
    }
  free((char *)buf);
}

void decode_evolver_info(struct evolution_info *info, int dir)
{
  unsigned char *buf;
  unsigned char *bufPtr;
  int i;

  if (dir==1)
    {
      buf = (unsigned char *)malloc(EVOLVER_INFO_SIZE);
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,EVOLVER_INFO_SIZE);
    }
  else
    {
      buf = (unsigned char *)malloc(sizeof(struct evolution_info));
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,sizeof(struct evolution_info));
    }

  getSInt(&info->evolving,&bufPtr,dir);
  getSInt(&info->gridsz,&bufPtr,dir);
  getInt(&info->this_gen_rseed,&bufPtr,dir);
  getDouble(&info->fiddlefactor,&bufPtr,dir);
  getDouble(&info->paramrangex,&bufPtr,dir);
  getDouble(&info->paramrangey,&bufPtr,dir);
  getDouble(&info->opx,&bufPtr,dir);
  getDouble(&info->opy,&bufPtr,dir);
  getSInt(&info->odpx,&bufPtr,dir);
  getSInt(&info->odpy,&bufPtr,dir);
  getSInt(&info->px,&bufPtr,dir);
  getSInt(&info->py,&bufPtr,dir);
  getSInt(&info->sxoffs,&bufPtr,dir);
  getSInt(&info->syoffs,&bufPtr,dir);
  getInt(&info->xdots,&bufPtr,dir);
  getInt(&info->ydots,&bufPtr,dir);
  for (i=0;i<NUMGENES;i++)
    {
      getSInt(&info->mutate[i],&bufPtr,dir);
    }
  getSInt(&info->ecount,&bufPtr,dir);

  for (i=0;i<(int)(sizeof(info->future)/sizeof(short));i++)
    {
      getSInt(&info->future[i],&bufPtr,dir);
    }
  if (bufPtr-buf != EVOLVER_INFO_SIZE)
    {
      printf("Warning: loadfile miscount on evolution_info structure.\n");
      printf("Components add up to %d bytes, but EVOLVER_INFO_SIZE = %d\n",
             bufPtr-buf, EVOLVER_INFO_SIZE);
    }
  if (dir==0)
    {
      bcopy((char *)buf,(char *)info,EVOLVER_INFO_SIZE);
    }

  free(buf);
}

void decode_orbits_info(struct orbits_info *info, int dir)
{
  unsigned char *buf;
  unsigned char *bufPtr;
  int i;

  if (dir==1)
    {
      buf = (unsigned char *)malloc(ORBITS_INFO_SIZE);
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,ORBITS_INFO_SIZE);
    }
  else
    {
      buf = (unsigned char *)malloc(sizeof(struct orbits_info));
      bufPtr = buf;
      bcopy((char *)info,(char *)buf,sizeof(struct orbits_info));
    }

  getDouble(&info->oxmin,&bufPtr,dir);
  getDouble(&info->oxmax,&bufPtr,dir);
  getDouble(&info->oymin,&bufPtr,dir);
  getDouble(&info->oymax,&bufPtr,dir);
  getDouble(&info->ox3rd,&bufPtr,dir);
  getDouble(&info->oy3rd,&bufPtr,dir);
  getSInt(&info->keep_scrn_coords,&bufPtr,dir);
  getChar(&info->drawmode,&bufPtr,dir);
  getChar(&info->dummy,&bufPtr,dir);

  for (i=0;i<(int)(sizeof(info->future)/sizeof(short));i++)
    {
      getSInt(&info->future[i],&bufPtr,dir);
    }
  if (bufPtr-buf != ORBITS_INFO_SIZE)
    {
      printf("Warning: loadfile miscount on orbits_info structure.\n");
      printf("Components add up to %d bytes, but ORBITS_INFO_SIZE = %d\n",
             bufPtr-buf, ORBITS_INFO_SIZE);
    }
  if (dir==0)
    {
      bcopy((char *)buf,(char *)info,ORBITS_INFO_SIZE);
    }

  free(buf);
}
#endif /* XFRACT */


/* This ftime simulation routine is from Frank Chen */
void ftimex(struct timebx *tp)
{
  struct timeval  timep;
/*  struct timezone timezp; */
  int *timezp = NULL;

  if ( gettimeofday(&timep,&timezp) != 0)
    {
      perror("error in gettimeofday");
      exit(0);
    }
  tp->time = timep.tv_sec;
  tp->millitm = timep.tv_usec/1000;
/*
  tp->timezone = timezp.tz_minuteswest;
  tp->dstflag = timezp.tz_dsttime;
*/
}

#if 1 //def XFRACT
unsigned short rotl(unsigned short num, short bits)
{
  unsigned long ll;
  ll = (((unsigned long)num << 16) + num) << (bits&15);
  return((unsigned short)(ll>>16));
}

/*
 *----------------------------------------------------------------------
 *
 * ltoa --
 *
 *      Convert long to string.
 *
 * Results:
 *      0.
 *
 * Side effects:
 *      Prints number into the string.
 *
 *----------------------------------------------------------------------
 */

int ltoa(long num, char *str, int len)
{
  sprintf(str,"%10d",(int)num);
  return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * strlwr --
 *
 *      Convert string to lower case.
 *
 * Results:
 *      The string.
 *
 * Side effects:
 *      Modifies the string.
 *
 *----------------------------------------------------------------------
 */
char *strlwr(char *s)
{
  register char *sptr = s;
  while (*sptr != '\0')
    {
      if (isupper(*sptr))
        {
          *sptr = tolower(*sptr);
        }
      sptr++;
    }
  return s;
}

/*
 *----------------------------------------------------------------------
 *
 * memicmp --
 *
 *      Compare memory (like memcmp), but ignoring case.
 *
 * Results:
 *      -1,0,1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int memicmp(char *s1, char *s2, int n)
{
  register char c1,c2;
  while (--n >= 0)
    {
      c1 = *s1++;
      if (isupper(c1)) c1 = tolower(c1);
      c2 = *s2++;
      if (isupper(c2)) c2 = tolower(c2);
      if (c1 != c2)
        return (c1 - c2);
    }
  return (0);
}

#ifndef HAVESTRI
/*
 *----------------------------------------------------------------------
 *
 * stricmp --
 *
 *      Compare strings, ignoring case.
 *
 * Results:
 *      -1,0,1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int stricmp(char *s1, char *s2)
{
  int c1, c2;

  while (1)
    {
      c1 = *s1++;
      c2 = *s2++;
      if (isupper(c1)) c1 = tolower(c1);
      if (isupper(c2)) c2 = tolower(c2);
      if (c1 != c2)
        {
          return c1 - c2;
        }
      if (c1 == 0)
        {
          return 0;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * strnicmp --
 *
 *      Compare strings, ignoring case.  Maximum length is specified.
 *
 * Results:
 *      -1,0,1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int strnicmp(char *s1, char *s2, int numChars)
{
  register char c1, c2;

  for ( ; numChars > 0; --numChars)
    {
      c1 = *s1++;
      c2 = *s2++;
      if (isupper(c1)) c1 = tolower(c1);
      if (isupper(c2)) c2 = tolower(c2);
      if (c1 != c2)
        {
          return c1 - c2;
        }
      if (c1 == '\0')
        {
          return 0;
        }
    }
  return 0;
}
#endif /* HAVESTRI */
#endif /* XFRACT */
