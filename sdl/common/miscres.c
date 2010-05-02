/*
        Resident odds and ends that don't fit anywhere else.
*/

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <malloc.h>

#ifndef XFRACT
#include <io.h>
#endif

#ifndef USE_VARARGS
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*#ifdef __TURBOC__
#include <dir.h>
#endif  */

/* see Fractint.c for a description of the "include"  hierarchy */
#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"

/* routines in this module      */

static  void trigdetails(char *);
static void area(void);

/* TW's static string consolidation campaign to help brain-dead compilers */
char s_cantwrite[]      = {"Can't write %s"};
char s_cantcreate[]     = {"Can't create %s"};
char s_cantunderstand[] = {"Can't understand %s"};
char s_cantfind[]       = {"Can't find %s"};

#ifndef XFRACT

void findpath(char far *filename, char *fullpathname) /* return full pathnames */
{
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];
  char temp_path[FILE_MAX_PATH];

  splitpath(filename ,NULL,NULL,fname,ext);
  makepath(temp_path,""   ,"" ,fname,ext);

  if (checkcurdir != 0 && access(temp_path,0)==0)    /* file exists */
    {
      strcpy(fullpathname,temp_path);
      return;
    }

  far_strcpy(temp_path,filename);   /* avoid side effect changes to filename */

  if (temp_path[0] == SLASHC || (temp_path[0] && temp_path[1] == ':'))
    {
      if (access(temp_path,0)==0)    /* file exists */
        {
          strcpy(fullpathname,temp_path);
          return;
        }
      else
        {
          splitpath(temp_path ,NULL,NULL,fname,ext);
          makepath(temp_path,""   ,"" ,fname,ext);
        }
    }
  fullpathname[0] = 0;                         /* indicate none found */
  /* #ifdef __TURBOC__ */                         /* look for the file */
  /*   strcpy(fullpathname,searchpath(temp_path)); */
  /* #else */
  _searchenv(temp_path,"PATH",fullpathname);
  /* #endif */
  if (fullpathname[0] != 0)                    /* found it! */
    if (strncmp(&fullpathname[2],SLASHSLASH,2) == 0) /* stupid klooge! */
      strcpy(&fullpathname[3],temp_path);
}
#endif


void notdiskmsg()
{
  static FCODE sorrymsg[]=
  {
    "This type may be slow using a real-disk based 'video' mode, but may not \n\
    be too bad if you have enough expanded or extended memory. Press <Esc> to \n\
    abort if it appears that your disk drive is working too hard."
  };
  stopmsg(0,sorrymsg);
}

/* Wrapping version of putstring for long numbers                         */
/* row     -- pointer to row variable, internally incremented if needed   */
/* col1    -- starting column                                             */
/* col2    -- last column                                                 */
/* color   -- attribute (same as for putstring)                           */
/* maxrow -- max number of rows to write                                 */
/* returns 0 if success, 1 if hit maxrow before done                      */
int putstringwrap(int *row,int col1,int col2,int color,char far *str,int maxrow)
{
  char save1, save2;
  int length, decpt, padding, startrow, done;
  done = 0;
  startrow = *row;
  length = far_strlen(str);
  padding = 3; /* space between col1 and decimal. */
  /* find decimal point */
  for (decpt=0;decpt < length; decpt++)
    if (str[decpt] == '.')
      break;
  if (decpt >= length)
    decpt = 0;
  if (decpt < padding)
    padding -= decpt;
  else
    padding = 0;
  col1 += padding;
  decpt += col1+1; /* column just past where decimal is */
  while (length > 0)
    {
      if (col2-col1 < length)
        {
          if ((*row - startrow + 1) >= maxrow)
            done = 1;
          else
            done = 0;
          save1 = str[col2-col1+1];
          save2 = str[col2-col1+2];
          if (done)
            str[col2-col1+1]   = '+';
          else
            str[col2-col1+1]   = '\\';
          str[col2-col1+2] = 0;
          putstring(*row,col1,color,str);
          if (done == 1)
            break;
          str[col2-col1+1] = save1;
          str[col2-col1+2] = save2;
          str += col2-col1;
          (*row)++;
        }
      else
        putstring(*row,col1,color,str);
      length -= col2-col1;
      col1 = decpt; /* align with decimal */
    }
  return(done);
}

#define rad_to_deg(x) ((x)*(180.0/PI)) /* most people "think" in degrees */
#define deg_to_rad(x) ((x)*(PI/180.0))
/*
convert corners to center/mag
Rotation angles indicate how much the IMAGE has been rotated, not the
zoom box.  Same goes for the Skew angles
*/

#ifdef _MSC_VER
#pragma optimize( "", off )
#endif

void cvtcentermag(double *Xctr, double *Yctr, LDBL *Magnification, double *Xmagfactor, double *Rotation, double *Skew)
{
  double Width, Height;
  double a, b; /* bottom, left, diagonal */
  double a2, b2, c2; /* squares of above */
  double tmpx1, tmpx2, tmpy1, tmpy2, tmpa; /* temporary x, y, angle */

  /* simple normal case first */
  if (xx3rd == xxmin && yy3rd == yymin)
    {
      /* no rotation or skewing, but stretching is allowed */
      Width  = xxmax - xxmin;
      Height = yymax - yymin;
      *Xctr = (xxmin + xxmax)/2.0;
      *Yctr = (yymin + yymax)/2.0;
      *Magnification  = 2.0/Height;
      *Xmagfactor =  Height / (DEFAULTASPECT * Width);
      *Rotation = 0.0;
      *Skew = 0.0;
    }
  else
    {
      /* set up triangle ABC, having sides abc */
      /* side a = bottom, b = left, c = diagonal not containing (x3rd,y3rd) */
      tmpx1 = xxmax - xxmin;
      tmpy1 = yymax - yymin;
      c2 = tmpx1*tmpx1 + tmpy1*tmpy1;

      tmpx1 = xxmax - xx3rd;
      tmpy1 = yymin - yy3rd;
      a2 = tmpx1*tmpx1 + tmpy1*tmpy1;
      a = sqrt(a2);
      *Rotation = -rad_to_deg(atan2( tmpy1, tmpx1 )); /* negative for image rotation */

      tmpx2 = xxmin - xx3rd;
      tmpy2 = yymax - yy3rd;
      b2 = tmpx2*tmpx2 + tmpy2*tmpy2;
      b = sqrt(b2);

      tmpa = acos((a2+b2-c2)/(2*a*b)); /* save tmpa for later use */
      *Skew = 90.0 - rad_to_deg(tmpa);

      *Xctr = (xxmin + xxmax)*0.5;
      *Yctr = (yymin + yymax)*0.5;

      Height = b * sin(tmpa);

      *Magnification  = 2.0/Height; /* 1/(h/2) */
      *Xmagfactor = Height / (DEFAULTASPECT * a);

      /* if vector_a cross vector_b is negative */
      /* then adjust for left-hand coordinate system */
      if ( tmpx1*tmpy2 - tmpx2*tmpy1 < 0 && debugflag != 4010)
        {
          *Skew = -*Skew;
          *Xmagfactor = -*Xmagfactor;
          *Magnification = -*Magnification;
        }
    }
  /* just to make par file look nicer */
  if (*Magnification < 0)
    {
      *Magnification = -*Magnification;
      *Rotation += 180;
    }
#ifdef DEBUG
  {
    double txmin, txmax, tx3rd, tymin, tymax, ty3rd;
    double error;
    txmin = xxmin;
    txmax = xxmax;
    tx3rd = xx3rd;
    tymin = yymin;
    tymax = yymax;
    ty3rd = yy3rd;
    cvtcorners(*Xctr, *Yctr, *Magnification, *Xmagfactor, *Rotation, *Skew);
    error = sqr(txmin - xxmin) +
            sqr(txmax - xxmax) +
            sqr(tx3rd - xx3rd) +
            sqr(tymin - yymin) +
            sqr(tymax - yymax) +
            sqr(ty3rd - yy3rd);
    if (error > .001)
      showcornersdbl("cvtcentermag problem");
    xxmin = txmin;
    xxmax = txmax;
    xx3rd = tx3rd;
    yymin = tymin;
    yymax = tymax;
    yy3rd = ty3rd;
  }
#endif
  return;
}


/* convert center/mag to corners */
void cvtcorners(double Xctr, double Yctr, LDBL Magnification, double Xmagfactor, double Rotation, double Skew)
{
  double x, y;
  double h, w; /* half height, width */
  double tanskew, sinrot, cosrot;

  if (Xmagfactor == 0.0)
    Xmagfactor = 1.0;

  h = (double)(1/Magnification);
  w = h / (DEFAULTASPECT * Xmagfactor);

  if (Rotation == 0.0 && Skew == 0.0)
    {
      /* simple, faster case */
      xx3rd = xxmin = Xctr - w;
      xxmax = Xctr + w;
      yy3rd = yymin = Yctr - h;
      yymax = Yctr + h;
      return;
    }

  /* in unrotated, untranslated coordinate system */
  tanskew = tan(deg_to_rad(Skew));
  xxmin = -w + h*tanskew;
  xxmax =  w - h*tanskew;
  xx3rd = -w - h*tanskew;
  yymax = h;
  yy3rd = yymin = -h;

  /* rotate coord system and then translate it */
  Rotation = deg_to_rad(Rotation);
  sinrot = sin(Rotation);
  cosrot = cos(Rotation);

  /* top left */
  x = xxmin * cosrot + yymax *  sinrot;
  y = -xxmin * sinrot + yymax *  cosrot;
  xxmin = x + Xctr;
  yymax = y + Yctr;

  /* bottom right */
  x = xxmax * cosrot + yymin *  sinrot;
  y = -xxmax * sinrot + yymin *  cosrot;
  xxmax = x + Xctr;
  yymin = y + Yctr;

  /* bottom left */
  x = xx3rd * cosrot + yy3rd *  sinrot;
  y = -xx3rd * sinrot + yy3rd *  cosrot;
  xx3rd = x + Xctr;
  yy3rd = y + Yctr;

  return;
}

/* convert corners to center/mag using bf */
void cvtcentermagbf(bf_t Xctr, bf_t Yctr, LDBL *Magnification, double *Xmagfactor, double *Rotation, double *Skew)
{
  /* needs to be LDBL or won't work past 307 (-DBL_MIN_10_EXP) or so digits */
  LDBL Width, Height;
  LDBL a, b; /* bottom, left, diagonal */
  LDBL a2, b2, c2; /* squares of above */
  LDBL tmpx1, tmpx2, tmpy=0.0, tmpy1, tmpy2 ;
  double tmpa; /* temporary x, y, angle */
  bf_t bfWidth, bfHeight;
  bf_t bftmpx, bftmpy;
  int saved;
  int signx;

  saved = save_stack();

  /* simple normal case first */
  /* if (xx3rd == xxmin && yy3rd == yymin) */
  if (!cmp_bf(bfx3rd, bfxmin) && !cmp_bf(bfy3rd, bfymin))
    {
      /* no rotation or skewing, but stretching is allowed */
      bfWidth  = alloc_stack(bflength+2);
      bfHeight = alloc_stack(bflength+2);
      /* Width  = xxmax - xxmin; */
      sub_bf(bfWidth, bfxmax, bfxmin);
      Width  = bftofloat(bfWidth);
      /* Height = yymax - yymin; */
      sub_bf(bfHeight, bfymax, bfymin);
      Height = bftofloat(bfHeight);
      /* *Xctr = (xxmin + xxmax)/2; */
      add_bf(Xctr, bfxmin, bfxmax);
      half_a_bf(Xctr);
      /* *Yctr = (yymin + yymax)/2; */
      add_bf(Yctr, bfymin, bfymax);
      half_a_bf(Yctr);
      *Magnification  = 2/Height;
      *Xmagfactor =  (double)(Height / (DEFAULTASPECT * Width));
      *Rotation = 0.0;
      *Skew = 0.0;
    }
  else
    {
      bftmpx = alloc_stack(bflength+2);
      bftmpy = alloc_stack(bflength+2);

      /* set up triangle ABC, having sides abc */
      /* side a = bottom, b = left, c = diagonal not containing (x3rd,y3rd) */
      /* IMPORTANT: convert from bf AFTER subtracting */

      /* tmpx = xxmax - xxmin; */
      sub_bf(bftmpx, bfxmax, bfxmin);
      tmpx1 = bftofloat(bftmpx);
      /* tmpy = yymax - yymin; */
      sub_bf(bftmpy, bfymax, bfymin);
      tmpy1 = bftofloat(bftmpy);
      c2 = tmpx1*tmpx1 + tmpy1*tmpy1;

      /* tmpx = xxmax - xx3rd; */
      sub_bf(bftmpx, bfxmax, bfx3rd);
      tmpx1 = bftofloat(bftmpx);

      /* tmpy = yymin - yy3rd; */
      sub_bf(bftmpy, bfymin, bfy3rd);
      tmpy1 = bftofloat(bftmpy);
      a2 = tmpx1*tmpx1 + tmpy1*tmpy1;
      a = sqrtl(a2);

      /* divide tmpx and tmpy by |tmpx| so that double version of atan2() can be used */
      /* atan2() only depends on the ratio, this puts it in double's range */
      signx = sign(tmpx1);
      tmpy = tmpy1; /* otherwise tmpy could be undefined below */
      if (signx)
        tmpy = tmpy1/tmpx1 * signx;    /* tmpy = tmpy / |tmpx| */
      *Rotation = (double)(-rad_to_deg(atan2( (double)tmpy, signx ))); /* negative for image rotation */

      /* tmpx = xxmin - xx3rd; */
      sub_bf(bftmpx, bfxmin, bfx3rd);
      tmpx2 = bftofloat(bftmpx);
      /* tmpy = yymax - yy3rd; */
      sub_bf(bftmpy, bfymax, bfy3rd);
      tmpy2 = bftofloat(bftmpy);
      b2 = tmpx2*tmpx2 + tmpy2*tmpy2;
      b = sqrtl(b2);

      tmpa = acos((double)((a2+b2-c2)/(2*a*b))); /* save tmpa for later use */
      *Skew = 90 - rad_to_deg(tmpa);

      /* these are the only two variables that must use big precision */
      /* *Xctr = (xxmin + xxmax)/2; */
      add_bf(Xctr, bfxmin, bfxmax);
      half_a_bf(Xctr);
      /* *Yctr = (yymin + yymax)/2; */
      add_bf(Yctr, bfymin, bfymax);
      half_a_bf(Yctr);

      Height = b * sin(tmpa);
      *Magnification  = 2/Height; /* 1/(h/2) */
      *Xmagfactor = (double)(Height / (DEFAULTASPECT * a));

      /* if vector_a cross vector_b is negative */
      /* then adjust for left-hand coordinate system */
      if ( tmpx1*tmpy2 - tmpx2*tmpy1 < 0 && debugflag != 4010)
        {
          *Skew = -*Skew;
          *Xmagfactor = -*Xmagfactor;
          *Magnification = -*Magnification;
        }
    }
  if (*Magnification < 0)
    {
      *Magnification = -*Magnification;
      *Rotation += 180;
    }
  restore_stack(saved);
  return;
}


/* convert center/mag to corners using bf */
void cvtcornersbf(bf_t Xctr, bf_t Yctr, LDBL Magnification, double Xmagfactor, double Rotation, double Skew)
{
  LDBL x, y;
  LDBL h, w; /* half height, width */
  LDBL xmin, ymin, xmax, ymax, x3rd, y3rd;
  double tanskew, sinrot, cosrot;
  bf_t bfh, bfw;
  bf_t bftmp;
  int saved;

  saved = save_stack();
  bfh = alloc_stack(bflength+2);
  bfw = alloc_stack(bflength+2);

  if (Xmagfactor == 0.0)
    Xmagfactor = 1.0;

  h = 1/Magnification;
  floattobf(bfh, h);
  w = h / (DEFAULTASPECT * Xmagfactor);
  floattobf(bfw, w);

  if (Rotation == 0.0 && Skew == 0.0)
    {
      /* simple, faster case */
      /* xx3rd = xxmin = Xctr - w; */
      sub_bf(bfxmin, Xctr, bfw);
      copy_bf(bfx3rd, bfxmin);
      /* xxmax = Xctr + w; */
      add_bf(bfxmax, Xctr, bfw);
      /* yy3rd = yymin = Yctr - h; */
      sub_bf(bfymin, Yctr, bfh);
      copy_bf(bfy3rd, bfymin);
      /* yymax = Yctr + h; */
      add_bf(bfymax, Yctr, bfh);
      restore_stack(saved);
      return;
    }

  bftmp = alloc_stack(bflength+2);
  /* in unrotated, untranslated coordinate system */
  tanskew = tan(deg_to_rad(Skew));
  xmin = -w + h*tanskew;
  xmax =  w - h*tanskew;
  x3rd = -w - h*tanskew;
  ymax = h;
  y3rd = ymin = -h;

  /* rotate coord system and then translate it */
  Rotation = deg_to_rad(Rotation);
  sinrot = sin(Rotation);
  cosrot = cos(Rotation);

  /* top left */
  x =  xmin * cosrot + ymax *  sinrot;
  y = -xmin * sinrot + ymax *  cosrot;
  /* xxmin = x + Xctr; */
  floattobf(bftmp, x);
  add_bf(bfxmin, bftmp, Xctr);
  /* yymax = y + Yctr; */
  floattobf(bftmp, y);
  add_bf(bfymax, bftmp, Yctr);

  /* bottom right */
  x =  xmax * cosrot + ymin *  sinrot;
  y = -xmax * sinrot + ymin *  cosrot;
  /* xxmax = x + Xctr; */
  floattobf(bftmp, x);
  add_bf(bfxmax, bftmp, Xctr);
  /* yymin = y + Yctr; */
  floattobf(bftmp, y);
  add_bf(bfymin, bftmp, Yctr);

  /* bottom left */
  x =  x3rd * cosrot + y3rd *  sinrot;
  y = -x3rd * sinrot + y3rd *  cosrot;
  /* xx3rd = x + Xctr; */
  floattobf(bftmp, x);
  add_bf(bfx3rd, bftmp, Xctr);
  /* yy3rd = y + Yctr; */
  floattobf(bftmp, y);
  add_bf(bfy3rd, bftmp, Yctr);

  restore_stack(saved);
  return;
}

#ifdef _MSC_VER
#pragma optimize( "", on )
#endif

void updatesavename(char *filename) /* go to the next file name */
{
  char *save, *hold;
  char drive[FILE_MAX_DRIVE];
  char dir[FILE_MAX_DIR];
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];

  splitpath(filename ,drive,dir,fname,ext);

  hold = fname + strlen(fname) - 1; /* start at the end */
  while (hold >= fname && (*hold == ' ' || isdigit(*hold))) /* skip backwards */
    hold--;
  hold++;                      /* recover first digit */
  while (*hold == '0')         /* skip leading zeros */
    hold++;
  save = hold;
  while (*save)                /* check for all nines */
    {
      if (*save != '9')
        break;
      save++;
    }
  if (!*save)                  /* if the whole thing is nines then back */
    save = hold - 1;          /* up one place. Note that this will eat */
  /* your last letter if you go to far.    */
  else
    save = hold;
  sprintf(save,"%ld",atol(hold)+1); /* increment the number */
  makepath(filename,drive,dir,fname,ext);
}

int check_writefile(char *name,char *ext)
{
  /* after v16 release, change encoder.c to also use this routine */
  char openfile[FILE_MAX_DIR];
  char opentype[20];
  /* int i; */
  char *period;
nextname:
  strcpy(openfile,name);
  strcpy(opentype,ext);
#if 0
  for (i = 0; i < (int)strlen(openfile); i++)
    if (openfile[i] == '.')
      {
        strcpy(opentype,&openfile[i]);
        openfile[i] = 0;
      }
#endif
  if ((period = has_ext(openfile)) != NULL)
    {
      strcpy(opentype,period);
      *period = 0;
    }
  strcat(openfile,opentype);
  if (access(openfile,0) != 0) /* file doesn't exist */
    {
      strcpy(name,openfile);
      return 0;
    }
  /* file already exists */
  if (overwrite == 0)
    {
      updatesavename(name);
      goto nextname;
    }
  return 1;
}

/* ('check_key()' was moved to FRACTINT.C for MSC7-overlay speed purposes) */
/* ('timer()'     was moved to FRACTINT.C for MSC7-overlay speed purposes) */

BYTE trigndx[] = {SIN,SQR,SINH,COSH};
void (*ltrig0)(void) = lStkSin;
void (*ltrig1)(void) = lStkSqr;
void (*ltrig2)(void) = lStkSinh;
void (*ltrig3)(void) = lStkCosh;
void (*dtrig0)(void) = dStkSin;
void (*dtrig1)(void) = dStkSqr;
void (*dtrig2)(void) = dStkSinh;
void (*dtrig3)(void) = dStkCosh;

/* struct trig_funct_lst trigfn[]  was moved to prompts1.c */

void showtrig(char *buf) /* return display form of active trig functions */
{
  char tmpbuf[30];
  *buf = 0; /* null string if none */
  trigdetails(tmpbuf);
  if (tmpbuf[0])
    sprintf(buf," function=%s",tmpbuf);
}

static void trigdetails(char *buf)
{
  int i, numfn;
  char tmpbuf[20];
  if (fractype==JULIBROT || fractype==JULIBROTFP)
    numfn = (fractalspecific[neworbittype].flags >> 6) & 7;
  else
    numfn = (curfractalspecific->flags >> 6) & 7;
  if (curfractalspecific == &fractalspecific[FORMULA] ||
      curfractalspecific == &fractalspecific[FFORMULA]  )
    numfn = maxfn;
  *buf = 0; /* null string if none */
  if (numfn>0)
    {
      strcpy(buf,trigfn[trigndx[0]].name);
      i = 0;
      while (++i < numfn)
        {
          sprintf(tmpbuf,"/%s",trigfn[trigndx[i]].name);
          strcat(buf,tmpbuf);
        }
    }
}

/* set array of trig function indices according to "function=" command */
int set_trig_array(int k, char *name)
{
  char trigname[10];
  int i;
  char *slash;
  strncpy(trigname,name,6);
  trigname[6] = 0; /* safety first */

  if ((slash = strchr(trigname,'/')) != NULL)
    *slash = 0;

  strlwr(trigname);

  for (i=0;i<numtrigfn;i++)
    {
      if (strcmp(trigname,trigfn[i].name)==0)
        {
          trigndx[k] = (BYTE)i;
          set_trig_pointers(k);
          break;
        }
    }
  return(0);
}
void set_trig_pointers(int which)
{
  /* set trig variable functions to avoid array lookup time */
  int i;
  switch (which)
    {
    case 0:
      ltrig0 = trigfn[trigndx[0]].lfunct;
      dtrig0 = trigfn[trigndx[0]].dfunct;
      break;
    case 1:
      ltrig1 = trigfn[trigndx[1]].lfunct;
      dtrig1 = trigfn[trigndx[1]].dfunct;
      break;
    case 2:
      ltrig2 = trigfn[trigndx[2]].lfunct;
      dtrig2 = trigfn[trigndx[2]].dfunct;
      break;
    case 3:
      ltrig3 = trigfn[trigndx[3]].lfunct;
      dtrig3 = trigfn[trigndx[3]].dfunct;
      break;
    default: /* do 'em all */
      for (i=0;i<4;i++)
        set_trig_pointers(i);
      break;
    }
}

static FCODE sfractal_type[] =     {"Fractal type:"};
static FCODE sitem_name[] =        {"Item name:"};
static FCODE sitem_file[] =        {"Item file:"};
static FCODE s3D_transform[] =     {"3D Transform"};
static FCODE syou_are_cycling[] =  {"You are in color-cycling mode"};
static FCODE sfloating_point[] =   {"Floating-point"};
static FCODE ssolid_guessing[] =   {"Solid Guessing"};
static FCODE sboundary_tracing[] = {"Boundary Tracing"};
static FCODE stesseral[] =         {"Tesseral"};
static FCODE sdiffusion[] =        {"Diffusion"};
static FCODE sorbits[] =           {"Orbits"};
static FCODE scalculation_time[] = {"Calculation time:"};
static FCODE siterations[] =       {" 1000's of points:"};
static FCODE scornersxy[] =        {"Corners:                X                     Y"};
static FCODE stop_left[] =         {"Top-l"};
static FCODE sbottom_right[] =     {"Bot-r"};
static FCODE sbottom_left[] =      {"Bot-l"};
static FCODE scenter[] =           {"Ctr"};
static FCODE struncate[] =         {"(Center values shown truncated to 320 decimals)"};
static FCODE smag[] =              {"Mag"};
static FCODE sxmag[] =             {"X-Mag-Factor"};
static FCODE srot[] =              {"Rotation"};
static FCODE sskew[] =             {"Skew"};
static FCODE sparams[] =           {"Params "};
static FCODE siteration_maximum[] ={"Current (Max) Iteration: "};
static FCODE seffective_bailout[] ={"     Effective bailout: "};
static FCODE scurrent_rseed[] =    {"Current 'rseed': "};
static FCODE sinversion_radius[] = {"Inversion radius: "};
static FCODE sxcenter[] =          {"  xcenter: "};
static FCODE sycenter[] =          {"  ycenter: "};
static FCODE sparms_chgd[] = {"Parms chgd since generated"};
static FCODE sstill_being[] = {"Still being generated"};
static FCODE sinterrupted_resumable[] = {"Interrupted, resumable"};
static FCODE sinterrupted_non_resumable[] = {"Interrupted, non-resumable"};
static FCODE simage_completed[] = {"Image completed"};
static FCODE sflag_is_activated[] = {" flag is activated"};
static FCODE sinteger_math[]      = {"Integer math is in use"};
static FCODE sin_use_required[] = {" in use (required)"};
static FCODE sarbitrary_precision[] = {"Arbitrary precision "};
#if defined(XFRACT) || defined(WINFRACT)
static FCODE spressanykey[] = {"Press any key to continue, F6 for area, F7 for next page"};
#else
static FCODE spressanykey[] = {"Press any key to continue, F6 for area, CTRL-TAB for next page"};
#endif
static FCODE spressanykey1[] = {"Press Esc to continue, Backspace for first screen"};
static FCODE sbatch[] = {" (Batch mode)"};
static FCODE ssavename[] = {"Savename: "};
static FCODE sstopsecret[] = {"Top Secret Developer's Screen"};
static FCODE sthreepass[] = {" (threepass)"};
static FCODE sreallylongtime[] = {"A long time! (> 24.855 days)"};

void get_calculation_time(char *msg, long ctime)
{
  if (ctime >= 0)
    {
      sprintf(msg,"%3ld:%02ld:%02ld.%02ld", ctime/360000L,
              (ctime%360000L)/6000, (ctime%6000)/100, ctime%100);
    }
  else
    strcpy(msg,sreallylongtime);
}

static void show_str_var(char *name, char *var, int *row, char *msg)
{
  if (var == NULL)
    return;
  if (*var != 0)
    {
      sprintf(msg,"%s=%s",name,var);
      putstring((*row)++,2,C_GENERAL_HI,msg);
    }
}

int tab_display_2(char *msg)
{
  extern long maxptr, maxstack, startstack;
  int s_row,key,ret=0;
  helptitle();
  setattr(1,0,C_GENERAL_MED,24*80); /* init rest to background */

  s_row = 1;
  putstringcenter(s_row++,0,80,C_PROMPT_HI, sstopsecret);
  sprintf(msg,"Version %d patch %d",release, patchlevel);
  putstring(++s_row,2,C_GENERAL_HI,msg);
  sprintf(msg,"%lu bytes conventional stack free",stackavail());
  putstring(++s_row,2,C_GENERAL_HI,msg);
  sprintf(msg,"%ld of %ld bignum memory used",maxptr,maxstack);
  putstring(++s_row,2,C_GENERAL_HI,msg);
  sprintf(msg,"   %ld used for bignum globals", startstack);
  putstring(++s_row,2,C_GENERAL_HI,msg);
  sprintf(msg,"   %ld stack used == %ld variables of length %d",
          maxptr-startstack,(long)((maxptr-startstack)/(rbflength+2)),rbflength+2);
  putstring(++s_row,2,C_GENERAL_HI,msg);
  if (bf_math)
    {
      sprintf(msg,"intlength %-d bflength %-d ",intlength, bflength);
      putstring(++s_row,2,C_GENERAL_HI,msg);
    }
  s_row++;
  show_str_var(s_tempdir,    tempdir,      &s_row, msg);
  show_str_var(s_workdir,    workdir,      &s_row, msg);
  show_str_var(s_filename,   readname,     &s_row, msg);
  show_str_var(s_formulafile,FormFileName, &s_row, msg);
  show_str_var(s_savename,   savename,     &s_row, msg);
  show_str_var(s_parmfile,   CommandFile,  &s_row, msg);
  show_str_var(s_ifsfile,    IFSFileName,  &s_row, msg);
  show_str_var(s_autokeyname,autoname,     &s_row, msg);
  show_str_var(s_lightname,  light_name,   &s_row, msg);
  show_str_var(s_map,        MAP_name,     &s_row, msg);
  sprintf(msg,"Sizeof fractalspecific array %d",
          num_fractal_types*(int)sizeof(struct fractalspecificstuff));
  putstring(s_row++,2,C_GENERAL_HI,msg);
  sprintf(msg,"calc_status %d pixel [%d,%d]",calc_status,col,row);
  putstring(s_row++,2,C_GENERAL_HI,msg);
  if (fractype==FORMULA || fractype==FFORMULA)
    {
      sprintf(msg,"total_formula_mem %ld Max_Ops (posp) %u Max_Args (vsp) %u Used_extra %u",
              total_formula_mem,posp,vsp,used_extra);
      putstring(s_row++,2,C_GENERAL_HI,msg);
      sprintf(msg,"   Store ptr %d Loadptr %d Max_Ops var %u Max_Args var %u LastInitOp %d",
              StoPtr,LodPtr,Max_Ops,Max_Args,LastInitOp);
      putstring(s_row++,2,C_GENERAL_HI,msg);
    }
  else if (rhombus_stack[0])
    {
      sprintf(msg,"SOI Recursion %d stack free %d %d %d %d %d %d %d %d %d %d",
              max_rhombus_depth+1,
              rhombus_stack[0],
              rhombus_stack[1],
              rhombus_stack[2],
              rhombus_stack[3],
              rhombus_stack[4],
              rhombus_stack[5],
              rhombus_stack[6],
              rhombus_stack[7],
              rhombus_stack[8],
              rhombus_stack[9]);
      putstring(s_row++,2,C_GENERAL_HI,msg);
    }

  /*
     sprintf(msg,"xdots %d ydots %d sxdots %d sydots %d",xdots,ydots,sxdots,sydots);
     putstring(s_row++,2,C_GENERAL_HI,msg);
  */
  sprintf(msg,"xxstart %d xxstop %d yystart %d yystop %d %s uses_ismand %d",
          xxstart,xxstop,yystart,yystop,
#ifndef XFRACT
          curfractalspecific->orbitcalc == fFormula?"fast parser":
#endif
          curfractalspecific->orbitcalc ==  Formula?"slow parser":
          curfractalspecific->orbitcalc ==  BadFormula?"bad formula":
          "",uses_ismand);
  putstring(s_row++,2,C_GENERAL_HI,msg);
  /*
     sprintf(msg,"ixstart %d ixstop %d iystart %d iystop %d bitshift %d",
        ixstart,ixstop,iystart,iystop,bitshift);
  */
  {
    sprintf(msg,"minstackavail %d llimit2 %ld use_grid %d",
            minstackavail,llimit2,use_grid);
  }
  putstring(s_row++,2,C_GENERAL_HI,msg);
  putstringcenter(24,0,80,C_GENERAL_LO,spressanykey1);
  *msg = 0;
again:
  putstring(s_row,2,C_GENERAL_HI,msg);
  key=getakeynohelp();
  if (key != ESC && key != BACKSPACE && key != TAB)
    {
      sprintf(msg,"%d      ",key);
      goto again;
    }
  if (key == BACKSPACE || key == TAB)
    ret = 1;
  return(ret);
}

int tab_display()       /* display the status of the current image */
{
  int s_row, i, j, addrow=0;
  double Xctr, Yctr;
  LDBL Magnification;
  double Xmagfactor, Rotation, Skew;
  bf_t bfXctr=NULL, bfYctr=NULL;
  char msg[350];
  char far *msgptr;
  int key;
  int saved=0;
  int dec;
  int k;
  U16 save_extra_handle = 0;
  BYTE *ptr_to_extraseg = NULL;
  int hasformparam = 0;

  if (calc_status < 0)         /* no active fractal image */
    {
      return(0);                /* (no TAB on the credits screen) */
    }
  if (calc_status == 1)        /* next assumes CLK_TCK is 10^n, n>=2 */
    calctime += (clock_ticks() - timer_start) / (CLK_TCK/100);
  stackscreen();
  if (bf_math)
    {
      saved = save_stack();
      bfXctr = alloc_stack(bflength+2);
      bfYctr = alloc_stack(bflength+2);
    }
  if (fractype == FORMULA || fractype == FFORMULA)
    for (i = 0; i < MAXPARAMS; i += 2)
      if (!paramnotused(i))
        hasformparam++;
top:
  k = 0; /* initialize here so parameter line displays correctly on return
             from control-tab */
  helptitle();
  setattr(1,0,C_GENERAL_MED,24*80); /* init rest to background */
  s_row = 2;
  putstring(s_row,2,C_GENERAL_MED,sfractal_type);
  if (display3d > 0)
    putstring(s_row,16,C_GENERAL_HI,s3D_transform);
  else
    {
      putstring(s_row,16,C_GENERAL_HI,
                curfractalspecific->name[0] == '*' ?
                &curfractalspecific->name[1] :
                curfractalspecific->name);
      i = 0;
      if (fractype == FORMULA || fractype == FFORMULA)
        {
          putstring(s_row+1,3,C_GENERAL_MED,sitem_name);
          putstring(s_row+1,16,C_GENERAL_HI,FormName);
          i = strlen(FormName)+1;
          putstring(s_row+2,3,C_GENERAL_MED,sitem_file);
          if ((int)strlen(FormFileName) >= 29)
            addrow = 1;
          putstring(s_row+2+addrow,16,C_GENERAL_HI,FormFileName);
        }
      trigdetails(msg);
      putstring(s_row+1,16+i,C_GENERAL_HI,msg);
      if (fractype == LSYSTEM)
        {
          putstring(s_row+1,3,C_GENERAL_MED,sitem_name);
          putstring(s_row+1,16,C_GENERAL_HI,LName);
          putstring(s_row+2,3,C_GENERAL_MED,sitem_file);
          if ((int)strlen(LFileName) >= 28)
            addrow = 1;
          putstring(s_row+2+addrow,16,C_GENERAL_HI,LFileName);
        }
      if (fractype == IFS || fractype == IFS3D)
        {
          putstring(s_row+1,3,C_GENERAL_MED,sitem_name);
          putstring(s_row+1,16,C_GENERAL_HI,IFSName);
          putstring(s_row+2,3,C_GENERAL_MED,sitem_file);
          if ((int)strlen(IFSFileName) >= 28)
            addrow = 1;
          putstring(s_row+2+addrow,16,C_GENERAL_HI,IFSFileName);
        }
    }

  switch (calc_status)
    {
    case 0:
      msgptr = sparms_chgd;
      break;
    case 1:
      msgptr = sstill_being;
      break;
    case 2:
      msgptr = sinterrupted_resumable;
      break;
    case 3:
      msgptr = sinterrupted_non_resumable;
      break;
    case 4:
      msgptr = simage_completed;
      break;
    default:
      msgptr = "";
    }
  putstring(s_row,45,C_GENERAL_HI,msgptr);
  if (initbatch && calc_status != 0)
    putstring(-1,-1,C_GENERAL_HI,sbatch);

  if (helpmode == HELPCYCLING)
    putstring(s_row+1,45,C_GENERAL_HI,syou_are_cycling);
  ++s_row;
  /* if(bf_math == 0) */
  ++s_row;

  i = j = 0;
  if (display3d > 0)
    {
      if (usr_floatflag)
        j = 1;
    }
  else
    if (floatflag)
      j = (usr_floatflag) ? 1 : 2;
  if (bf_math==0)
    {
      if (j)
        {
          putstring(s_row,45,C_GENERAL_HI,sfloating_point);

          putstring(-1,-1,C_GENERAL_HI,(j == 1) ? sflag_is_activated
                    : sin_use_required );
          i = 1;
        }
      else
        {
          putstring(s_row,45,C_GENERAL_HI,sinteger_math);
          i = 1;
        }
    }
  else
    {
      sprintf(msg,"(%-d decimals)",decimals /*getprecbf(CURRENTREZ)*/);
      putstring(s_row,45,C_GENERAL_HI,sarbitrary_precision);
      putstring(-1,-1,C_GENERAL_HI,msg);

      i = 1;
    }

  s_row += i;

  if (calc_status == 1 || calc_status == 2)
    if (curfractalspecific->flags&NORESUME)
      {
        static FCODE msg[] = {"Note: can't resume this type after interrupts other than <tab> and <F1>"};
        putstring(s_row++,2,C_GENERAL_HI,msg);
      }
  s_row += addrow;
  putstring(s_row,2,C_GENERAL_MED,ssavename);
  putstring(s_row,-1,C_GENERAL_HI,savename);

  /* if(bf_math == 0) */
  ++s_row;

  if (got_status >= 0 && (calc_status == 1 || calc_status == 2))
    {
      switch (got_status)
        {
        case 0:
          sprintf(msg,"%d Pass Mode",totpasses);
          putstring(s_row,2,C_GENERAL_HI,msg);
          if (usr_stdcalcmode=='3')
            putstring(s_row,-1,C_GENERAL_HI,sthreepass);
          break;
        case 1:
          putstring(s_row,2,C_GENERAL_HI,ssolid_guessing);
          if (usr_stdcalcmode=='3')
            putstring(s_row,-1,C_GENERAL_HI,sthreepass);
          break;
        case 2:
          putstring(s_row,2,C_GENERAL_HI,sboundary_tracing);
          break;
        case 3:
          sprintf(msg,"Processing row %d (of %d) of input image",currow,fileydots);
          putstring(s_row,2,C_GENERAL_HI,msg);
          break;
        case 4:
          putstring(s_row,2,C_GENERAL_HI,stesseral);
          break;
        case 5:
          putstring(s_row,2,C_GENERAL_HI,sdiffusion);
          break;
        case 6:
          putstring(s_row,2,C_GENERAL_HI,sorbits);
          break;
        }
      ++s_row;
      if (got_status == 5 )
        {
          sprintf(msg,"%2.2f%% done, counter at %lu of %lu (%u bits)",
                  (100.0 * dif_counter)/dif_limit,
                  dif_counter,dif_limit,bits);
          putstring(s_row,2,C_GENERAL_MED,msg);
          ++s_row;
        }
      else
        if (got_status != 3)
          {
            sprintf(msg,"Working on block (y,x) [%d,%d]...[%d,%d], ",
                    yystart,xxstart,yystop,xxstop);
            putstring(s_row,2,C_GENERAL_MED,msg);
            if (got_status == 2 || got_status == 4)   /* btm or tesseral */
              {
                putstring(-1,-1,C_GENERAL_MED,"at ");
                sprintf(msg,"[%d,%d]",currow,curcol);
                putstring(-1,-1,C_GENERAL_HI,msg);
              }
            else
              {
                if (totpasses > 1)
                  {
                    putstring(-1,-1,C_GENERAL_MED,"pass ");
                    sprintf(msg,"%d",curpass);
                    putstring(-1,-1,C_GENERAL_HI,msg);
                    putstring(-1,-1,C_GENERAL_MED," of ");
                    sprintf(msg,"%d",totpasses);
                    putstring(-1,-1,C_GENERAL_HI,msg);
                    putstring(-1,-1,C_GENERAL_MED,", ");
                  }
                putstring(-1,-1,C_GENERAL_MED,"at row ");
                sprintf(msg,"%d",currow);
                putstring(-1,-1,C_GENERAL_HI,msg);
                putstring(-1,-1,C_GENERAL_MED," col ");
                sprintf(msg,"%d",col);
                putstring(-1,-1,C_GENERAL_HI,msg);
              }
            ++s_row;
          }
    }
  putstring(s_row,2,C_GENERAL_MED,scalculation_time);
  get_calculation_time(msg,calctime);
  putstring(-1,-1,C_GENERAL_HI,msg);
  if ((got_status == 5) && (calc_status == 1))   /* estimate total time */
    {
      putstring(-1,-1,C_GENERAL_MED," estimated total time: ");
      get_calculation_time( msg,(long)(calctime*((dif_limit*1.0)/dif_counter)) );
      putstring(-1,-1,C_GENERAL_HI,msg);
    }

  if ((curfractalspecific->flags&INFCALC) && (coloriter != 0))
    {
      putstring(s_row,-1,C_GENERAL_MED,siterations);
      sprintf(msg," %ld of %ld",coloriter-2,maxct);
      putstring(s_row,-1,C_GENERAL_HI,msg);
    }

  ++s_row;
  if (bf_math == 0)
    ++s_row;
  if (bf_math==0)
    {
      sprintf(msg,"Video: %dx%dx%d",
              xdots, ydots, colors);
      putstring(s_row++,2,C_GENERAL_MED,msg);
    }
  if (!(curfractalspecific->flags&NOZOOM))
    {
      adjust_corner(); /* make bottom left exact if very near exact */
      if (bf_math)
        {
          int truncate, truncaterow;
          dec = min(320,decimals);
          adjust_cornerbf(); /* make bottom left exact if very near exact */
          cvtcentermagbf(bfXctr, bfYctr, &Magnification, &Xmagfactor, &Rotation, &Skew);
          /* find alignment information */
          msg[0] = 0;
          truncate = 0;
          if (dec < decimals)
            truncate = 1;
          truncaterow = row;
          putstring(++s_row,2,C_GENERAL_MED,scenter);
          putstring(s_row,8,C_GENERAL_MED,s_x);
          bftostr(msg,dec,bfXctr);
          if (putstringwrap(&s_row,10,78,C_GENERAL_HI,msg,5)==1)
            truncate = 1;
          putstring(++s_row,8,C_GENERAL_MED,s_y);
          bftostr(msg,dec,bfYctr);
          if (putstringwrap(&s_row,10,78,C_GENERAL_HI,msg,5)==1 || truncate)
            putstring(truncaterow,2,C_GENERAL_MED,struncate);
          putstring(++s_row,2,C_GENERAL_MED,smag);
#ifdef USE_LONG_DOUBLE
          sprintf(msg,"%10.8Le",Magnification);
#else
          sprintf(msg,"%10.8le",Magnification);
#endif
          putstring(-1,11,C_GENERAL_HI,msg);
          putstring(++s_row,2,C_GENERAL_MED,sxmag);
          sprintf(msg,"%11.4f   ",Xmagfactor);
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(-1,-1,C_GENERAL_MED,srot);
          sprintf(msg,"%9.3f   ",Rotation);
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(-1,-1,C_GENERAL_MED,sskew);
          sprintf(msg,"%9.3f",Skew);
          putstring(-1,-1,C_GENERAL_HI,msg);
        }
      else /* bf != 1 */
        {
          putstring(s_row,2,C_GENERAL_MED,scornersxy);
          putstring(++s_row,3,C_GENERAL_MED,stop_left);
          sprintf(msg,"%20.16f  %20.16f",xxmin,yymax);
          putstring(-1,17,C_GENERAL_HI,msg);
          putstring(++s_row,3,C_GENERAL_MED,sbottom_right);
          sprintf(msg,"%20.16f  %20.16f",xxmax,yymin);
          putstring(-1,17,C_GENERAL_HI,msg);

          if (xxmin != xx3rd || yymin != yy3rd)
            {
              putstring(++s_row,3,C_GENERAL_MED,sbottom_left);
              sprintf(msg,"%20.16f  %20.16f",xx3rd,yy3rd);
              putstring(-1,17,C_GENERAL_HI,msg);
            }
          cvtcentermag(&Xctr, &Yctr, &Magnification, &Xmagfactor, &Rotation, &Skew);
          putstring(s_row+=2,2,C_GENERAL_MED,scenter);
          sprintf(msg,"%20.16f %20.16f  ",Xctr,Yctr);
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(-1,-1,C_GENERAL_MED,smag);
#ifdef USE_LONG_DOUBLE
          sprintf(msg," %10.8Le",Magnification);
#else
          sprintf(msg," %10.8le",Magnification);
#endif
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(++s_row,2,C_GENERAL_MED,sxmag);
          sprintf(msg,"%11.4f   ",Xmagfactor);
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(-1,-1,C_GENERAL_MED,srot);
          sprintf(msg,"%9.3f   ",Rotation);
          putstring(-1,-1,C_GENERAL_HI,msg);
          putstring(-1,-1,C_GENERAL_MED,sskew);
          sprintf(msg,"%9.3f",Skew);
          putstring(-1,-1,C_GENERAL_HI,msg);

        }
    }

  if (typehasparm(fractype,0,msg) || hasformparam)
    for (i = 0; i < MAXPARAMS; i++)
      {
        int col;
        char p[50];
        if (typehasparm(fractype,i,p))
          {
            if (k%4 == 0)
              {
                s_row++;
                col = 9;
              }
            else
              col = -1;
            if (k == 0) /* only true with first displayed parameter */
              putstring(++s_row,2,C_GENERAL_MED,sparams);
            sprintf(msg,"%3d: ",i+1);
            putstring(s_row,col,C_GENERAL_MED,msg);
            if (*p == '+')
              sprintf(msg,"%-12d",(int)param[i]);
            else if (*p == '#')
              sprintf(msg,"%-12lu",(U32)param[i]);
            else
              sprintf(msg,"%-12.9f",param[i]);
            putstring(-1,-1,C_GENERAL_HI,msg);
            k++;
          }
      }
  putstring(s_row+=2,2,C_GENERAL_MED,siteration_maximum);
  sprintf(msg,"%ld (%ld)",coloriter,maxit);
  putstring(-1,-1,C_GENERAL_HI,msg);
  putstring(-1,-1,C_GENERAL_MED,seffective_bailout);
  sprintf(msg,"%f",rqlim);
  putstring(-1,-1,C_GENERAL_HI,msg);

  if (fractype == PLASMA || fractype == ANT || fractype == CELLULAR)
    {
      putstring(++s_row,2,C_GENERAL_MED,scurrent_rseed);
      sprintf(msg,"%d",rseed);
      putstring(-1,-1,C_GENERAL_HI,msg);
    }

  if (invert)
    {
      putstring(++s_row,2,C_GENERAL_MED,sinversion_radius);
      sprintf(msg,"%12.9f",f_radius);
      putstring(-1,-1,C_GENERAL_HI,msg);
      putstring(-1,-1,C_GENERAL_MED,sxcenter);
      sprintf(msg,"%12.9f",f_xcenter);
      putstring(-1,-1,C_GENERAL_HI,msg);
      putstring(-1,-1,C_GENERAL_MED,sycenter);
      sprintf(msg,"%12.9f",f_ycenter);
      putstring(-1,-1,C_GENERAL_HI,msg);
    }

  if ((s_row += 2) < 23) ++s_row;
  /*waitforkey:*/
  putstringcenter(/*s_row*/24,0,80,C_GENERAL_LO,spressanykey);
  movecursor(25,80);
#ifdef XFRACT
  while (keypressed())
    {
      getakey();
    }
#endif
  key = getakeynohelp();
  if (key==F6)
    {
      unstackscreen();
      area();
      stackscreen();
      /*       goto waitforkey;*/
      goto top;
    }
  else if (key==CTL_TAB || key==BACK_TAB || key==F7)
    {
      if (tab_display_2(msg))
        goto top;
    }
  unstackscreen();
  timer_start = clock_ticks(); /* tab display was "time out" */
  if (bf_math)
    {
      restore_stack(saved);
    }
  return(0);
}

static void area(void)
{
  /* apologies to UNIX folks, we PC guys have to save near space */
  static FCODE warning[] = {"Warning: inside may not be unique\n"};
  static FCODE total_area[] = {".  Total area "};
  char *msg;
  int x,y;
  char buf[160];
  long cnt=0;
  if (inside<0)
    {
      static FCODE msg[] = {"Need solid inside to compute area"};
      stopmsg(0,msg);
      return;
    }
  for (y=0;y<ydots;y++)
    {
      for (x=0;x<xdots;x++)
        {
          if (getcolor(x,y)==inside)
            {
              cnt++;
            }
        }
    }
  if (inside>0 && outside<0 && maxit>inside)
    {
      msg = warning;
    }
  else
    {
      msg = (char far *)"";
    }
#ifndef XFRACT
  sprintf(buf,"%Fs%ld inside pixels of %ld%Fs%f",
          msg,cnt,(long)xdots*(long)ydots,(char far *)total_area,
          cnt/((float)xdots*(float)ydots)*(xxmax-xxmin)*(yymax-yymin));
#else
  sprintf(buf,"%s%ld inside pixels of %ld%s%f",
          msg,cnt,(long)xdots*(long)ydots,total_area,
          cnt/((float)xdots*(float)ydots)*(xxmax-xxmin)*(yymax-yymin));
#endif
  stopmsg(4,buf);
}

int endswithslash(char *fl)
{
  int len;
  len = strlen(fl);
  if (len)
    if (fl[--len] == SLASHC)
      return(1);
  return(0);
}

/* --------------------------------------------------------------------- */
static char seps[] = {"' ','\t',\n',\r'"};
char *get_ifs_token(char *buf,FILE *ifsfile)
{
  char *bufptr;
  for (;;)
    {
      if (file_gets(buf,200,ifsfile) < 0)
        return(NULL);
      else
        {
          if ((bufptr = strchr(buf,';')) != NULL) /* use ';' as comment to eol */
            *bufptr = 0;
          if ((bufptr = strtok(buf, seps)) != NULL)
            return(bufptr);
        }
    }
}

FCODE insufficient_ifs_mem[]={"Insufficient memory for IFS"};
int numaffine;
int ifsload()                   /* read in IFS parameters */
{
  int i;
  FILE *ifsfile;
  char buf[201];
  char *bufptr;
  int ret,rowsize;

  if (ifs_defn)   /* release prior parms */
    {
      farmemfree((char far *)ifs_defn);
      ifs_defn = NULL;
    }

  ifs_type = 0;
  rowsize = IFSPARM;
  if (find_file_item(IFSFileName,IFSName,&ifsfile, 3) < 0)
    return(-1);

  file_gets(buf,200,ifsfile);
  if ((bufptr = strchr(buf,';')) != NULL) /* use ';' as comment to eol */
    *bufptr = 0;

  strlwr(buf);
  bufptr = &buf[0];
  while (*bufptr)
    {
      if (strncmp(bufptr,"(3d)",4) == 0)
        {
          ifs_type = 1;
          rowsize = IFS3DPARM;
        }
      ++bufptr;
    }

  for (i = 0; i < (NUMIFS+1)*IFS3DPARM; ++i)
    ((float *)tstack)[i] = 0;
  i = ret = 0;
  bufptr = get_ifs_token(buf,ifsfile);
  while (bufptr != NULL)
    {
      if (sscanf(bufptr," %f ",&((float *)tstack)[i]) != 1)
        break ;
      if (++i >= NUMIFS*rowsize)
        {
          static FCODE msg[]={"IFS definition has too many lines"};
          stopmsg(0,msg);
          ret = -1;
          break;
        }
      if ((bufptr = strtok( NULL, seps ))==NULL)
        {
          if ((bufptr = get_ifs_token(buf,ifsfile)) == NULL)
            {
              ret = -1;
              break;
            }
        }
      if (ret == -1)
        break;
      if (*bufptr == '}')
        break;
    }

  if ((i % rowsize) != 0 || *bufptr != '}')
    {
      static FCODE msg[]={"invalid IFS definition"};
      stopmsg(0,msg);
      ret = -1;
    }
  if (i == 0 && ret == 0)
    {
      static FCODE msg[]={"Empty IFS definition"};
      stopmsg(0,msg);
      ret = -1;
    }
  fclose(ifsfile);

  if (ret == 0)
    {
      numaffine = i/rowsize;
      if ((ifs_defn = (float far *)farmemalloc(
                        (long)((NUMIFS+1)*IFS3DPARM*sizeof(float)))) == NULL)
        {
          stopmsg(0,insufficient_ifs_mem);
          ret = -1;
        }
      else
        for (i = 0; i < (NUMIFS+1)*IFS3DPARM; ++i)
          ifs_defn[i] = ((float *)tstack)[i];
    }
  return(ret);
}
/* TW 5-31-94 - added search of current directory for entry files if
   entry item not found */

int find_file_item(char *filename,char *itemname,FILE **fileptr, int itemtype)
{
  FILE *infile=NULL;
  int found = 0;
  char parsearchname[ITEMNAMELEN + 6];
  char drive[FILE_MAX_DRIVE];
  char dir[FILE_MAX_DIR];
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];
  char fullpath[FILE_MAX_PATH];
  char defaultextension[FILE_MAX_EXT];

  splitpath(filename,drive,dir,fname,ext);
  makepath(fullpath,"","",fname,ext);
  if (stricmp(filename, CommandFile))
    {
      if ((infile=fopen(filename, "rb")) != NULL)
        {
          if (scan_entries(infile, NULL, itemname) == -1)
            {
              found = 1;
            }
          else
            {
              fclose(infile);
              infile = NULL;
            }
        }

      if (!found && checkcurdir)
        {
          makepath(fullpath,"",DOTSLASH,fname,ext);
          if ((infile=fopen(fullpath, "rb")) != NULL)
            {
              if (scan_entries(infile, NULL, itemname) == -1)
                {
                  strcpy(filename, fullpath);
                  found = 1;
                }
              else
                {
                  fclose(infile);
                  infile = NULL;
                }
            }
        }
    }

  switch (itemtype)
    {
    case 1:
      strcpy(parsearchname, "frm:");
      strcat(parsearchname, itemname);
      parsearchname[ITEMNAMELEN + 5] = (char) 0; /*safety*/
      strcpy(defaultextension, ".frm");
      splitpath(searchfor.frm,drive,dir,NULL,NULL);
      break;
    case 2:
      strcpy(parsearchname, "lsys:");
      strcat(parsearchname, itemname);
      parsearchname[ITEMNAMELEN + 5] = (char) 0; /*safety*/
      strcpy(defaultextension, ".l");
      splitpath(searchfor.lsys,drive,dir,NULL,NULL);
      break;
    case 3:
      strcpy(parsearchname, "ifs:");
      strcat(parsearchname, itemname);
      parsearchname[ITEMNAMELEN + 5] = (char) 0; /*safety*/
      strcpy(defaultextension, ".ifs");
      splitpath(searchfor.ifs,drive,dir,NULL,NULL);
      break;
    default:
      strcpy(parsearchname, itemname);
      parsearchname[ITEMNAMELEN + 5] = (char) 0; /*safety*/
      strcpy(defaultextension, ".par");
      splitpath(searchfor.par,drive,dir,NULL,NULL);
      break;
    }

  if (!found)
    {
      if ((infile=fopen(CommandFile, "rb")) != NULL)
        {
          if (scan_entries(infile, NULL, parsearchname) == -1)
            {
              strcpy(filename, CommandFile);
              found = 1;
            }
          else
            {
              fclose(infile);
              infile = NULL;
            }
        }
    }

  if (!found)
    {
      makepath(fullpath,drive,dir,fname,ext);
      if ((infile=fopen(fullpath, "rb")) != NULL)
        {
          if (scan_entries(infile, NULL, itemname) == -1)
            {
              strcpy(filename, fullpath);
              found = 1;
            }
          else
            {
              fclose(infile);
              infile = NULL;
            }
        }
    }

  if (!found)   /* search for file */
    {
      int out;
      makepath(fullpath,drive,dir,"*",defaultextension);
      out = fr_findfirst(fullpath);
      while (out == 0)
        {
          char msg[200];
          DTA.filename[MAX_NAME-1]=0;
          if (!(DTA.attribute & SUBDIR) &&
              strcmp(DTA.filename,".")&&
              strcmp(DTA.filename,".."))
            {
              sprintf(msg,"Searching %s for %s      ",DTA.filename,itemname);
              showtempmsg(msg);
#ifndef XFRACT
              strlwr(DTA.filename);
#endif
              splitpath(DTA.filename,NULL,NULL,fname,ext);
              makepath(fullpath,drive,dir,fname,ext);
              if ((infile=fopen(fullpath, "rb")) != NULL)
                {
                  if (scan_entries(infile, NULL, itemname) == -1)
                    {
                      strcpy(filename, fullpath);
                      found = 1;
                      break;
                    }
                  else
                    {
                      fclose(infile);
                      infile = NULL;
                    }
                }
            }
          out = fr_findnext();
        }
      cleartempmsg();
    }

  if (!found && orgfrmsearch && itemtype == 1)
    {
      splitpath(orgfrmdir,drive,dir,NULL,NULL);
      fname[0] = '_';
      fname[1] = (char) 0;
      if (isalpha(itemname[0]))
        {
          if (strnicmp(itemname, "carr", 4))
            {
              fname[1] = itemname[0];
              fname[2] = (char) 0;
            }
          else if (isdigit(itemname[4]))
            {
              strcat(fname, "rc");
              fname[3] = itemname[4];
              fname[4] = (char) 0;
            }
          else
            {
              strcat(fname, "rc");
            }
        }
      else if (isdigit(itemname[0]))
        {
          strcat(fname, "num");
        }
      else
        {
          strcat(fname, "chr");
        }
      makepath(fullpath,drive,dir,fname,defaultextension);
      if ((infile=fopen(fullpath, "rb")) != NULL)
        {
          if (scan_entries(infile, NULL, itemname) == -1)
            {
              strcpy(filename, fullpath);
              found = 1;
            }
          else
            {
              fclose(infile);
              infile = NULL;
            }
        }
    }

  if (!found)
    {
      sprintf(fullpath,"'%s' file entry item not found",itemname);
      stopmsg(0,fullpath);
      return(-1);
    }
  /* found file */
  if (fileptr != NULL)
    *fileptr = infile;
  else if (infile != NULL)
    fclose(infile);
  return(0);
}

int file_gets(char *buf,int maxlen,FILE *infile)
{
  int len,c;
  /* similar to 'fgets', but file may be in either text or binary mode */
  /* returns -1 at eof, length of string otherwise */
  if (feof(infile)) return -1;
  len = 0;
  while (len < maxlen)
    {
      if ((c = getc(infile)) == EOF || c == '\032')
        {
          if (len) break;
          return -1;
        }
      if (c == '\n') break;             /* linefeed is end of line */
      if (c != '\r') buf[len++] = (char)c;    /* ignore c/r */
    }
  buf[len] = 0;
  return len;
}

int matherr_ct = 0;

#ifndef XFRACT
#ifdef WINFRACT
/* call this something else to dodge the QC4WIN bullet... */
int win_matherr( struct exception *except )
#else
int _cdecl _matherr( struct exception *except )
#endif
{
  if (debugflag != 0)
    {
      static FILE *fp=NULL;
      static FCODE msg[]={"Math error, but we'll try to keep going"};
      if (matherr_ct++ == 0)
        if (debugflag == 4000 || debugflag == 3200)
          stopmsg(0,msg);
      if (fp==NULL)
        fp = fopen("matherr","w");
      if (matherr_ct < 100)
        {
          fprintf(fp,"err #%d:  %d\nname: %s\narg:  %e\n",
                  matherr_ct, except->type, except->name, except->arg1);
          fflush(fp);
        }
      else
        matherr_ct = 100;

    }
  if ( except->type == DOMAIN )
    {
      char buf[40];
      sprintf(buf,"%e",except->arg1);
      /* This test may be unnecessary - from my experiments if the
         argument is too large or small the error is TLOSS not DOMAIN */
      if (strstr(buf,"IN")||strstr(buf,"NAN")) /* trashed arg? */
        /* "IND" with MSC, "INF" with BC++ */
        {
          if ( strcmp( except->name, s_sin ) == 0 )
            {
              except->retval = 0.0;
              return(1);
            }
          else if ( strcmp( except->name, s_cos ) == 0 )
            {
              except->retval = 1.0;
              return(1);
            }
          else if ( strcmp( except->name, s_log ) == 0 )
            {
              except->retval = 1.0;
              return(1);
            }
        }
    }
  if ( except->type == TLOSS )
    {
      /* try valiantly to keep going */
      if ( strcmp( except->name, s_sin ) == 0 )
        {
          except->retval = 0.5;
          return(1);
        }
      else if ( strcmp( except->name, s_cos ) == 0 )
        {
          except->retval = 0.5;
          return(1);
        }
    }
  /* shucks, no idea what went wrong, but our motto is "keep going!" */
  except->retval = 1.0;
  return(1);
}
#endif

void roundfloatd(double *x) /* make double converted from float look ok */
{
  char buf[30];
  sprintf(buf,"%-10.7g",*x);
  *x = atof(buf);
}

void fix_inversion(double *x) /* make double converted from string look ok */
{
  char buf[30];
  sprintf(buf,"%-1.15lg",*x);
  *x = atof(buf);
}

/* fake a keystroke, returns old pending key */
int ungetakey(int key)
{
  int old;
  old = keybuffer;
  keybuffer = key;
  return(old);
}
