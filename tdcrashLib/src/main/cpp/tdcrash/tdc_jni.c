//
// Created by bugliee on 2022/1/11.
//

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <jni.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <android/log.h>
#include <jni.h>
#include "tdc_jni.h"
#include "tdc_test.h"
#include "tdc_errno.h"
#include "tdc_trace.h"
#include "tdc_common.h"
#include "tdc_crash.h"


#pragma clang diagnostic push
//忽略警告
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int tdc_jni_init_status = 0;

static jint Java_com_thinkingdata_tdcrash_NativeHandler_nativeInit(JNIEnv *env,
                                                                 jclass thiz,
                                                                 jint api_level,
                                                                 jstring os_version, jstring abi_list,
                                                                 jstring manufacturer, jstring brand,
                                                                 jstring model, jstring build_fingerprint,
                                                                 jstring app_id, jstring app_version,
                                                                 jstring app_lib_dir, jstring log_dir,
                                                                 jboolean crash_enable, jboolean crash_rethrow,
                                                                 jint crash_logcat_system_lines,
                                                                 jint crash_logcat_events_lines,
                                                                 jint crash_logcat_main_lines,
                                                                 jboolean crash_dump_elf_hash,
                                                                 jboolean crash_dump_map,
                                                                 jboolean crash_dump_fds,
                                                                 jboolean crash_dump_network_info,
                                                                 jboolean crash_dump_all_threads,
                                                                 jint crash_dump_all_threads_count_max,
                                                                 jobjectArray crash_dump_all_threads_white_list,
                                                                 jboolean trace_enable, jboolean trace_rethrow,
                                                                 jint trace_logcat_system_lines,
                                                                 jint trace_logcat_events_lines,
                                                                 jint trace_logcat_main_lines,
                                                                 jboolean trace_dump_fds,
                                                                 jboolean trace_dump_network_info)
{
    int              result_crash                                = TDC_ERRNO_JNI;
    int              result_trace                                = TDC_ERRNO_JNI;

    //基本参数
    const char      *current_os_version                               = NULL;
    const char      *current_abi_list                                 = NULL;
    const char      *current_manufacturer                             = NULL;
    const char      *current_brand                                    = NULL;
    const char      *current_model                                    = NULL;
    const char      *current_build_fingerprint                        = NULL;
    const char      *current_app_id                                   = NULL;
    const char      *current_app_version                              = NULL;
    const char      *current_app_lib_dir                              = NULL;
    const char      *current_log_dir                                  = NULL;

    const char      **current_crash_dump_all_threads_whitelist        = NULL;
    size_t          current_crash_dump_all_threads_whitelist_len      =0;
    // size_t 内部存储了一个整数用来记录大小
    size_t          len, i;
    jstring         tmp_str;
    const char      *tmp_c_str;

    (void)thiz;

    //check has initialized
    if (tdc_jni_init_status) return TDC_ERRNO_JNI;
    tdc_jni_init_status = 1;
    // check params
    if (!env || !(*env) || (!crash_enable && !trace_enable)
    || api_level < 0 || !os_version || !abi_list || !manufacturer
    || !brand || !model || !build_fingerprint || !app_id
    || !app_version || !app_lib_dir || !log_dir
    || crash_logcat_system_lines < 0 || crash_logcat_events_lines < 0 || crash_logcat_main_lines < 0
    || crash_dump_all_threads_count_max < 0 || trace_logcat_system_lines < 0 || trace_logcat_events_lines < 0
    || trace_logcat_main_lines< 0)
    return TDC_ERRNO_INVAL;

    //尝试转换字符串，转换失败则通知jvm回首变量释放内存
    if(NULL == (current_os_version        = (*env)->GetStringUTFChars(env, os_version,        0))) goto clean;
    if(NULL == (current_abi_list          = (*env)->GetStringUTFChars(env, abi_list,          0))) goto clean;
    if(NULL == (current_manufacturer      = (*env)->GetStringUTFChars(env, manufacturer,      0))) goto clean;
    if(NULL == (current_brand             = (*env)->GetStringUTFChars(env, brand,             0))) goto clean;
    if(NULL == (current_model             = (*env)->GetStringUTFChars(env, model,             0))) goto clean;
    if(NULL == (current_build_fingerprint = (*env)->GetStringUTFChars(env, build_fingerprint, 0))) goto clean;
    if(NULL == (current_app_id            = (*env)->GetStringUTFChars(env, app_id,            0))) goto clean;
    if(NULL == (current_app_version       = (*env)->GetStringUTFChars(env, app_version,       0))) goto clean;
    if(NULL == (current_app_lib_dir       = (*env)->GetStringUTFChars(env, app_lib_dir,       0))) goto clean;
    if(NULL == (current_log_dir           = (*env)->GetStringUTFChars(env, log_dir,           0))) goto clean;

    //公共属性初始化
    if (0 != tdc_common_init((int)api_level,
                                 current_os_version,
                                 current_abi_list,
                                 current_manufacturer,
                                 current_brand,
                                 current_model,
                                 current_build_fingerprint,
                                 current_app_id,
                                 current_app_version,
                                 current_app_lib_dir,
                                 current_log_dir)) goto clean;
    result_crash = 0;
    result_trace = 0;

    if (crash_enable)
    {
        result_crash = TDC_ERRNO_JNI;

        if (crash_dump_all_threads_white_list)
        {
            len = (size_t)(*env)->GetArrayLength(env, crash_dump_all_threads_white_list);
            if(len > 0)
            {
                if(NULL != (current_crash_dump_all_threads_whitelist = calloc(len, sizeof(char *))))
                {
                    current_crash_dump_all_threads_whitelist_len = len;
                    for(i = 0; i < len; i++)
                    {   //遍历白名单赋值到数组
                        tmp_str = (jstring)((*env)->GetObjectArrayElement(env, crash_dump_all_threads_white_list, (jsize)i));
                        current_crash_dump_all_threads_whitelist[i] = (tmp_str ? (*env)->GetStringUTFChars(env, tmp_str, 0) : NULL);
                    }
                }
            }
        }

        //native crash 初始化
        result_crash = tdc_crash_init(env,
                                crash_rethrow ? 1 : 0,
                                (unsigned int)crash_logcat_system_lines,
                                (unsigned int)crash_logcat_events_lines,
                                (unsigned int)crash_logcat_main_lines,
                                crash_dump_elf_hash ? 1 : 0,
                                crash_dump_map ? 1 : 0,
                                crash_dump_fds ? 1 : 0,
                                crash_dump_network_info ? 1 : 0,
                                crash_dump_all_threads ? 1 : 0,
                                (unsigned int)crash_dump_all_threads_count_max,
                                current_crash_dump_all_threads_whitelist,
                                current_crash_dump_all_threads_whitelist_len);
    }

    if (trace_enable)
    {
        //ANR 初始化
        result_trace = tdc_trace_init(env,
                                          trace_rethrow ? 1 : 0,
                                          (unsigned int)trace_logcat_system_lines,
                                          (unsigned int)trace_logcat_events_lines,
                                          (unsigned int)trace_logcat_main_lines,
                                          trace_dump_fds ? 1 : 0,
                                          trace_dump_network_info ? 1 : 0);
    }



    clean:
    if(os_version        && current_os_version)        (*env)->ReleaseStringUTFChars(env, os_version,        current_os_version);
    if(abi_list          && current_abi_list)          (*env)->ReleaseStringUTFChars(env, abi_list,          current_abi_list);
    if(manufacturer      && current_manufacturer)      (*env)->ReleaseStringUTFChars(env, manufacturer,      current_manufacturer);
    if(brand             && current_brand)             (*env)->ReleaseStringUTFChars(env, brand,             current_brand);
    if(model             && current_model)             (*env)->ReleaseStringUTFChars(env, model,             current_model);
    if(build_fingerprint && current_build_fingerprint) (*env)->ReleaseStringUTFChars(env, build_fingerprint, current_build_fingerprint);
    if(app_id            && current_app_id)            (*env)->ReleaseStringUTFChars(env, app_id,            current_app_id);
    if(app_version       && current_app_version)       (*env)->ReleaseStringUTFChars(env, app_version,       current_app_version);
    if(app_lib_dir       && current_app_lib_dir)       (*env)->ReleaseStringUTFChars(env, app_lib_dir,       current_app_lib_dir);
    if(log_dir           && current_log_dir)           (*env)->ReleaseStringUTFChars(env, log_dir,           current_log_dir);

    if(crash_dump_all_threads_white_list && NULL != current_crash_dump_all_threads_whitelist)
    {
        for(i = 0; i < current_crash_dump_all_threads_whitelist_len; i++)
        {
            tmp_str = (jstring)((*env)->GetObjectArrayElement(env, crash_dump_all_threads_white_list, (jsize)i));
            tmp_c_str = current_crash_dump_all_threads_whitelist[i];
            if(tmp_str && NULL != tmp_c_str) (*env)->ReleaseStringUTFChars(env, tmp_str, tmp_c_str);
        }
        free(current_crash_dump_all_threads_whitelist);
    }

    return (0 == result_crash && 0 == result_trace) ? 0 : TDC_ERRNO_JNI;
}

static void Java_com_thinkingdata_tdcrash_NativeHandler_nativeNotifyJavaCrashed(JNIEnv *env, jclass thiz)
{
    (void)env;
    (void)thiz;

    tdc_common_java_crashed = 1;
}

static void Java_com_thinkingdata_tdcrash_NativeHandler_testNativeCrash(JNIEnv *env, jclass thiz, jint run_in_new_thread)
{
    (void)env;
    (void)thiz;

    tdc_test_crash(run_in_new_thread);
}

static JNINativeMethod tdc_jni_methods[] = {
        {
                "nativeInit",
                "("
                "I"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Ljava/lang/String;"
                "Z"
                "Z"
                "I"
                "I"
                "I"
                "Z"
                "Z"
                "Z"
                "Z"
                "Z"
                "I"
                "[Ljava/lang/String;"
                "Z"
                "Z"
                "I"
                "I"
                "I"
                "Z"
                "Z"
                ")"
                "I",
                (void *)Java_com_thinkingdata_tdcrash_NativeHandler_nativeInit
        },
        {
                "nativeNotifyJavaCrashed",
                "("
                ")"
                "V",
                (void *)Java_com_thinkingdata_tdcrash_NativeHandler_nativeNotifyJavaCrashed
        },
        {
                "testNativeCrash",
                "("
                "I"
                ")"
                "V",
                (void *)Java_com_thinkingdata_tdcrash_NativeHandler_testNativeCrash
        }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass  cls;

    (void)reserved;

    if(NULL == vm) return -1;

    //register JNI methods
    if(JNI_OK != (*vm)->GetEnv(vm, (void**)&env, TDC_JNI_VERSION)) return -1;
    if(NULL == env || NULL == *env) return -1;
    if(NULL == (cls = (*env)->FindClass(env, TDC_JNI_CLASS_NAME))) return -1;
    if((*env)->RegisterNatives(env, cls, tdc_jni_methods, sizeof(tdc_jni_methods) / sizeof(tdc_jni_methods[0]))) return -1;

    tdc_common_set_vm(vm, env, cls);

    return TDC_JNI_VERSION;
}

#pragma clang diagnostic pop
