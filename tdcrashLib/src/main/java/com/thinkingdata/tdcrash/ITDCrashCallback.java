/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

/**
 * callback.
 *
 * @author  bugliee
 * @version 1.0.0
 */
public interface ITDCrashCallback {

    /**
     * onCrash.
     *
     * @param crashType crashType
     * @param logPath logPath
     * @param emergency emergency
     * */
    void onCrash(CrashType crashType, String logPath, String emergency);
}
