//
// Created by bugliee on 2022/1/11.
//

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <android/log.h>
#include "tac_test.h"
#include "tac_common.h"
#include "../dl/tac_dl.h"
#include "../common/tacc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#define TAC_TEST_LOG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "tacrash", fmt, ##__VA_ARGS__)
#define TAC_TEST_ABORT_MSG     "abort message for tacrash internal testing"

static void tac_test_set_abort_msg()
{
    tac_dl_t                           *libc          = NULL;
    tacc_util_libc_set_abort_message_t  set_abort_msg = NULL;

    if(tac_common_api_level >= 29) libc = tac_dl_create(TACC_UTIL_LIBC_APEX);
    if(NULL == libc && NULL == (libc = tac_dl_create(TACC_UTIL_LIBC))) goto end;
    if(NULL == (set_abort_msg = (tacc_util_libc_set_abort_message_t)tac_dl_sym(libc, TACC_UTIL_LIBC_SET_ABORT_MSG))) goto end;

    set_abort_msg(TAC_TEST_ABORT_MSG);

    end:
    if(NULL != libc) tac_dl_destroy(&libc);
}

int tac_test_call_4(int v)
{
    int *a = NULL;

    tac_test_set_abort_msg();

    *a = v; // crash!
    (*a)++;
    v = *a;

    return v;
}

int tac_test_call_3(int v)
{
    int r = tac_test_call_4(v + 1);
    return r;
}

int tac_test_call_2(int v)
{
    int r = tac_test_call_3(v + 1);
    return r;
}

void tac_test_call_1(void)
{
    int r = tac_test_call_2(1);
    r = 0;
}

static void *tac_test_new_thread(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "tac_test_cal");

    tac_test_call_1();

    return NULL;
}

static void *tac_test_keep_logging(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "tac_test_log");

    int i = 0;
    while(++i < 600)
    {
        TAC_TEST_LOG("crashed APP's thread is running ...... %d", i);
        usleep(1000 * 100);
    }

    return NULL;
}

void tac_test_crash(int run_in_new_thread)
{
    pthread_t tid;

    pthread_create(&tid, NULL, &tac_test_keep_logging, NULL);
    usleep(1000 * 10);

    if(run_in_new_thread)
        pthread_create(&tid, NULL, &tac_test_new_thread, NULL);
    else
        tac_test_call_1();
}

#pragma clang diagnostic pop



