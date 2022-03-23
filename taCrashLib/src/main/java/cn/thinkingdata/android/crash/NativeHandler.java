/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

import android.os.Build;
import android.text.TextUtils;

import java.io.File;
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Map;

/**
 * Used to catch native exceptions.
 * contains anr & nativeCrash.
 *
 * @author  bugliee
 * @version 1.0.0
 */
public class NativeHandler {
    private static final String TAG = TAConstants.TAG + ".NativeHandler";

    private static final NativeHandler instance = new NativeHandler();
    private long anrTimeoutMs = 15 * 1000;

    private boolean crashRethrow;
    ITACrashCallback crashCallback;
    private boolean anrCheckProcessState;
    private ITACrashCallback anrCallback;

    private boolean anrEnable;
    private boolean initNativeSuccess = false;

    private NativeHandler() {
    }

    public static NativeHandler getInstance() {
        return instance;
    }

    /**
     * Initialize method.
     * */
    public int initialize(ITALibLoader libLoader,
                          String appId,
                          String appVersion,
                          String logDir,
                          boolean crashEnable,
                          boolean crashRethrow,
                          ITACrashCallback crashCallback,
                          boolean anrEnable,
                          boolean anrRethrow,
                          boolean anrCheckProcessState,
                          ITACrashCallback anrCallback) {
        //load so
        if (libLoader == null) {
            try {
                System.loadLibrary("tacrash");
            } catch (Throwable e) {
                TACrashLogger.error(TAG, "NativeHandler System.loadLibrary failed", e);
                return TAConstants.LOAD_LIBRARY_FAILED;
            }
        } else {
            try {
                libLoader.loadLibrary("tacrash");
            } catch (Throwable e) {
                TACrashLogger.error(TAG, "NativeHandler ITDLibLoader.loadLibrary failed", e);
                return TAConstants.LOAD_LIBRARY_FAILED;
            }
        }

        this.crashRethrow = crashRethrow;
        this.crashCallback = crashCallback;
        this.anrCheckProcessState = anrCheckProcessState;
        this.anrCallback = anrCallback;
        this.anrTimeoutMs = anrRethrow ? 15 * 1000 : 30 * 1000;

        this.anrEnable = anrEnable;
        try {
            int r = nativeInit(
                    Build.VERSION.SDK_INT,
                    Build.VERSION.RELEASE,
                    Utils.getAbiList(),
                    Build.MANUFACTURER,
                    Build.BRAND,
                    Utils.getMobileModel(),
                    Build.FINGERPRINT,
                    appId,
                    appVersion,
                    TACrash.getInstance().config.context.getApplicationInfo()
                            .nativeLibraryDir,
                    logDir,
                    crashEnable,
                    crashRethrow,
                    anrEnable,
                    anrRethrow);
            if (r != 0) {
                TACrashLogger.error(TAG, "NativeHandler init failed");
                return TAConstants.INIT_LIBRARY_FAILED;
            }
            initNativeSuccess = true;
            return TAConstants.OK;
        } catch (Throwable e) {
            TACrashLogger.error(TAG, "NativeHandler init failed", e);
            return TAConstants.INIT_LIBRARY_FAILED;
        }
    }

    private static String getStacktraceByThreadName(boolean isMainThread, String threadName) {
        try {
            for (Map.Entry<Thread, StackTraceElement[]> entry : Thread.getAllStackTraces()
                    .entrySet()) {
                Thread thd = entry.getKey();
                if ((isMainThread && thd.getName().equals("main"))
                        || (!isMainThread && thd.getName().contains(threadName))) {
                    StringBuilder sb = new StringBuilder();
                    for (StackTraceElement element : entry.getValue()) {
                        sb.append("    at ").append(element.toString()).append("\n");
                    }
                    return sb.toString();
                }
            }
        } catch (Exception e) {
            TACrashLogger.error(TAG, "NativeHandler getStacktraceByThreadName failed", e);
        }
        return null;
    }

    static boolean appendText(String logPath, String text) {
        RandomAccessFile raf = null;

        try {
            raf = new RandomAccessFile(logPath, "rws");

            long pos = 0;
            if (raf.length() > 0) {
                FileChannel fc = raf.getChannel();
                MappedByteBuffer mbb = fc.map(FileChannel.MapMode.READ_ONLY, 0, raf.length());
                for (pos = raf.length(); pos > 0; pos--) {
                    if (mbb.get((int) pos - 1) != (byte) 0) {
                        break;
                    }
                }
            }

            raf.seek(pos);
            raf.write(text.getBytes("UTF-8"));

            return true;
        } catch (Exception e) {
            TACrashLogger.error(TAG,  "FileManager appendText failed", e);
            return false;
        } finally {
            if (raf != null) {
                try {
                    raf.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * appendText.
     * */
    public static boolean appendText(String logPath, String key, String content) {
        if (TextUtils.isEmpty(logPath) || TextUtils.isEmpty(key) || content == null) {
            return false;
        }

        return appendText(logPath, "\n\n" + key + ":\n" + content + "\n\n");
    }

    private static void crashCallback(String logPath, String emergency, boolean dumpJavaStacktrace,
                                      boolean isMainThread, String threadName) {
        if (!TextUtils.isEmpty(logPath)) {

            if (dumpJavaStacktrace) {
                String stacktrace = getStacktraceByThreadName(isMainThread, threadName);
                if (!TextUtils.isEmpty(stacktrace)) {
                    appendText(logPath, "java stacktrace", stacktrace);
                }
            }

            appendText(logPath, "foreground",
                    Utils.isForeground(TACrash.getInstance().config.context) ? "yes" : "no");
        }

        ITACrashCallback callback = NativeHandler.getInstance().crashCallback;
        if (callback != null) {
            try {
                callback.onCrash(CrashType.NATIVE_CRASH, logPath, emergency);
            } catch (Exception e) {
                TACrashLogger.error(TAG,  "NativeHandler native crash callback.onCrash failed", e);
            }
        }
    }

    private static void traceCallback(String logPath, String emergency) {
        if (TextUtils.isEmpty(logPath)) {
            return;
        }

        appendText(logPath, "foreground",
                Utils.isForeground(TACrash.getInstance().config.context) ? "yes" : "no");

        if (NativeHandler.getInstance().anrCheckProcessState) {
            if (!Utils.checkProcessAnrState(TACrash.getInstance().config.context,
                    NativeHandler.getInstance().anrTimeoutMs)) {
                return;
            }
        }
        String anrLogPath = logPath.substring(0, logPath.length()
                - TAConstants.traceLogSuffix.length()) + TAConstants.anrLogSuffix;
        File traceFile = new File(logPath);
        File anrFile = new File(anrLogPath);
        if (!traceFile.renameTo(anrFile)) {
            return;
        }

        ITACrashCallback callback = NativeHandler.getInstance().anrCallback;
        if (callback != null) {
            try {
                callback.onCrash(CrashType.ANR_CRASH, anrLogPath, emergency);
            } catch (Exception e) {
                TACrashLogger.error(TAG, "NativeHandler ANR callback.onCrash failed", e);
            }
        }
    }

    //Initialize native
    private static native int nativeInit(
            int apiLevel,
            String osVersion,
            String abiList,
            String manufacturer,
            String brand,
            String model,
            String buildFingerprint,
            String appId,
            String appVersion,
            String appLibDir,
            String logDir,
            boolean crashEnable,
            boolean crashRethrow,
            boolean traceEnable,
            boolean traceRethrow);

    void notifyJavaCrashed() {
        if (initNativeSuccess && anrEnable) {
            nativeNotifyJavaCrashed();
        }
    }

    private static native void nativeNotifyJavaCrashed();

    public static native void testNativeCrash(int runInNewThread);
}
