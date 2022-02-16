/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

import android.os.Build;
import android.os.Debug;
import android.text.TextUtils;
import java.io.File;
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Locale;
import java.util.Map;

/**
 * Used to catch native exceptions.
 * contains anr & nativeCrash.
 *
 * @author  bugliee
 * @version 1.0.0
 */
public class NativeHandler {
    private static final String TAG = TDConstants.TAG + ".NativeHandler";

    private static final NativeHandler instance = new NativeHandler();
    private long anrTimeoutMs = 15 * 1000;

    private boolean crashRethrow;
    ITDCrashCallback crashCallback;
    private boolean anrCheckProcessState;
    private ITDCrashCallback anrCallback;

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
    public int initialize(ITDLibLoader libLoader,
                   String appId,
                   String appVersion,
                   String logDir,
                   boolean crashEnable,
                   boolean crashRethrow,
                   int crashLogcatSystemLines,
                   int crashLogcatEventsLines,
                   int crashLogcatMainLines,
                   boolean crashDumpElfHash,
                   boolean crashDumpMap,
                   boolean crashDumpFds,
                   boolean crashDumpNetworkInfo,
                   boolean crashDumpAllThreads,
                   int crashDumpAllThreadsCountMax,
                   String[] crashDumpAllThreadsWhiteList,
                   ITDCrashCallback crashCallback,
                   boolean anrEnable,
                   boolean anrRethrow,
                   boolean anrCheckProcessState,
                   int anrLogcatSystemLines,
                   int anrLogcatEventsLines,
                   int anrLogcatMainLines,
                   boolean anrDumpFds,
                   boolean anrDumpNetworkInfo,
                   ITDCrashCallback anrCallback) {
        //load so
        if (libLoader == null) {
            try {
                System.loadLibrary("tdcrash");
            } catch (Throwable e) {
                TDCrashLogger.error(TAG, "NativeHandler System.loadLibrary failed", e);
                return TDConstants.LOAD_LIBRARY_FAILED;
            }
        } else {
            try {
                libLoader.loadLibrary("tdcrash");
            } catch (Throwable e) {
                TDCrashLogger.error(TAG, "NativeHandler ITDLibLoader.loadLibrary failed", e);
                return TDConstants.LOAD_LIBRARY_FAILED;
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
                    ActivityMonitor.getInstance().getApplication().getApplicationInfo()
                            .nativeLibraryDir,
                    logDir,
                    crashEnable,
                    crashRethrow,
                    crashLogcatSystemLines,
                    crashLogcatEventsLines,
                    crashLogcatMainLines,
                    crashDumpElfHash,
                    crashDumpMap,
                    crashDumpFds,
                    crashDumpNetworkInfo,
                    crashDumpAllThreads,
                    crashDumpAllThreadsCountMax,
                    crashDumpAllThreadsWhiteList,
                    anrEnable,
                    anrRethrow,
                    anrLogcatSystemLines,
                    anrLogcatEventsLines,
                    anrLogcatMainLines,
                    anrDumpFds,
                    anrDumpNetworkInfo);
            if (r != 0) {
                TDCrashLogger.error(TAG, "NativeHandler init failed");
                return TDConstants.INIT_LIBRARY_FAILED;
            }
            initNativeSuccess = true;
            return TDConstants.OK;
        } catch (Throwable e) {
            TDCrashLogger.error(TAG, "NativeHandler init failed", e);
            return TDConstants.INIT_LIBRARY_FAILED;
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
            TDCrashLogger.error(TAG, "NativeHandler getStacktraceByThreadName failed", e);
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
            TDCrashLogger.error(TAG,  "FileManager appendText failed", e);
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

    static String getProcessMemoryInfo() {
        StringBuilder sb = new StringBuilder();
        sb.append(" Process Summary (From: android.os.Debug.MemoryInfo)\n");
        sb.append(String.format(Locale.US, TDConstants.memInfoFmt, "", "Pss(KB)"));
        sb.append(String.format(Locale.US, TDConstants.memInfoFmt, "", "------"));

        try {
            Debug.MemoryInfo mi = new Debug.MemoryInfo();
            Debug.getMemoryInfo(mi);

            if (Build.VERSION.SDK_INT >= 23) {
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Java Heap:", mi.getMemoryStat("summary.java-heap")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Native Heap:", mi.getMemoryStat("summary.native-heap")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Code:", mi.getMemoryStat("summary.code")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Stack:", mi.getMemoryStat("summary.stack")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Graphics:", mi.getMemoryStat("summary.graphics")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Private Other:", mi.getMemoryStat("summary.private-other")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "System:", mi.getMemoryStat("summary.system")));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt2,
                        "TOTAL:", mi.getMemoryStat("summary.total-pss"), "TOTAL SWAP:",
                        mi.getMemoryStat("summary.total-swap")));
            } else {
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Java Heap:", "~ " + mi.dalvikPrivateDirty));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Native Heap:", mi.nativePrivateDirty));
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "Private Other:", "~ " + mi.otherPrivateDirty));
                if (Build.VERSION.SDK_INT >= 19) {
                    sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                            "System:", (mi.getTotalPss()
                                    - mi.getTotalPrivateDirty() - mi.getTotalPrivateClean())));
                } else {
                    sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                            "System:", "~ " + (mi.getTotalPss() - mi.getTotalPrivateDirty())));
                }
                sb.append(String.format(Locale.US, TDConstants.memInfoFmt,
                        "TOTAL:", mi.getTotalPss()));
            }
        } catch (Exception e) {
            TDCrashLogger.error(TAG, "Util getProcessMemoryInfo failed", e);
        }

        return sb.toString();
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

            appendText(logPath, "memory info", getProcessMemoryInfo());

            appendText(logPath, "foreground",
                    ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no");
        }

        ITDCrashCallback callback = NativeHandler.getInstance().crashCallback;
        if (callback != null) {
            try {
                callback.onCrash(CrashType.NATIVE_CRASH, logPath, emergency);
            } catch (Exception e) {
                TDCrashLogger.error(TAG,  "NativeHandler native crash callback.onCrash failed", e);
            }
        }

        if (!NativeHandler.getInstance().crashRethrow) {
            ActivityMonitor.getInstance().finishAllActivities();
        }
    }

    private static void traceCallback(String logPath, String emergency) {
        if (TextUtils.isEmpty(logPath)) {
            return;
        }

        appendText(logPath, "memory info", getProcessMemoryInfo());

        appendText(logPath, "foreground",
                ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no");

        if (NativeHandler.getInstance().anrCheckProcessState) {
            if (!Utils.checkProcessAnrState(ActivityMonitor.getInstance().getApplication(),
                    NativeHandler.getInstance().anrTimeoutMs)) {
                return;
            }
        }

        String anrLogPath = logPath.substring(0, logPath.length()
                - TDConstants.traceLogSuffix.length()) + TDConstants.anrLogSuffix;
        File traceFile = new File(logPath);
        File anrFile = new File(anrLogPath);
        if (!traceFile.renameTo(anrFile)) {
            return;
        }

        ITDCrashCallback callback = NativeHandler.getInstance().anrCallback;
        if (callback != null) {
            try {
                callback.onCrash(CrashType.ANR_CRASH, anrLogPath, emergency);
            } catch (Exception e) {
                TDCrashLogger.error(TAG, "NativeHandler ANR callback.onCrash failed", e);
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
            int crashLogcatSystemLines,
            int crashLogcatEventsLines,
            int crashLogcatMainLines,
            boolean crashDumpElfHash,
            boolean crashDumpMap,
            boolean crashDumpFds,
            boolean crashDumpNetworkInfo,
            boolean crashDumpAllThreads,
            int crashDumpAllThreadsCountMax,
            String[] crashDumpAllThreadsWhiteList,
            boolean traceEnable,
            boolean traceRethrow,
            int traceLogcatSystemLines,
            int traceLogcatEventsLines,
            int traceLogcatMainLines,
            boolean traceDumpFds,
            boolean traceDumpNetworkInfo);

    void notifyJavaCrashed() {
        if (initNativeSuccess && anrEnable) {
            nativeNotifyJavaCrashed();
        }
    }

    private static native void nativeNotifyJavaCrashed();

    public static native void testNativeCrash(int runInNewThread);
}
