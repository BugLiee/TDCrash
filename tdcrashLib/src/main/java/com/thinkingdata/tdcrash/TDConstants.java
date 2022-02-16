/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

/**
 * Defined Constants.
 *
 * @author bugliee
 * @version 1.0.0
 */
public class TDConstants {

    public static final String TAG = "td_crash";

    //Initialization Status.
    public static final int OK = 0;
    public static final int CONTEXT_IS_NULL = -1;
    public static final int LOAD_LIBRARY_FAILED = -2;
    public static final int INIT_LIBRARY_FAILED = -3;

    //Prefix
    static final String logPrefix = "tombstone";

    //Suffix
    static final String javaLogSuffix = ".java.tdcrash";
    static final String nativeLogSuffix = ".native.tdcrash";
    static final String anrLogSuffix = ".anr.tdcrash";
    static final String traceLogSuffix = ".trace.tdcrash";

    public static final int javaLogCountMax = 20;
    public static final int nativeLogCountMax = 20;
    public static final int anrLogCountMax = 5;
    public static final int traceLogCountMax = 20;

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

    static final String memInfoFmt = "%21s %8s\n";
    static final String memInfoFmt2 = "%21s %8s %21s %8s\n";

    //Version
    static final String version = "1.0.0";
    static final String fullVersion = "TDCrash " + version;

}
