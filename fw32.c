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
#include <ftw.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pwd.h>

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

  assert(args1 && args2);

  i = 0;

  if(name)
  {
    args3 = xmalloc((1 + args_len(args1) + args_len(args2) + 1) * sizeof(char *));

    args3[i++] = name;
  }
  else
    args3 = xmalloc((args_len(args1) + args_len(args2) + 1) * sizeof(char *));

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
run(const char *cmd,const char *dir,bool drop,char **args1)
{
  char path[PATH_MAX];
  struct stat st;
  pid_t id;
  int status;

  assert(cmd && dir && args1);

  snprintf(path,sizeof path,"%s%s",FW32_ROOT,dir);

  if(stat(path,&st))
    error("%s does not exist. Cannot execute command.\n",path);

  id = fork();

  if(!id)
  {
    char *args2[] =
    {
      cmd,
      0
    };

    if(chroot(FW32_ROOT))
      error("Failed to enter chroot %s.",FW32_ROOT);

    if(chdir(dir))
      error("Failed to chdir to %s.\n",dir);

    if(drop)
      if(setuid(getuid()) || seteuid(getuid()))
        error("Failed to drop root privileges.\n");

    execvp(cmd,args_merge(0,args2,args1));

    _exit(errno);
  }
  else if(id == -1)
    error("fork: %s\n",strerror(errno));

  if(waitpid(id,&status,0) == -1)
    error("waitpid: %s\n",strerror(errno));

  if(!WIFEXITED(status) || (WEXITSTATUS(status) && WEXITSTATUS(status) != ENOENT))
    error("%s failed to complete its operation.\n",cmd);
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
mount_all(void)
{
  const char **p;

  p = FW32_DIRS;

  while(*p)
    mount_directory(*p++);
}

static void
umount_all(void)
{
  const char **p;
  char path[PATH_MAX];

  p = FW32_DIRS;

  while(*p)
  {
    snprintf(path,sizeof path,"%s%s",FW32_ROOT,*p++);

    umount_directory(path);
  }
}

static void
pacman_g2(char **args1)
{
  pid_t id;
  int status;

  assert(args1);

  umount_all();

  mount_directory("/var/cache/pacman-g2/pkg");

  id = fork();

  if(!id)
  {
    char *args2[] =
    {
      "--noconfirm",
      "--root",
      FW32_ROOT,
      "--config",
      FW32_CONFIG,
      0
    };

    execv("/usr/bin/pacman-g2",args_merge("/usr/bin/pacman-g2",args2,args1));

    _exit(EXIT_FAILURE);
  }
  else if(id == -1)
    error("fork: %s\n",strerror(errno));

  if(waitpid(id,&status,0) == -1)
    error("waitpid: %s\n",strerror(errno));

  if(!WIFEXITED(status) || WEXITSTATUS(status))
    error("pacman-g2 failed to complete its operation.\n");

  umount_directory("/var/cache/pacman-g2/pkg");

  mount_all();
}

static int
nftw_cb(const char *path,const struct stat *st,int type,struct FTW *buf)
{
  assert(path && st && buf);

  if(remove(path))
    return 1;

  return 0;
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

  pacman_g2(args);
}

static void
fw32_delete(void)
{
  int rv;

  umount_all();

  rv = nftw(FW32_ROOT,nftw_cb,16,FTW_DEPTH | FTW_PHYS);

  if(rv == -1)
    error("nftw: %s\n",strerror(errno));
  else if(rv == 1)
    error("Failed to remove a file while deleting.\n");
}

static void
fw32_update(void)
{
  struct stat st;
  const char **p;
  char path[PATH_MAX];

  if(stat(FW32_ROOT,&st))
    error("%s does not exist.\n",FW32_ROOT);

  p = FW32_DIRS;

  while(*p)
  {
    snprintf(path,sizeof path,"%s%s",FW32_ROOT,*p++);

    mkdir_parents(path);
  }
}

static void
fw32_upgrade(void)
{
  char *args1[] =
  {
    "-Syuf",
    0
  };
  char *args2[] =
  {
    "--force",
    "--system-only",
    0
  };

  pacman_g2(args1);

  run("/usr/bin/fc-cache","/",false,args2);
}

static void
fw32_run(int i,char **args1)
{
  char cwd[PATH_MAX];
  struct passwd *pwd;

  if(!getcwd(cwd,sizeof cwd))
    error("getcwd: %s\n",strerror(errno));

  pwd = getpwuid(getuid());

  if(!pwd)
    error("Failed to retrieve password entry.\n");

  if(i < 1)
    run(pwd->pw_shell,cwd,true,args1);
  else
    run(args1[0],cwd,true,args1+1);
}

static void
fw32_clean(void)
{
  char *args[] =
  {
    "-Sc",
    0
  };

  pacman_g2(args);
}

static void
fw32_install(char **args1)
{
  char *args2[] =
  {
    "-Syf",
    0
  };

  pacman_g2(args_merge(0,args2,args1));
}

static void
fw32_remove(char **args1)
{
  char *args2[] =
  {
    "-Rsc",
    0
  };

  pacman_g2(args_merge(0,args2,args1));
}

static void
fw32_mount_all(void)
{
  mount_all();
}

static void
fw32_umount_all(void)
{
  umount_all();
}

extern int
main(int argc,char **argv)
{
  char *cmd, **args;
  int i;

  cmd = argv[0];

  args = argv + 1;

  i = argc - 1;

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
  else if(!strcmp(cmd,"fw32-update"))
    fw32_update();
  else if(!strcmp(cmd,"fw32-delete"))
    fw32_delete();
  else if(!strcmp(cmd,"fw32-run"))
    fw32_run(i,args);
  else if(!strcmp(cmd,"fw32-upgrade"))
    fw32_upgrade();
  else if(!strcmp(cmd,"fw32-install"))
    fw32_install(args);
  else if(!strcmp(cmd,"fw32-remove"))
    fw32_remove(args);
  else if(!strcmp(cmd,"fw32-clean"))
    fw32_clean();
  else if(!strcmp(cmd,"fw32-mount-all"))
    fw32_mount_all();
  else if(!strcmp(cmd,"fw32-umount-all"))
    fw32_umount_all();

  return EXIT_SUCCESS;
}
