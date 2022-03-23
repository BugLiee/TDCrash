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
#include "tac_jni.h"
#include "tac_test.h"
#include "tac_errno.h"
#include "tac_trace.h"
#include "tac_common.h"
#include "tac_crash.h"


#pragma clang diagnostic push
//忽略警告
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int tac_jni_init_status = 0;

static jint Java_cn_thinkingdata_android_crash_NativeHandler_nativeInit(JNIEnv *env,
                                                                 jclass thiz,
                                                                 jint api_level,
                                                                 jstring os_version, jstring abi_list,
                                                                 jstring manufacturer, jstring brand,
                                                                 jstring model, jstring build_fingerprint,
                                                                 jstring app_id, jstring app_version,
                                                                 jstring app_lib_dir, jstring log_dir,
                                                                 jboolean crash_enable, jboolean crash_rethrow,
                                                                 jboolean trace_enable, jboolean trace_rethrow)
{
    int              result_crash                                = TAC_ERRNO_JNI;
    int              result_trace                                = TAC_ERRNO_JNI;

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


    (void)thiz;

    //check has initialized
    if (tac_jni_init_status) return TAC_ERRNO_JNI;
    tac_jni_init_status = 1;
    // check params
    if (!env || !(*env) || (!crash_enable && !trace_enable)
    || api_level < 0 || !os_version || !abi_list || !manufacturer
    || !brand || !model || !build_fingerprint || !app_id
    || !app_version || !app_lib_dir || !log_dir)
    return TAC_ERRNO_INVAL;

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
    if (0 != tac_common_init((int)api_level,
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
        result_crash = TAC_ERRNO_JNI;

        //native crash 初始化
        result_crash = tac_crash_init(env, crash_rethrow ? 1 : 0);
    }

    if (trace_enable)
    {
        //ANR 初始化
        result_trace = tac_trace_init(env, trace_rethrow ? 1 : 0);
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

    return (0 == result_crash && 0 == result_trace) ? 0 : TAC_ERRNO_JNI;
}

static void Java_cn_thinkingdata_android_crash_NativeHandler_nativeNotifyJavaCrashed(JNIEnv *env, jclass thiz)
{
    (void)env;
    (void)thiz;

    tac_common_java_crashed = 1;
}

static void Java_cn_thinkingdata_android_crash_NativeHandler_testNativeCrash(JNIEnv *env, jclass thiz, jint run_in_new_thread)
{
    (void)env;
    (void)thiz;

    tac_test_crash(run_in_new_thread);
}

static JNINativeMethod tac_jni_methods[] = {
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
                "Z"
                "Z"
                ")"
                "I",
                (void *)Java_cn_thinkingdata_android_crash_NativeHandler_nativeInit
        },
        {
                "nativeNotifyJavaCrashed",
                "("
                ")"
                "V",
                (void *)Java_cn_thinkingdata_android_crash_NativeHandler_nativeNotifyJavaCrashed
        },
        {
                "testNativeCrash",
                "("
                "I"
                ")"
                "V",
                (void *)Java_cn_thinkingdata_android_crash_NativeHandler_testNativeCrash
        }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass  cls;

    (void)reserved;

    if(NULL == vm) return -1;

    //register JNI methods
    if(JNI_OK != (*vm)->GetEnv(vm, (void**)&env, TAC_JNI_VERSION)) return -1;
    if(NULL == env || NULL == *env) return -1;
    if(NULL == (cls = (*env)->FindClass(env, TAC_JNI_CLASS_NAME))) return -1;
    if((*env)->RegisterNatives(env, cls, tac_jni_methods, sizeof(tac_jni_methods) / sizeof(tac_jni_methods[0]))) return -1;

    tac_common_set_vm(vm, env, cls);

    return TAC_JNI_VERSION;
}

#pragma clang diagnostic pop
