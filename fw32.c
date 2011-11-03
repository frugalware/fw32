#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/personality.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
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
  "/mnt",
  0
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

static void *
xmalloc(size_t n)
{
  void *p;

  assert(n);

  p = malloc(n);

  if(!p)
    error("malloc: %s\n",strerror(errno));

  return p;
}

#if 0
static char *
xstrdup(const char *s)
{
  char *p;

  p = strdup(s);

  if(!p)
    error("strdup: %s\n",strerror(errno));

  return p;
}
#endif

static size_t
args_len(char **args)
{
  size_t n;

  assert(args);

  for( n = 0 ; *args ; ++n, ++args )
    ;

  return n;
}

static char **
args_merge(char *name,char **args1,char **args2)
{
  size_t i;
  char **args3;

  assert(name && args1 && args2);

  args3 = xmalloc((1 + args_len(args1) + args_len(args2) + 1) * sizeof(char *));

  i = 0;

  args3[i++] = name;

  while(*args1)
    args3[i++] = *args1++;

  while(*args2)
    args3[i++] = *args2++;

  args3[i] = 0;

  return args3;
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

    if(!strcmp(s,path))
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

static void
pacman_g2(char **args1)
{
  pid_t id;
  int status;

  id = fork();

  if(!id)
  {
    char *args2[] = { "--noconfirm", "--root", FW32_ROOT, "--config", FW32_CONFIG, 0 };

    execv("/usr/bin/pacman-g2",args_merge("/usr/bin/pacman-g2",args2,args1));

    _exit(EXIT_FAILURE);
  }
  else if(id == -1)
    error("fork: %s\n",strerror(errno));

  if(waitpid(id,&status,0) == -1)
    error("waitpid: %s\n",strerror(errno));

  if(!WIFEXITED(status) || WEXITSTATUS(status))
    error("pacman-g2 failed to complete its operation.\n");
}

static void
mount_all(void)
{
  const char **p;

  for( p = FW32_DIRS ; *p ; ++p )
    mount_directory(*p);
}

static void
umount_all(void)
{
  FILE *f;
  char line[LINE_MAX], *s, *e;

  f = fopen("/proc/mounts","rb");

  if(!f)
    error("Cannot open /proc/mounts for reading.\n");

  while(fgets(line,sizeof line,f))
  {
    s = strchr(line,' ');

    if(!s)
      continue;

    e = strchr(++s,' ');

    if(!e)
      continue;

    *e = 0;

    if(!strncmp(s,FW32_ROOT,strlen(FW32_ROOT)))
      umount_directory(s);
  }

  fclose(f);
}

static void
fw32_create(void)
{
  struct stat st;
  const char **p;
  char path[PATH_MAX];
  char *args[] =
  {
    "-Sy",
    "shadow",
    "coreutils",
    "findutils",
    "which",
    "wget",
    "file",
    "tar",
    "gzip",
    "bzip2",
    "util-linux",
    "procps",
    "kbd",
    "psmisc",
    "less",
    "pacman-g2",
    0
  };

  if(!stat(FW32_ROOT,&st))
    error("%s appears to already exist.\n",FW32_ROOT);

  p = FW32_DIRS;

  while(*p)
  {
    snprintf(path,sizeof path,"%s%s",FW32_ROOT,*p++);

    mkdir_parents(path);
  }

  mount_directory("/var/cache/pacman-g2/pkg");

  pacman_g2(args);

  umount_directory("/var/cache/pacman-g2/pkg");

  mount_all();
}

extern int
main(int argc,char **argv)
{
  char *cmd, **args;

  cmd = argv[0];

  args = argv + 1;

  if(!strcmp(cmd,"fw32-run"))
  {
    if(!getuid() || geteuid())
      error("This must be run as non-root, be SETUID, and owned by root.\n");
  }
  else if(getuid() || geteuid())
    error("This must be run as root.\n");

  if(personality(PER_LINUX32))
    error("Failed to enable 32 bit emulation.\n");

  if(!strcmp(cmd,"fw32-create"))
    fw32_create();

  return EXIT_SUCCESS;
}
