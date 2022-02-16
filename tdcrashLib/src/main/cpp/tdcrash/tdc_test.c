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
#include "tdc_test.h"
#include "tdc_common.h"
#include "../dl/tdc_dl.h"
#include "../common/tdcc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#define TDC_TEST_LOG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "tdcrash", fmt, ##__VA_ARGS__)
#define TDC_TEST_ABORT_MSG     "abort message for tdcrash internal testing"

static void tdc_test_set_abort_msg()
{
    tdc_dl_t                           *libc          = NULL;
    tdcc_util_libc_set_abort_message_t  set_abort_msg = NULL;

    if(tdc_common_api_level >= 29) libc = tdc_dl_create(TDCC_UTIL_LIBC_APEX);
    if(NULL == libc && NULL == (libc = tdc_dl_create(TDCC_UTIL_LIBC))) goto end;
    if(NULL == (set_abort_msg = (tdcc_util_libc_set_abort_message_t)tdc_dl_sym(libc, TDCC_UTIL_LIBC_SET_ABORT_MSG))) goto end;

    set_abort_msg(TDC_TEST_ABORT_MSG);

    end:
    if(NULL != libc) tdc_dl_destroy(&libc);
}

int tdc_test_call_4(int v)
{
    int *a = NULL;

    tdc_test_set_abort_msg();

    *a = v; // crash!
    (*a)++;
    v = *a;

    return v;
}

int tdc_test_call_3(int v)
{
    int r = tdc_test_call_4(v + 1);
    return r;
}

int tdc_test_call_2(int v)
{
    int r = tdc_test_call_3(v + 1);
    return r;
}

void tdc_test_call_1(void)
{
    int r = tdc_test_call_2(1);
    r = 0;
}

static void *tdc_test_new_thread(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "tdc_test_cal");

    tdc_test_call_1();

    return NULL;
}

static void *tdc_test_keep_logging(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "tdc_test_log");

    int i = 0;
    while(++i < 600)
    {
        TDC_TEST_LOG("crashed APP's thread is running ...... %d", i);
        usleep(1000 * 100);
    }

    return NULL;
}

void tdc_test_crash(int run_in_new_thread)
{
    pthread_t tid;

    pthread_create(&tid, NULL, &tdc_test_keep_logging, NULL);
    usleep(1000 * 10);

    if(run_in_new_thread)
        pthread_create(&tid, NULL, &tdc_test_new_thread, NULL);
    else
        tdc_test_call_1();
}

#pragma clang diagnostic pop



