/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

/**
 * callback.
 *
 * @author  bugliee
 * @version 1.0.0
 */
public interface ITACrashCallback {

    /**
     * onCrash.
     *
     * @param crashType crashType
     * @param logPath logPath
     * @param emergency emergency
     * */
    void onCrash(CrashType crashType, String logPath, String emergency);
}
