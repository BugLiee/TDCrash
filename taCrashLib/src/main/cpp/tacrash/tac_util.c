//
// Created by bugliee on 2022/1/11.
//

#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include "tac_errno.h"
#include "tacc_fmt.h"
#include "../common/tacc_util.h"
#include "tac_util.h"

char *tac_util_strdupcat(const char *s1, const char *s2)
{
    size_t s1_len, s2_len;
    char *s;

    if(NULL == s1 || NULL == s2) return NULL;

    s1_len = strlen(s1);
    s2_len = strlen(s2);

    if(NULL == (s = malloc(s1_len + s2_len + 1))) return NULL;
    memcpy(s, s1, s1_len);
    memcpy(s + s1_len, s2, s2_len + 1);

    return s;
}

//检查目录存在与否，无则创建
int tac_util_mkdirs(const char *dir)
{
    size_t  len;
    char    buf[PATH_MAX];
    char   *p;

    //check or try create dir directly
    errno = 0;
    if(0 == mkdir(dir, S_IRWXU) || EEXIST == errno) return 0;

    //trycreate dir recursively...

    len = strlen(dir);
    if(0 == len) return TAC_ERRNO_INVAL;
    if('/' != dir[0]) return TAC_ERRNO_INVAL;

    memcpy(buf, dir, len + 1);
    if(buf[len - 1] == '/') buf[len - 1] = '\0';

    for(p = buf + 1; *p; p++)
    {
        if(*p == '/')
        {
            *p = '\0';
            errno = 0;
            if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
            *p = '/';
        }
    }
    errno = 0;
    if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
    return 0;
}

//获取系统版本信息
void tac_util_get_kernel_version(char *buf, size_t len)
{
    struct utsname uts;

    if(0 != uname(&uts))
    {
        strncpy(buf, "unknown", len);
        buf[len - 1] = '\0';
        return;
    }

    snprintf(buf, len, "%s version %s %s (%s)", uts.sysname, uts.release, uts.version, uts.machine);
}



