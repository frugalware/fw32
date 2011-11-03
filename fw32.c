#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

static const char *FW32_ROOT = "/usr/lib/fw32";

static const char *FW32_CONFIG = "/etc/fw32/pacman-g2.conf";

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

  assert(s && *s == '/');

  snprintf(path,sizeof path,"%s",s);

  for( p = strchr(path + 1,'/') ; p && *p ; *p = '/', p = strchr(p + 1,'/') )
  {
    *p = 0;

    if(mkdir(path,0755) && errno != EEXIST)
        error("Failed to create parent directory: %s: %s\n",path,strerror(errno));
  }

  if(mkdir(path,0755) && errno != EEXIST)
    error("Failed to create directory: %s: %s\n",path,strerror(errno));
}

static bool
ismounted(const char *path)
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

static void
mount_directory(const char *src)
{
  char dst[PATH_MAX];

  assert(src);
  
  snprintf(dst,sizeof dst,"%s%s",FW32_ROOT,src);

  if(ismounted(dst))
    return;
  
  if(mount(src,dst,"",MS_BIND,""))
    error("Failed to mount directory: %s: %s\n",dst,strerror(errno));
}

static void
umount_directory(const char *path)
{
  assert(path);

  if(umount2(path,UMOUNT_NOFOLLOW) && errno != EINVAL)
    error("Failed to umount directory: %s: %s\n",path,strerror(errno));
}

int main(int argc,char **argv) { umount_directory(argv[1]); }
