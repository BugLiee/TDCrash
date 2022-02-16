/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

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
import java.util.Map;
import java.util.regex.Pattern;

/**
 * Used to catch Java exceptions.
 *
 * @author  bugliee
 * @version 1.0.0
 * @see java.lang.Thread.UncaughtExceptionHandler
 */
public final class JavaCrashHandler implements Thread.UncaughtExceptionHandler {
    private static final String TAG = TDConstants.TAG + ".JavaCrashHandler";

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

    private int logcatSystemLines;

    private int logcatEventsLines;

    private int logcatMainLines;

    private boolean dumpFds;

    private boolean dumpNetworkInfo;

    private boolean dumpAllThreads;

    private int dumpAllThreadsCountMax;

    private String[] dumpAllThreadsWhiteList;

    private ITDCrashCallback callback;

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
     * @param logcatSystemLines logcatSystemLines
     * @param logcatEventsLines logcatEventsLines
     * @param logcatMainLines logcatMainLines
     * @param dumpFds dumpFds
     * @param dumpNetworkInfo dumpNetworkInfo
     * @param dumpAllThreads dumpAllThreads
     * @param dumpAllThreadsCountMax dumpAllThreadsCountMax
     * @param dumpAllThreadsWhiteList dumpAllThreadsWhiteList
     * @param callback callback
     * */
    public void initialize(int pid, String processName,
                           String appId, String appVersion,
                           String logDir, boolean rethrow, int logcatSystemLines,
                           int logcatEventsLines, int logcatMainLines,
                           boolean dumpFds, boolean dumpNetworkInfo, boolean dumpAllThreads,
                           int dumpAllThreadsCountMax, String[] dumpAllThreadsWhiteList,
                           ITDCrashCallback callback) {
        this.pid = pid;
        this.processName = processName;
        this.appId = appId;
        this.appVersion = appVersion;
        this.logDir = logDir;
        this.rethrow = rethrow;
        this.logcatSystemLines = logcatSystemLines;
        this.logcatEventsLines = logcatEventsLines;
        this.logcatMainLines = logcatMainLines;
        this.dumpFds = dumpFds;
        this.dumpNetworkInfo = dumpNetworkInfo;
        this.dumpAllThreads = dumpAllThreads;
        this.dumpAllThreadsCountMax = dumpAllThreadsCountMax;
        this.dumpAllThreadsWhiteList = dumpAllThreadsWhiteList;
        this.callback = callback;
        // Save previous handler
        this.defaultHandler = Thread.getDefaultUncaughtExceptionHandler();

        try {
            //Set current handler
            Thread.setDefaultUncaughtExceptionHandler(this);
        } catch (Exception e) {
            TDCrashLogger.error(TAG,
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
            TDCrashLogger.error(TAG, "JavaCrashHandler : handleException failed! ", e);
        }
        if (this.rethrow) {
            if (defaultHandler != null) {
                defaultHandler.uncaughtException(thread, throwable);
            }
        } else {
            ActivityMonitor.getInstance().finishAllActivities();
            Process.killProcess(this.pid);
            System.exit(14);
        }
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
                    logDir, TDConstants.logPrefix, crashTime.getTime(),
                    appVersion, processName, TDConstants.javaLogSuffix);
            logFile = Utils.createFile(logPath);
        } catch (Exception e) {
            TDCrashLogger.error(TAG, "JavaCrashHandler createFile failed! ", e);
        }

        String emergency = null;
        try {
            emergency = getEmergency(crashTime, thread, throwable);
        } catch (Exception e) {
            TDCrashLogger.error(TAG, "JavaCrashHandler getEmergency failed", e);
        }

        if (logFile != null) {
            RandomAccessFile raf = null;
            try {
                raf = new RandomAccessFile(logFile, "rws");

                if (emergency != null) {
                    raf.write(emergency.getBytes("UTF-8"));
                }

                emergency = null;
                if (logcatSystemLines > 0 || logcatEventsLines > 0 || logcatMainLines > 0) {
                    raf.write(Utils.getLogcat(logcatMainLines, logcatSystemLines, logcatEventsLines)
                            .getBytes("UTF-8"));
                }

                if (dumpFds) {
                    raf.write(Utils.getFds().getBytes("UTF-8"));
                }

                if (dumpNetworkInfo) {
                    raf.write(Utils.getNetworkInfo().getBytes("UTF-8"));
                }

                if (dumpAllThreads) {
                    raf.write(getOtherThreadsInfo(thread).getBytes("UTF-8"));
                }

                raf.write(Utils.getMemoryInfo().getBytes("UTF-8"));

                raf.write(("foreground:\n"
                        + (ActivityMonitor.getInstance().isApplicationForeground() ? "yes" : "no")
                        + "\n\n").getBytes("UTF-8"));

            } catch (Exception e) {
                TDCrashLogger.error(TAG, "JavaCrashHandler write log file failed!", e);
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
                        new SimpleDateFormat(TDConstants.timeFormatterStr, Locale.US);
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

                libPathList.add(TDCrash.getInstance().config.nativeLibDir + "/" + libName);
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
                TDConstants.javaCrashType, appId, appVersion)
                + "pid: " + pid + ", tid: "
                + Process.myTid() + ", name: "
                + thread.getName() + "  >>> " + processName + " <<<\n"
                + "\n"
                + "java stacktrace:\n"
                + stacktrace
                + "\n"
                + getBuildId(stacktrace);
    }

    private String getOtherThreadsInfo(Thread crashedThread) {

        int thdMatchedRegex = 0;
        int thdIgnoredByLimit = 0;
        int thdDumped = 0;

        ArrayList<Pattern> whiteList = null;
        if (dumpAllThreadsWhiteList != null) {
            whiteList = new ArrayList<>();
            for (String s : dumpAllThreadsWhiteList) {
                try {
                    whiteList.add(Pattern.compile(s));
                } catch (Exception e) {
                    TDCrashLogger.warn(TAG,
                            "JavaCrashHandler pattern compile failed", e);
                }
            }
        }

        StringBuilder sb = new StringBuilder();
        Map<Thread, StackTraceElement[]> map = Thread.getAllStackTraces();
        for (Map.Entry<Thread, StackTraceElement[]> entry : map.entrySet()) {

            Thread thd = entry.getKey();

            //skip current thread
            if (thd.getName().equals(crashedThread.getName())) {
                continue;
            }

            //skip whiteList
            if (whiteList != null && !matchThreadName(whiteList, thd.getName())) {
                continue;
            }
            thdMatchedRegex++;

            if (dumpAllThreadsCountMax > 0 && thdDumped >= dumpAllThreadsCountMax) {
                thdIgnoredByLimit++;
                continue;
            }

            sb.append(TDConstants.sepOtherThreads + "\n");
            sb.append("pid: ")
                    .append(pid).append(", tid: ")
                    .append(thd.getId()).append(", name: ")
                    .append(thd.getName()).append("  >>> ")
                    .append(processName).append(" <<<\n");
            sb.append("\n");
            sb.append("java stacktrace:\n");

            StackTraceElement[] stacktrace = entry.getValue();
            for (StackTraceElement element : stacktrace) {
                sb.append("    at ").append(element.toString()).append("\n");
            }
            sb.append("\n");

            thdDumped++;
        }

        if (map.size() > 1) {
            if (thdDumped == 0) {
                sb.append(TDConstants.sepOtherThreads + "\n");
            }

            sb.append("total JVM threads (exclude the crashed thread): ")
                    .append(map.size() - 1)
                    .append("\n");
            if (whiteList != null) {
                sb.append("JVM threads matched whitelist: ").append(thdMatchedRegex).append("\n");
            }
            if (dumpAllThreadsCountMax > 0) {
                sb.append("JVM threads ignored by max count limit: ")
                        .append(thdIgnoredByLimit).append("\n");
            }
            sb.append("dumped JVM threads:").append(thdDumped).append("\n");
            sb.append(TDConstants.sepOtherThreadsEnding + "\n");
        }

        return sb.toString();
    }

    private boolean matchThreadName(ArrayList<Pattern> whiteList, String threadName) {
        for (Pattern pat : whiteList) {
            if (pat.matcher(threadName).matches()) {
                return true;
            }
        }
        return false;
    }
}
