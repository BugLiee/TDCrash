/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import java.io.File;

/**
 * TDCrash.
 *
 * @author  bugliee
 * @version 1.0.0
 * */
public class TDCrash {

    private static final String TAG = TDConstants.TAG + ".TDCrash";

    /**
     * Common parameters for configuring TDCrash.
     *
     * @author  bugliee
     * @version 1.0.0
     * */
    public static class TDCrashConfig {

        public String logDir;

        public String appVersion = "unknown";

        public String nativeLibDir;
        
        public int pid;
        
        public String processName;

        public String appId;

        TDCrashConfig(Context context) {
            this.logDir = context.getCacheDir().getAbsolutePath() + File.separator + "tdcrash";
            try {
                this.appVersion = context.getPackageManager().getPackageInfo(
                        context.getPackageName(), PackageManager.GET_CONFIGURATIONS).versionName;
            } catch (PackageManager.NameNotFoundException e) {
                e.printStackTrace();
            }
            this.nativeLibDir = context.getApplicationInfo().nativeLibraryDir;
            this.pid = Process.myPid();
            this.processName = Utils.getProcessName(context, pid);
            this.appId = context.getPackageName();
        }


    }

    public TDCrashConfig config;

    private static volatile TDCrash instance;

    /**
     * Get instance.
     *
     * @return TDCrash
     * */
    public static TDCrash getInstance() {
        if (instance == null) {
            synchronized (TDCrash.class) {
                if (instance == null) {
                    instance = new TDCrash();
                }
            }
        }
        return instance;
    }


    /**
     * Initialize basic parameters.
     *
     * @param context Context
     * @param listener CheckListener
     * */
    public void init(Context context, CheckListener listener) {
        TDCrashLogger.error("td_crash", "context : " + context + "path "+ context.getCacheDir());
        if (context == null) {
            TDCrashLogger.error(TAG, "context is null !" + TDConstants.CONTEXT_IS_NULL);
            return;
        }
        config = new TDCrashConfig(context);
        checkListener = listener;
        checkFile();
    }

    /**
     * Check Listener.
     * */
    public interface CheckListener {
        /**
         * heck whether there is a crash log file locally.
         *
         * @param needReportFile crash log
         * @return save to database
         */
        boolean fileCallback(File needReportFile);
    }

    private CheckListener checkListener;

    private void checkFile() {
        File logF = new File(config.logDir);
        if (logF.exists()) {
            File[] logs = logF.listFiles();
            if (checkListener != null && logs != null) {
                for (File f : logs) {
                    if (checkListener.fileCallback(f)) {
                        if (f.delete()) {
                            TDCrashLogger.debug(TAG,
                                    "delete success : " + f.getAbsolutePath());
                        } else {
                            TDCrashLogger.debug(TAG,
                                    "delete failed : " + f.getAbsolutePath());
                        }
                    }
                }
            }
        }
    }

    //Initialization status
    public boolean javaInitialized = false;

    public boolean nativeInitialized = false;

    public boolean anrInitialized = false;

    /**
     * Initialize JavaCrashHandler.
     *
     * */
    public void initJavaCrashHandler(boolean rethrow) {
        if (!javaInitialized) {
            JavaCrashHandler.getInstance().initialize(
                    config.pid,
                    config.processName,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    rethrow,
                    30,
                    30,
                    30,
                    true,
                    true,
                    true,
                    10,
                    new String[]{},
                    (crashType, logPath, emergency) -> {
                        TDCrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (checkListener != null) {
                            checkListener.fileCallback(new File(logPath));
                        }
                    });
            javaInitialized = true;
        }
    }

    /**
     * Initialize NativeCrashHandler.
     */
    public void initNativeCrashHandler(boolean crashEnable, boolean crashRethrow, boolean anrEnable, boolean anrRethrow) {

        if (!nativeInitialized) {
            nativeInitialized = NativeHandler.getInstance().initialize(
                    null,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    crashEnable,
                    crashRethrow,
                    30,
                    30,
                    30,
                    true,
                    true,
                    true,
                    true,
                    true,
                    10,
                    new String[]{},
                    (crashType, logPath, emergency) -> {
                        TDCrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (checkListener != null) {
                            checkListener.fileCallback(new File(logPath));
                        }
                    },
                    anrEnable,
                    anrRethrow,
                    true,
                    30,
                    30,
                    30,
                    true,
                    true,
                    (crashType, logPath, emergency) -> {
                        TDCrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (checkListener != null) {
                            checkListener.fileCallback(new File(logPath));
                        }
                    }

            ) == TDConstants.OK;
        }

    }

    /**
     * Initialize ANRHandler.
     *
     * */
    public void initANRHandler() {
        if (!anrInitialized) {
            AnrHandler.getInstance().initialize(
                    config.pid,
                    config.processName,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    true,
                    30,
                    30,
                    30,
                    true,
                    true,
                    (crashType, logPath, emergency) -> {
                        TDCrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (checkListener != null) {
                            checkListener.fileCallback(new File(logPath));
                        }
                    });
            anrInitialized = true;
        }

    }
}
