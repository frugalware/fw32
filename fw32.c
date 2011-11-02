#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

static const char *FW32_ROOT = "/usr/lib/fw32";

static const char *FW32_DIRS[] =
{
  "/proc",
  "/sys",
  "/dev",
  "/etc",
  "/home",
  "/tmp",
  "/var/tmp",
  "/var/cache/pacman-g2/pkg",
  "/usr/share/kde",
  "/usr/share/icons",
  "/usr/share/fonts",
  "/usr/share/themes",
  "/media",
  "/mnt"
};

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

  for( p = strchr(path + 1,'/') ; p && *p ; *p = '/', p = strchr(p + 1,'/') )
  {
    *p = 0;

    if(!stat(path,&st))
    {
      if(S_ISDIR(st.st_mode))
        continue;

      error("Parent directory exists and is not a directory: %s\n",path);
    }

    if(mkdir(path,0755))
      error("Failed to create parent directory: %s\n",path);
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

static bool
is_mounted(const char *path)
{
  FILE *f;
  char line[LINE_MAX], *s, *e;
  bool found;

  assert(path);

  f = fopen("/proc/mounts","rb");

  if(!f)
    error("Cannot open /proc/mounts for reading.\n");

  found = false;

  while(fgets(line,sizeof line,f))
  {
    s = strchr(line,' ');

    if(!s)
      continue;

    e = strchr(++s,' ');

    if(!e)
      continue;

    *e = 0;

    if(strcmp(s,path))
    {
      found = true;

      break;
    }
  }

  fclose(f);

  return found;
}

int main(int argc,char **argv) { mkdir_parents(argv[1]); }
