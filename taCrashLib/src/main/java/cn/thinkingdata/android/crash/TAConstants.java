/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

/**
 * Defined Constants.
 *
 * @author bugliee
 * @version 1.0.0
 */
public class TAConstants {

    public static final String TAG = "td_crash";

    //Initialization Status.
    public static final int OK = 0;
    public static final int CONTEXT_IS_NULL = -1;
    public static final int LOAD_LIBRARY_FAILED = -2;
    public static final int INIT_LIBRARY_FAILED = -3;

    //Prefix
    static final String logPrefix = "tombstone";

    //Suffix
    static final String javaLogSuffix = ".java.tacrash";
    static final String nativeLogSuffix = ".native.tacrash";
    static final String anrLogSuffix = ".anr.tacrash";
    static final String traceLogSuffix = ".trace.tacrash";

    //Separator
    static final String sepHead
            = "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***";
    static final String sepOtherThreads
            = "--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---";
    static final String sepOtherThreadsEnding
            = "+++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++";

    //Time Formatter
    static final String timeFormatterStr = "yyyy-MM-dd'T'HH:mm:ss.SSSZ";

    //Crash Type
    static final String javaCrashType = "java";
    static final String nativeCrashType = "native";
    static final String anrCrashType = "anr";


    //Version
    static final String version = "1.0.0";
    static final String fullVersion = "TACrash " + version;

}
