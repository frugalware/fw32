#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

static void
error(const char *fmt,...)
{
  va_list args;

  assert(fmt);

  va_start(args,fmt);

  vfprintf(stderr,fmt,args);

  va_end(args);

  exit(EXIT_FAILURE);
}

static void
mkdir_parents(const char *s)
{
  char path[PATH_MAX], *p;
  struct stat st;

  assert(s && *s == '/');

  snprintf(path,sizeof path,"%s",s);

  for( p = strchr(path + 1,'/') ; p && *p ; p = strchr(p + 1,'/') )
  {
    *p = 0;

    if(!stat(path,&st))
    {
      if(S_ISDIR(st.st_mode))
      {
        *p = '/';

        continue;
      }

      error("Parent directory exists and is not a directory: %s\n",path);
    }

    if(mkdir(path,0755))
      error("Failed to create parent directory: %s\n",path);

    *p = '/';
  }

  if(!stat(path,&st))
  {
    if(S_ISDIR(st.st_mode))
      return;

    error("Directory exists and is not a directory: %s\n",path);
  }

  if(mkdir(path,0755))
    error("Failed to create directory: %s\n",path);
}
