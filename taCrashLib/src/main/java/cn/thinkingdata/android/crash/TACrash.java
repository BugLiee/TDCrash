/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import java.io.File;

/**
 * TACrash.
 *
 * @author  bugliee
 * @version 1.0.0
 * */
public class TACrash {

    private static final String TAG = TAConstants.TAG + ".TACrash";

    /**
     * Common parameters for configuring TACrash.
     *
     * @author  bugliee
     * @version 1.0.0
     * */
    public static class TACrashConfig {

        public String logDir;

        public String appVersion = "unknown";

        public String nativeLibDir;
        
        public int pid;
        
        public String processName;

        public String appId;

        public Context context;

        TACrashConfig(Context context) {
            this.context = context;
            this.logDir = context.getCacheDir().getAbsolutePath() + File.separator + "tacrash";
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

    public TACrashConfig config;

    private static volatile TACrash instance;

    /**
     * Get instance.
     *
     * @return TACrash
     * */
    public static TACrash getInstance() {
        if (instance == null) {
            synchronized (TACrash.class) {
                if (instance == null) {
                    instance = new TACrash();
                }
            }
        }
        return instance;
    }


    /**
     * Initialize basic parameters.
     *
     * @param context Context
     * */
    public TACrash init(Context context) {
        //TACrashLogger.error("td_crash", "context : " + context + "path " + context.getCacheDir());
        config = new TACrashConfig(context);
        return this;
    }


    private CrashLogListener logListener;

    //Initialization status
    public boolean javaInitialized = false;

    public boolean nativeInitialized = false;

    public boolean anrInitialized = false;

    /**
     * Initialize JavaCrashHandler.
     *
     * */
    public TACrash initJavaCrashHandler(boolean rethrow) {
        if (!javaInitialized) {
            JavaCrashHandler.getInstance().initialize(
                    config.pid,
                    config.processName,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    rethrow,

                    (crashType, logPath, emergency) -> {
                        TACrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (logListener != null) {
                            logListener.onFile(new File(logPath));
                        }
                    });
            javaInitialized = true;
        }
        return this;
    }

    /**
     * Initialize NativeCrashHandler.
     */
    public TACrash initNativeCrashHandler(boolean crashEnable, boolean crashRethrow,
                                          boolean anrEnable, boolean anrRethrow) {

        if (!nativeInitialized) {
            nativeInitialized = NativeHandler.getInstance().initialize(
                    null,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    crashEnable,
                    crashRethrow,
                    (crashType, logPath, emergency) -> {
                        TACrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (logListener != null) {
                            logListener.onFile(new File(logPath));
                        }
                    },
                    anrEnable,
                    anrRethrow,
                    true,
                    (crashType, logPath, emergency) -> {
                        TACrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (logListener != null) {
                            logListener.onFile(new File(logPath));
                        }
                    }

            ) == TAConstants.OK;
        }
        return this;
    }

    /**
     * Initialize ANRHandler.
     *
     * */
    public TACrash initANRHandler() {
        if (!anrInitialized) {
            AnrHandler.getInstance().initialize(
                    config.pid,
                    config.processName,
                    config.appId,
                    config.appVersion,
                    config.logDir,
                    true,
                    (crashType, logPath, emergency) -> {
                        TACrashLogger.debug(TAG, "crashType : " + crashType.name()
                                + " logPath : " + logPath + " emergency : " + emergency);
                        if (logListener != null) {
                            logListener.onFile(new File(logPath));
                        }
                    });
            anrInitialized = true;
        }
        return this;
    }

    public TACrash enableLog() {
        TACrashLogger.setLog(true);
        return this;
    }

    public TACrash initCrashLogListener(CrashLogListener listener) {
        this.logListener = listener;
        return this;
    }
}
