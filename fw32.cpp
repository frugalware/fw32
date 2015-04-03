// Copyright (C) 2011 James Buren
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

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

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

typedef struct
{
  const char *dir;
  bool ro;
} FW32_DIR;

static const char *FW32_ROOT = "/usr/lib/fw32";

static const char *FW32_CONFIG = "/etc/fw32/pacman-g2.conf";

static std::vector<FW32_DIR> FW32_DIRS_ALL {
  { "/proc",                false },
  { "/sys",                 false },
  { "/dev",                 false },
  { "/etc",                 false },
  { "/dev/pts",             false },
  { "/dev/shm",             false },
  { "/run",                 false },
  { "/usr/share/kde",        true },
  { "/usr/share/icons",      true },
  { "/usr/share/fonts",      true },
  { "/usr/share/themes",     true },
  { "/var/cache/pacman-g2", false },
  { "/var/run",             false },
  { "/var/fst",             false },
  { "/media",               false },
  { "/mnt",                 false },
  { "/home",                false },
  { "/var/tmp",             false },
  { "/tmp",                 false },
};

static std::vector<FW32_DIR> FW32_DIRS_BASE {
  { "/proc",                false },
  { "/sys",                 false },
  { "/dev",                 false },
  { "/var/fst",             false },
  { "/var/cache/pacman-g2", false },
  { "/var/tmp",             false },
  { "/tmp",                 false },
};

static std::vector<const char *> FW32_DEF_PKGS {
  "chroot-core",
  "devel-core",
  "procps",
  "kbd",
  "psmisc",
  "less",
  "git",
  "man",
  "openssh",
};

int nrSeparators(const char* s) {
    int nSep = 0;
    for(const char* c=s; *c; c++) {
        if(*c == '/') {
            nSep++;
        }
    }
    return nSep;
}

bool hasMoreSeparators(const char *s1, const char *s2) {
    return nrSeparators(s1) > nrSeparators(s2);
}

bool hasLessSeparators(const FW32_DIR& s1, const FW32_DIR& s2) {
    return nrSeparators(s1.dir) < nrSeparators(s2.dir);
}

static bool
is_cmd(const std::string& s,const std::string cmd)
{
  if(s == cmd)
    return true;

  std::string temp;
  temp = "/usr/sbin/" + cmd;

  if(s == temp)
    return true;

  temp = "/usr/bin/" + cmd;

  if(s == temp)
    return true;

  return false;
}

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
cp_file(const char *file)
{
  char file2[PATH_MAX];
  unsigned char buf[4096];
  FILE *in, *out;
  size_t n;

  assert(file);

  snprintf(file2,sizeof file2,"%s%s",FW32_ROOT,file);

  in = fopen(file,"rb");

  if(!in)
    error("Failed to open %s for reading.\n",file);

  out = fopen(file2,"wb");

  if(!out)
    error("Failed to open %s for writing.\n",file2);

  while(true)
  {
    n = fread(buf,sizeof *buf,sizeof buf,in);

    if(!n)
      break;

    fwrite(buf,sizeof *buf,n,out);
  }

  fclose(in);

  fclose(out);
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

    char *ptr = strstr(s,"\\040(deleted)");

    if(ptr)
      *ptr = 0;

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
run(const char *cmd,const char *dir,bool drop,std::vector <const char *> args1)
{
  char path[PATH_MAX];
  struct stat st;
  pid_t id;
  int status;

  snprintf(path,sizeof path,"%s%s",FW32_ROOT,dir);

  if(stat(path,&st))
    error("%s does not exist. Cannot execute command.\n",path);

  id = fork();

  if(!id)
  {
    args1.insert(args1.begin(),cmd);

    if(chroot(FW32_ROOT))
      error("Failed to enter chroot %s.",FW32_ROOT);

    if(chdir(dir))
      error("Failed to chdir to %s.\n",dir);

    if(drop)
      if(setuid(getuid()) || seteuid(getuid()) || setgid(getgid()) || setegid(getgid()))
        error("Failed to drop root privileges.\n");

    args1.push_back(NULL);
    execvp(cmd,(char * const*)args1.data());

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
mount_directory(const FW32_DIR& src)
{
  char dst[PATH_MAX];

  snprintf(dst,sizeof dst,"%s%s",FW32_ROOT,src.dir);

  if(ismounted(dst))
    return;

  mkdir_parents(dst);

  if(mount(src.dir,dst,"",MS_BIND,""))
    error("Failed to mount directory: %s: %s\n",dst,strerror(errno));

  if(src.ro)
    if(mount(src.dir,dst,"",MS_BIND | MS_RDONLY | MS_REMOUNT,""))
      error("Failed to mount directory: %s: %s\n",dst,strerror(errno));
}

static void
umount_directory(const char *path)
{

  if(umount2(path,UMOUNT_NOFOLLOW) && errno != EINVAL && errno != ENOENT)
    error("Failed to umount directory: %s: %s\n",path,strerror(errno));
}

static void
mount_all(void)
{
  std::sort(FW32_DIRS_ALL.begin(), FW32_DIRS_ALL.end(), hasLessSeparators);
  for(const auto& pdir:FW32_DIRS_ALL) {
    mount_directory(pdir);
  }
}

static void
umount_all(void)
{
  char line[LINE_MAX];
  char *s;
  FILE *in;

  in = fopen("/proc/mounts","rb");

  if(!in)
    error("Cannot open /proc/mounts for reading.\n");

  std::vector <const char *> umountDirs;

  while(fgets(line,sizeof line,in))
  {
    s = strtok(line, " ");
    s = strtok(NULL, " ");
    if(!strncmp(s,FW32_ROOT,strlen(FW32_ROOT)))
    {
      char *ptr = strstr(s,"\\040(deleted)");
      if(!ptr) {
        char * sCpy = new char[strlen(s)+1];
        strncpy(sCpy,s,strlen(s)+1);
        umountDirs.push_back(sCpy);
      }
    }
  }

  fclose(in);

  std::sort(umountDirs.begin(),umountDirs.end(),hasMoreSeparators);
  for(const auto& pDir:umountDirs)
  {
    umount_directory(pDir);
    delete(pDir);
  }
}

static void
mount_base(void)
{
  std::sort(FW32_DIRS_BASE.begin(), FW32_DIRS_BASE.end(), hasLessSeparators);
  for(const auto& pdir:FW32_DIRS_BASE) {
    mount_directory(pdir);
  }
}

static void
pacman_g2(std::vector<const char *> args1)
{
  pid_t id;
  int status;
  const FW32_DIR cache = { "/var/cache/pacman-g2", false };

  umount_all();

  mount_directory(cache);

  id = fork();

  if(!id)
  {
    args1.push_back("--noconfirm");
    args1.push_back("--root");
    args1.push_back(FW32_ROOT);
    args1.push_back("--config");
    args1.push_back(FW32_CONFIG);

    args1.insert(args1.begin(),"/usr/bin/pacman-g2");
    args1.push_back(NULL);
    execv("/usr/bin/pacman-g2",(char * const*)args1.data());

    _exit(EXIT_FAILURE);
  }
  else if(id == -1)
    error("fork: %s\n",strerror(errno));

  if(waitpid(id,&status,0) == -1)
    error("waitpid: %s\n",strerror(errno));

  if(!WIFEXITED(status) || WEXITSTATUS(status))
    error("pacman-g2 failed to complete its operation.\n");

  sleep(1);

  umount_all();

  mount_all();
}

static void
repoman(std::vector<const char *> args)
{
  assert(args);

  umount_all();

  cp_file("/etc/resolv.conf");

  cp_file("/etc/services");

  cp_file("/etc/localtime");

  mount_base();

  run("/usr/bin/repoman","/",false,args);

  sleep(1);

  umount_all();

  mount_all();
}

static int
nftw_cb( const char *path,const struct stat *st __attribute__ ((unused)),
		int type __attribute__ ((unused)),struct FTW *buf __attribute__ ((unused)) )
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
  char path[PATH_MAX];
  std::vector<const char *> args;
  args.push_back("-Sy");

  if(!stat(FW32_ROOT,&st))
    error("%s appears to already exist.\n",FW32_ROOT);

  for(const auto& pdir:FW32_DIRS_ALL) {
    snprintf(path,sizeof path,"%s%s",FW32_ROOT,pdir.dir);
    mkdir_parents(path);
  }

  args.insert(args.end(),FW32_DEF_PKGS.begin(),FW32_DEF_PKGS.end());
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
  char path[PATH_MAX];
  std::vector<const char *> args { "-Syf" };

  if(stat(FW32_ROOT,&st))
    error("%s does not exist.\n",FW32_ROOT);

  for(const auto& pdir:FW32_DIRS_ALL) {
    snprintf(path,sizeof path,"%s%s",FW32_ROOT,pdir.dir);
    mkdir_parents(path);
  }

  args.insert(args.end(),FW32_DEF_PKGS.begin(),FW32_DEF_PKGS.end());
  pacman_g2(args);
}

static void
fw32_upgrade(void)
{
  struct stat st;

  pacman_g2({"-Syuf"});

  if(!stat("/var/fst/current",&st) || !stat("/var/fst/stable",&st))
  {
      repoman({"update"});

      repoman({"upgrade"});
  }

  run("/usr/bin/fc-cache","/",false,   { "--force", "--system-only"});
}

static void
fw32_merge(std::vector<const char*> args1)
{
  repoman({"update"});

  args1.insert(args1.begin(),"merge");
  repoman(args1);
}

static void
fw32_run(int i,std::vector<const char*> args1)
{
  char cwd[PATH_MAX], path[PATH_MAX], *dir;
  struct passwd *pwd;
  struct stat st;

  mount_all();

  if(!getcwd(cwd,sizeof cwd))
    error("getcwd: %s\n",strerror(errno));

  pwd = getpwuid(getuid());

  if(!pwd)
    error("Failed to retrieve password entry.\n");

  snprintf(path,sizeof path,"%s%s",FW32_ROOT,cwd);

  dir = stat(path,&st) ? pwd->pw_dir : cwd;

  if(i < 1)
    run(pwd->pw_shell,dir,true,args1);
  else
  {
    const char * cmd = args1[0];
    args1.erase(args1.begin());
    run(cmd,dir,true,args1);
  }
}

static void
fw32_clean(void)
{
  pacman_g2({ "-Sc" });
}

static void
fw32_install(std::vector<const char *> args1)
{
  std::vector<const char *> args2 { "-Syf" };

  args2.insert(args2.begin(),args1.begin(),args1.end());
  pacman_g2(args2);
}

static void
fw32_install_package(std::vector<const char *> args1)
{
  std::vector<const char *> args2 { "-Uf" };

  args2.insert(args2.begin(),args1.begin(),args1.end());
  pacman_g2(args2);
}

static void
fw32_remove(std::vector<const char *> args1)
{
  std::vector<const char *> args2 { "-Rsc" };

  args2.insert(args2.begin(),args1.begin(),args1.end());
  pacman_g2(args2);
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
main(int argc,const char **argv)
{
  int i;
  const char *cmd;

  cmd = argv[0];

  std::vector<const char *> args;

  if (argc > 1) {
    args.assign(argv + 1, argv + argc);
  }

  i = argc - 1;

  if(is_cmd(cmd,"fw32-run"))
  {
    if(!getuid() || geteuid())
      error("This must be run as non-root, be SETUID, and owned by root.\n");
  }
  else if(getuid() || geteuid())
    error("This must be run as root.\n");

  if(personality(PER_LINUX32))
    error("Failed to enable 32 bit emulation.\n");

  if(is_cmd(cmd,"fw32-create"))
    fw32_create();
  else if(is_cmd(cmd,"fw32-update"))
    fw32_update();
  else if(is_cmd(cmd,"fw32-merge"))
    fw32_merge(args);
  else if(is_cmd(cmd,"fw32-delete"))
    fw32_delete();
  else if(is_cmd(cmd,"fw32-run"))
    fw32_run(i,args);
  else if(is_cmd(cmd,"fw32-upgrade"))
    fw32_upgrade();
  else if(is_cmd(cmd,"fw32-install"))
    fw32_install(args);
  else if(is_cmd(cmd,"fw32-install-package"))
    fw32_install_package(args);
  else if(is_cmd(cmd,"fw32-remove"))
    fw32_remove(args);
  else if(is_cmd(cmd,"fw32-clean"))
    fw32_clean();
  else if(is_cmd(cmd,"fw32-mount-all"))
    fw32_mount_all();
  else if(is_cmd(cmd,"fw32-umount-all"))
    fw32_umount_all();

  return EXIT_SUCCESS;
}
