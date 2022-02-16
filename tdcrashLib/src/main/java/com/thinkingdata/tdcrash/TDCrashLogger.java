/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

import android.util.Log;

/**
 * Log.
 *
 * @author bugliee
 * @version 1.0.0
 */
public class TDCrashLogger {
    static boolean isLog = false;

    public static void setLog(boolean log) {
        isLog = log;
    }

    /**
     * level v.
     *
     * @param tag TAG
     * @param msg MSG
     */
    public static void verbose(String tag, String msg) {
        if (isLog) {
            Log.v(tag, msg);
        }
    }

    /**
     * level v.
     *
     * @param tag TAG
     * @param msg MSG
     * @param tr  Throwable
     */
    public static void verbose(String tag, String msg, Throwable tr) {
        if (isLog) {
            Log.v(tag, msg, tr);
        }
    }

    /**
     * level i.
     *
     * @param tag TAG
     * @param msg MSG
     */
    public static void info(String tag, String msg) {
        if (isLog) {
            Log.i(tag, msg);
        }
    }

    /**
     * level i.
     *
     * @param tag TAG
     * @param msg MSG
     * @param tr  Throwable
     */
    public static void info(String tag, String msg, Throwable tr) {
        if (isLog) {
            Log.i(tag, msg, tr);
        }
    }

    /**
     * level d.
     *
     * @param tag TAG
     * @param msg MSG
     */
    public static void debug(String tag, String msg) {
        if (isLog) {
            Log.d(tag, msg);
        }
    }

    /**
     * level d.
     *
     * @param tag TAG
     * @param msg MSG
     * @param tr  Throwable
     */
    public static void debug(String tag, String msg, Throwable tr) {
        if (isLog) {
            Log.d(tag, msg, tr);
        }
    }

    /**
     * level e.
     *
     * @param tag TAG
     * @param msg MSG
     */
    public static void error(String tag, String msg) {
        if (isLog) {
            Log.e(tag, msg);
        }
    }

    /**
     * level e.
     *
     * @param tag TAG
     * @param msg MSG
     * @param tr  Throwable
     */
    public static void error(String tag, String msg, Throwable tr) {
        if (isLog) {
            Log.e(tag, msg, tr);
        }
    }

    /**
     * level w.
     *
     * @param tag TAG
     * @param msg MSG
     */
    public static void warn(String tag, String msg) {
        if (isLog) {
            Log.w(tag, msg);
        }
    }

    /**
     * level w.
     *
     * @param tag TAG
     * @param msg MSG
     * @param tr  Throwable
     */
    public static void warn(String tag, String msg, Throwable tr) {
        if (isLog) {
            Log.w(tag, msg, tr);
        }
    }
}
