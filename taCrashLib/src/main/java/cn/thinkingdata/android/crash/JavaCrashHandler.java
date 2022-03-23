/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

import android.os.Process;
import java.io.File;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.StringWriter;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.TimerTask;

/**
 * Used to catch Java exceptions.
 *
 * @author  bugliee
 * @version 1.0.0
 * @see java.lang.Thread.UncaughtExceptionHandler
 */
public final class JavaCrashHandler implements Thread.UncaughtExceptionHandler {
    private static final String TAG = TAConstants.TAG + ".JavaCrashHandler";

    private static final JavaCrashHandler instance = new JavaCrashHandler();

    private final Date startTime = new Date();

    private int pid;

    private String processName;

    private String appId;

    private String appVersion;

    /**
     * rethrow exception.
     * */
    private boolean rethrow;

    private String logDir;

    private ITACrashCallback callback;

    private Thread.UncaughtExceptionHandler defaultHandler = null;

    private JavaCrashHandler() {}

    public static JavaCrashHandler getInstance() {
        return instance;
    }

    /**
     * Initialize method.
     *
     * @param pid pid
     * @param processName processName
     * @param appId appId
     * @param appVersion appVersion
     * @param logDir logDir
     * @param rethrow rethrow
     * @param callback callback
     * */
    public void initialize(int pid, String processName,
                           String appId, String appVersion,
                           String logDir, boolean rethrow,
                           ITACrashCallback callback) {
        this.pid = pid;
        this.processName = processName;
        this.appId = appId;
        this.appVersion = appVersion;
        this.logDir = logDir;
        this.rethrow = rethrow;
        this.callback = callback;
        // Save previous handler
        this.defaultHandler = Thread.getDefaultUncaughtExceptionHandler();

        try {
            //Set current handler
            Thread.setDefaultUncaughtExceptionHandler(this);
        } catch (Exception e) {
            TACrashLogger.error(TAG,
                    "JavaCrashHandler : setDefaultUncaughtExceptionHandler failed! ", e);
        }
    }

    @Override
    public void uncaughtException(Thread thread, Throwable throwable) {
        if (defaultHandler != null) {
            //Restore handler
            Thread.setDefaultUncaughtExceptionHandler(defaultHandler);
        }

        try {
            //Handle exception
            handleException(thread, throwable);
        } catch (Exception e) {
            TACrashLogger.error(TAG, "JavaCrashHandler : handleException failed! ", e);
        }
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        if (this.rethrow && defaultHandler != null) {
            defaultHandler.uncaughtException(thread, throwable);
        } else {
            killProcess();
        }
    }

    private void killProcess() {
        Process.killProcess(this.pid);
        System.exit(14);
    }

    private void handleException(Thread thread, Throwable throwable) {
        //crashTime
        Date crashTime = new Date();

        //notify crash
        AnrHandler.getInstance().notifyJavaCrashed();
        NativeHandler.getInstance().notifyJavaCrashed();

        File logFile = null;
        try {
            String logPath = String.format(Locale.US, "%s/%s_%d_%s_%s_%s",
                    logDir, TAConstants.logPrefix, crashTime.getTime(),
                    appVersion, processName, TAConstants.javaLogSuffix);
            logFile = Utils.createFile(logPath);
        } catch (Exception e) {
            TACrashLogger.error(TAG, "JavaCrashHandler createFile failed! ", e);
        }

        String emergency = null;
        try {
            emergency = getEmergency(crashTime, thread, throwable);
        } catch (Exception e) {
            TACrashLogger.error(TAG, "JavaCrashHandler getEmergency failed", e);
        }

        if (logFile != null) {
            RandomAccessFile raf = null;
            try {
                raf = new RandomAccessFile(logFile, "rws");

                if (emergency != null) {
                    raf.write(emergency.getBytes("UTF-8"));
                }

                emergency = null;
                raf.write(("foreground:\n"
                        + (Utils.isForeground(TACrash.getInstance().config.context) ? "yes" : "no")
                        + "\n\n").getBytes("UTF-8"));

            } catch (Exception e) {
                TACrashLogger.error(TAG, "JavaCrashHandler write log file failed!", e);
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

        if (callback != null) {
            try {
                if (logFile == null) {
                    callback.onCrash(null, null, emergency);
                } else {
                    callback.onCrash(CrashType.JAVA_CRASH, logFile.getAbsolutePath(), emergency);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private String getLibInfo(List<String> libPathList) {
        StringBuilder sb = new StringBuilder();
        for (String libPath : libPathList) {
            File libFile = new File(libPath);
            if (libFile.exists() && libFile.isFile()) {
                String md5 = Utils.getFileMD5(libFile);

                DateFormat timeFormatter =
                        new SimpleDateFormat(TAConstants.timeFormatterStr, Locale.US);
                Date lastTime = new Date(libFile.lastModified());

                sb.append("    ")
                        .append(libPath)
                        .append("(BuildId: unknown. FileSize: ")
                        .append(libFile.length()).append(". LastModified: ")
                        .append(timeFormatter.format(lastTime)).append(". MD5: ")
                        .append(md5).append(")\n");
            } else {
                sb.append("    ").append(libPath).append(" (Not found)\n");
            }
        }

        return sb.toString();
    }

    private String getBuildId(String stkTrace) {
        String buildId = "";
        List<String> libPathList = new ArrayList<>();
        if (stkTrace.contains("UnsatisfiedLinkError")) {
            String libInfo = null;
            String[] tempLibPathStr;
            tempLibPathStr = stkTrace.split("\"");
            for (String libPathStr : tempLibPathStr) {
                if (libPathStr.isEmpty() || !libPathStr.endsWith(".so")) {
                    continue;
                }
                libPathList.add(libPathStr);

                String libName = libPathStr.substring(libPathStr.lastIndexOf('/') + 1);

                libPathList.add(TACrash.getInstance().config.nativeLibDir + "/" + libName);
                libPathList.add("/vendor/lib/" + libName);
                libPathList.add("/vendor/lib64/" + libName);
                libPathList.add("/system/lib/" + libName);
                libPathList.add("/system/lib64/" + libName);

                libInfo = getLibInfo(libPathList);
            }

            buildId = "build id:"
                    + "\n"
                    + libInfo
                    + "\n";
        }

        return buildId;
    }

    private String getEmergency(Date crashTime, Thread thread, Throwable throwable) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        String stacktrace = sw.toString();

        return Utils.getLogHeader(startTime, crashTime,
                TAConstants.javaCrashType, appId, appVersion)
                + "pid: " + pid + ", tid: "
                + Process.myTid() + ", name: "
                + thread.getName() + "  >>> " + processName + " <<<\n"
                + "\n"
                + "java stacktrace:\n"
                + stacktrace
                + "\n"
                + getBuildId(stacktrace);
    }
}
