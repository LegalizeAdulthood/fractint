/* This file contains all the file access routines */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "port.h"
#include "prototyp.h"

#define   FILEATTR       0x37      /* File attributes; select all but volume labels */
#define   HIDDEN         2
#define   SYSTEM         4
#define   SUBDIR         16
#define   MAXNUMFILES    2977L

struct DIR_SEARCH DTA;          /* Allocate DTA and define structure */

static char searchdir[FILE_MAX_DIR];
static char searchname[FILE_MAX_PATH];
static char searchext[FILE_MAX_EXT];
static DIR *currdir = NULL;

static char getdriveletter(void)
{
  char *dummy; /* to quiet compiler */
  char curdir[FILE_MAX_DIR+1];

  dummy = getcwd(curdir,FILE_MAX_DIR);
  return ((char)tolower(curdir[0]));
}

int isadirectory(char *s)
{
  int len;
  char sv;

  if (strchr(s,'*') || strchr(s,'?'))
    return(0); /* for my purposes, not a directory */

  len = strlen(s);
  if (len > 0)
    sv = s[len-1];   /* last char */
  else
    sv = 0;

#if 0
  if (_dos_getfileattr(s, &attrib) == 0 && ((attrib&_A_SUBDIR) != 0))
    {
      return(1);  /* not a directory or doesn't exist */
    }
  else if (sv == SLASHC)
    {
      /* strip trailing slash and try again */
      s[len-1] = 0;
      if (_dos_getfileattr(s, &attrib) == 0 && ((attrib&_A_SUBDIR) != 0))
        {
          s[len-1] = sv;
          return(1);
        }
      s[len-1] = sv;
    }
  return(0);
#else
  if (fr_findfirst(s) != 0) /* couldn't find it */
    {
      /* any better ideas?? */
      if (sv == SLASHC) /* we'll guess it is a directory */
        return(1);
      else
        return(0);  /* no slashes - we'll guess it's a file */
    }
  else if ((DTA.attribute & SUBDIR) != 0)
    {
      if (sv == SLASHC)
        {
          /* strip trailing slash and try again */
          s[len-1] = 0;
          if (fr_findfirst(s) != 0) /* couldn't find it */
            return(0);
          else if ((DTA.attribute & SUBDIR) != 0)
            return(1);   /* we're SURE it's a directory */
          else
            return(0);
        }
      else
        return(1);   /* we're SURE it's a directory */
    }
  return(0);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * findpath --
 *
 *      Find where a file is.
 *	We return filename if it is an absolute path.
 *	Otherwise we first try FRACTDIR/filename, SRCDIR/filename,
 *      and then ./filename.
 *
 * Results:
 *      Returns full pathname in fullpathname.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
#ifdef XFRACT
void findpath(char *filename, char *fullpathname)
{
  int fd;
  char *fractdir;

  if (filename[0]==SLASHC)
    {
      strcpy(fullpathname,filename);
      fd = open(fullpathname,O_RDONLY);
      if (fd != -1)
        {
          close(fd);
          return;
        }
    }
  fractdir = getenv("FRACTDIR");
  if (fractdir != NULL)
    {
      if (strlen(fractdir) > FILE_MAX_DIR)
        {
          char msg[120];
          sprintf(msg, "Warning: Length of Directory set by FRACTDIR exceeds %d characters.", FILE_MAX_DIR);
          popup_error(3, msg);
          fullpathname[0] = 0; /* indicate failure */
          return;
        }
      strcpy(fullpathname,fractdir);
      strcat(fullpathname,SLASH);
      strcat(fullpathname,filename);
      fd = open(fullpathname,O_RDONLY);
      if (fd != -1)
        {
          close(fd);
          return;
        }
    }
// FIXME (jonathan#1#): This will need to be fixed.  02/14/2010
//  strcpy(fullpathname,SRCDIR);
//  strcat(fullpathname,SLASH);
//  strcat(fullpathname,filename);
//  fd = open(fullpathname,O_RDONLY);
//  if (fd != -1)
//    {
//      close(fd);
//      return;
//    }
  strcpy(fullpathname,DOTSLASH);
  strcat(fullpathname,filename);
  fd = open(fullpathname,O_RDONLY);
  if (fd != -1)
    {
      close(fd);
      return;
    }
  fullpathname=NULL;
}
#else
void findpath(char *filename, char *fullpathname) /* return full pathnames */
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

  strcpy(temp_path,filename);   /* avoid side effect changes to filename */

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
  _searchenv(temp_path,"PATH",fullpathname);
  if (fullpathname[0] != 0)                    /* found it! */
    if (strncmp(&fullpathname[2],SLASHSLASH,2) == 0) /* stupid klooge! */
      strcpy(&fullpathname[3],temp_path);
}
#endif  /*  XFRACT  */

/*
 *----------------------------------------------------------------------
 *
 * splitpath --
 *
 *      This is the splitpath code from prompts.c
 *
 * Results:
 *      Returns drive, dir, base, and extension.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int splitpath(char *template,char *drive,char *dir,char *fname,char *ext)
{
  int length;
  int len;
  int offset;
  char *tmp;
  if (drive)
    drive[0] = 0;
  if (dir)
    dir[0]   = 0;
  if (fname)
    fname[0] = 0;
  if (ext)
    ext[0]   = 0;

  if ((length = strlen(template)) == 0)
    return(0);

  offset = 0;

  /* get drive */
  if (length >= 2)
    if (template[1] == ':')
      {
        if (drive)
          {
            drive[0] = template[offset++];
            drive[1] = template[offset++];
            drive[2] = 0;
          }
        else
          {
            offset++;
            offset++;
          }
      }

  /* get dir */
  if (offset < length)
    {
      tmp = strrchr(template,SLASHC);
      if (tmp)
        {
          tmp++;  /* first character after slash */
          len = tmp - (char *)&template[offset];
          if (len >= 0 && len < FILE_MAX_DIR && dir)
            strncpy(dir,&template[offset],min(len,FILE_MAX_DIR));
          if (len < FILE_MAX_DIR && dir)
            dir[len] = 0;
          offset += len;
        }
    }
  else
    return(0);

  /* get fname */
  if (offset < length)
    {
      tmp = strrchr(template,'.');
      if (tmp < strrchr(template,SLASHC) || tmp < strrchr(template,':'))
        tmp = 0; /* in this case the '.' must be a directory */
      if (tmp)
        {
          /* tmp++; */ /* first character past "." */
          len = tmp - (char *)&template[offset];
          if ((len > 0) && (offset+len < length) && fname)
            {
              strncpy(fname,&template[offset],min(len,FILE_MAX_FNAME));
              if (len < FILE_MAX_FNAME)
                fname[len] = 0;
              else
                fname[FILE_MAX_FNAME-1] = 0;
            }
          offset += len;
          if ((offset < length) && ext)
            {
              strncpy(ext,&template[offset],FILE_MAX_EXT);
              ext[FILE_MAX_EXT-1] = 0;
            }
        }
      else if ((offset < length) && fname)
        {
          strncpy(fname,&template[offset],FILE_MAX_FNAME);
          fname[FILE_MAX_FNAME-1] = 0;
        }
    }
  return(0);
}

int makepath(char *template,char *drive,char *dir,char *fname,char *ext)
{
  if (template)
    *template = 0;
  else
    return(-1);
#ifndef XFRACT
  if (drive)
    strcpy(template,drive);
#endif
  if (dir)
    strcat(template,dir);
  if (fname)
    strcat(template,fname);
  if (ext)
    strcat(template,ext);
  return(0);
}

/* fix up directory names */
void fix_dirname(char *dirname)
{
  int length;
  length = strlen(dirname); /* index of last character */

  /* make sure dirname ends with a slash */
  if (length > 0)
    if (dirname[length-1] == SLASHC)
      return;
  strcat(dirname,SLASH);
}

static void dir_name(char *target, char *dir, char *name)
{
  *target = 0;
  if (*dir != 0)
    strcpy(target,dir);
  strcat(target,name);
}

/* opens file in dir directory */
int dir_open(char *dir, char *filename, int oflag, int pmode)
{
  char tmp[FILE_MAX_PATH];
  dir_name(tmp,dir,filename);
  return(open(tmp,oflag,pmode));
}

/* removes file in dir directory */
int dir_remove(char *dir,char *filename)
{
  char tmp[FILE_MAX_PATH];
  dir_name(tmp,dir,filename);
  return(remove(tmp));
}

/* fopens file in dir directory */
FILE *dir_fopen(char *dir, char *filename, char *mode )
{
  char tmp[FILE_MAX_PATH];
  dir_name(tmp,dir,filename);
  return(fopen(tmp,mode));
}

/* converts relative path to absolute path */
void expand_dirname(char *dirname, char *drive)
{
  char *dummy; /* to quiet compiler */

  fix_dirname(dirname);
  if (dirname[0] != SLASHC)
    {
      char buf[FILE_MAX_DIR+1];
      char curdir[FILE_MAX_DIR+1];
      int i = 2;
      dummy = getcwd(curdir,FILE_MAX_DIR);
      strcat(curdir,SLASH);
#ifndef XFRACT
// NOTE (jonathan#1#): Do we ever need the drive information?  10/29/10
//      drive[0] = (char)tolower(curdir[0]);
//      drive[1] = curdir[1];
//      drive[2] = 0; /* terminate string */
      while (curdir[i] != 0)
        {
          curdir[i-2] = curdir[i]; /* remove drive info */
          i++;
        }
      curdir[i-2] = 0; /* terminate string */
#endif
      while (strncmp(dirname,DOTSLASH,2) == 0)
        {
          strcpy(buf,&dirname[2]);
          strcpy(dirname,buf);
        }
      while (strncmp(dirname,DOTDOTSLASH,3) == 0)
        {
          char *s;
          curdir[strlen(curdir)-1] = 0; /* strip trailing slash */
          if ((s = strrchr(curdir,SLASHC)) != NULL)
            *s = 0;
          strcat(curdir,SLASH);
          strcpy(buf,&dirname[3]);
          strcpy(dirname,buf);
        }
      strcpy(buf,dirname);
      dirname[0] = 0;
      if (curdir[0] != SLASHC)
        strcpy(dirname,SLASH);
      strcat(dirname,curdir);
      strcat(dirname,buf);
    }
}

/* merge existing full path with new one  */
/* attempt to detect if file or directory */

#define ATFILENAME 0
#define SSTOOLSINI 1
#define ATCOMMANDINTERACTIVE 2
#define ATFILENAMESETNAME  3

#define GETPATH (mode < 2)

/* copies the proposed new filename to the fullpath variable */
/* does not copy directories for PAR files (modes 2 and 3)   */
/* attempts to extract directory and test for existence (modes 0 and 1) */
int merge_pathnames(char *oldfullpath, char *newfilename, int mode)
{
  int isadir = 0;
  int isafile = 0;
  int len;
  char drive[FILE_MAX_DRIVE];
  char dir[FILE_MAX_DIR];
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];
  char temp_path[FILE_MAX_PATH];

  char drive1[FILE_MAX_DRIVE];
  char dir1[FILE_MAX_DIR];
  char fname1[FILE_MAX_FNAME];
  char ext1[FILE_MAX_EXT];

  /* no dot or slash so assume a file */
  if (strchr(newfilename,'.')==NULL && strchr(newfilename,SLASHC) == NULL)
    isafile=1;
  if ((isadir = isadirectory(newfilename)) != 0)
    fix_dirname(newfilename);
#if 1
  /* if slash by itself, it's a directory */
  if (strcmp(newfilename,SLASH)==0)
    isadir = 1;
#endif
#if 1
  /* if drive, colon, slash, is a directory */
  if (strlen(newfilename) == 3 &&
      newfilename[1] == ':' &&
      newfilename[2] == SLASHC)
    isadir = 1;
  /* if drive, colon, with no slash, is a directory */
  if (strlen(newfilename) == 2 &&
      newfilename[1] == ':')
    {
      newfilename[2] = SLASHC;
      newfilename[3] = 0;
      isadir = 1;
    }
  /* if dot, slash, '0', its the current directory, set up full path */
  if (newfilename[0] == '.' &&
      newfilename[1] == SLASHC && newfilename[2] == 0)
    {
      temp_path[0] = getdriveletter();
      temp_path[1] = ':';
      temp_path[2] = 0;
      expand_dirname(newfilename,temp_path);
      strcat(temp_path,newfilename);
      strcpy(newfilename,temp_path);
      isadir = 1;
    }
  /* if dot, slash, its relative to the current directory, set up full path */
  if (newfilename[0] == '.' &&
      newfilename[1] == SLASHC)
    {
      int len, test_dir=0;
      temp_path[0] = getdriveletter();
      temp_path[1] = ':';
      temp_path[2] = 0;
      if (strrchr(newfilename,'.') == newfilename)
        test_dir = 1;  /* only one '.' assume its a directory */
      expand_dirname(newfilename,temp_path);
      strcat(temp_path,newfilename);
      strcpy(newfilename,temp_path);
      if (!test_dir)
        {
          len = strlen(newfilename);
          newfilename[len-1] = 0; /* get rid of slash added by expand_dirname */
        }
    }
#else
  findpath(newfilename,temp_path);
  strcpy(newfilename,temp_path);
#endif
  /* check existence */
  if (isadir==0 || isafile==1)
    {
      if (fr_findfirst(newfilename) == 0)
        {
          if (DTA.attribute & SUBDIR) /* exists and is dir */
            {
              fix_dirname(newfilename);  /* add trailing slash */
              isadir = 1;
              isafile = 0;
            }
          else
            isafile = 1;
        }
    }

  splitpath(newfilename,drive,dir,fname,ext);
  splitpath(oldfullpath,drive1,dir1,fname1,ext1);
  if (strlen(drive) != 0 && GETPATH)
    strcpy(drive1,drive);
  if (strlen(dir) != 0 && GETPATH)
    strcpy(dir1,dir);
  if (strlen(fname) != 0)
    strcpy(fname1,fname);
  if (strlen(ext) != 0)
    strcpy(ext1,ext);
  if (isadir == 0 && isafile == 0 && GETPATH)
    {
      makepath(oldfullpath,drive1,dir1,NULL,NULL);
      len = strlen(oldfullpath);
      if (len > 0)
        {
          char save;
          /* strip trailing slash */
          save = oldfullpath[len-1];
          if (save == SLASHC)
            oldfullpath[len-1] = 0;
          if (access(oldfullpath,0))
            isadir = -1;
          oldfullpath[len-1] = save;
        }
    }
  makepath(oldfullpath,drive1,dir1,fname1,ext1);
  return(isadir);
}

/* extract just the filename/extension portion of a path */
void extract_filename(char *target, char *source)
{
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];
  splitpath(source,NULL,NULL,fname,ext);
  makepath(target,"","",fname,ext);
}

/* tells if filename has extension */
/* returns pointer to period or NULL */
char *has_ext(char *source)
{
  char fname[FILE_MAX_FNAME];
  char ext[FILE_MAX_EXT];
  char *ret = NULL;
  splitpath(source,NULL,NULL,fname,ext);
  if (ext != NULL)
    if (*ext != 0)
      ret = strrchr(source,'.');
  return(ret);
}

int  fr_findfirst(char *path)  /* Find 1st file (or subdir) meeting path/filespec */
{
  if (currdir != NULL)
    {
      closedir(currdir);
      currdir = NULL;
    }
  splitpath(path,NULL,searchdir,searchname,searchext);
  if (searchdir[0]=='\0')
    {
      currdir = opendir(".");
    }
  else
    {
      currdir = opendir(searchdir);
    }
  if (currdir==NULL)
    {
      return -1;
    }
  else
    {
      return fr_findnext();
    }

}

int  fr_findnext(void)  /* Find next file (or subdir) meeting above path/filespec */
{
  struct dirent *dirEntry;
  struct stat sbuf;
  char thisname[FILE_MAX_PATH];
  char tmpname[FILE_MAX_PATH];
  char thisext[FILE_MAX_EXT];
  for (;;)
    {
      dirEntry = readdir(currdir);
      if (dirEntry == NULL)
        {
          closedir(currdir);
          currdir = NULL;
          return -1;
        }
#ifdef XFRACT
      else if (dirEntry->d_ino != 0)
#else
      else
#endif
        {
          splitpath(dirEntry->d_name,NULL,NULL,thisname,thisext);
          strncpy(DTA.filename,dirEntry->d_name,MAX_NAME);
          DTA.filename[MAX_NAME-1]='\0';
          strcpy(tmpname,searchdir);
          strcat(tmpname,dirEntry->d_name);
          stat(tmpname,&sbuf);
          DTA.size = sbuf.st_size;
#ifdef XFRACT
          if ((sbuf.st_mode&__S_IFMT)==__S_IFREG &&
#else
          if ((sbuf.st_mode&S_IFMT)==S_IFREG &&
#endif
              (searchname[0]=='*' || stricmp(searchname,thisname)==0) &&
              (searchext[0]=='*' || stricmp(searchext,thisext)==0))
            {
              DTA.attribute = 0;
              return 0;
            }
#ifdef XFRACT
          else if (((sbuf.st_mode&__S_IFMT)==__S_IFDIR) &&
#else
          else if (((sbuf.st_mode&S_IFMT)==S_IFDIR) &&
#endif
                   ((searchname[0]=='*' || searchext[0]=='*') ||
                    (stricmp(searchname,thisname)==0)))
            {
              DTA.attribute = SUBDIR;
              return 0;
            }
        }
    }
}
