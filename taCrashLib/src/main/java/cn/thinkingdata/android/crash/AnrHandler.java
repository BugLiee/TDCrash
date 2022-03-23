/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

import static android.os.FileObserver.CLOSE_WRITE;

import android.os.Build;
import android.os.FileObserver;
import android.text.TextUtils;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.RandomAccessFile;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Used to catch ANR exceptions.
 * require SDK_INT < 21.
 *
 * @author  bugliee
 * @version 1.0.0
 */
public class AnrHandler {
    private static final String TAG = TAConstants.TAG + ".AnrHandler";

    private static final AnrHandler instance = new AnrHandler();

    private final Date startTime = new Date();

    private final Pattern patPidTime
            = Pattern.compile("^-----\\spid\\s(\\d+)\\sat\\s(.*)\\s-----$");

    private final Pattern patProcessName
            = Pattern.compile("^Cmd\\sline:\\s+(.*)$");

    private final long anrTimeoutMs = 15 * 1000;

    private int pid;

    private String processName;

    private String appId;

    private String appVersion;

    private String logDir;

    private boolean checkProcessState;

    private ITACrashCallback callback;

    private long lastTime = 0;

    private FileObserver fileObserver;

    /**
     * Construction method.
     * */
    private AnrHandler() {
    }

    public static AnrHandler getInstance() {
        return instance;
    }

    /**
     * Initialize method.
     * */
    public void initialize(int pid,
                    String processName,
                    String appId,
                    String appVersion,
                    String logDir,
                    boolean checkProcessState,
                    ITACrashCallback callback) {

        //check API level
        if (Build.VERSION.SDK_INT >= 21) {
            return;
        }

        this.pid = pid;
        this.processName = (TextUtils.isEmpty(processName) ? "unknown" : processName);
        this.appId = appId;
        this.appVersion = appVersion;
        this.logDir = logDir;
        this.checkProcessState = checkProcessState;
        this.callback = callback;

        //This method is deprecated in Android 10(Api 29).
        fileObserver = new FileObserver("/data/anr/", CLOSE_WRITE) {
            public void onEvent(int event, String path) {
                try {
                    if (path != null) {
                        String filepath = "/data/anr/" + path;
                        if (filepath.contains("trace")) {
                            handleAnr(filepath);
                            try {
                                Thread.sleep(1000);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                    }
                } catch (Exception e) {
                    TACrashLogger.error(TAG,
                            "AnrHandler fileObserver onEvent failed", e);
                }
            }
        };

        try {
            fileObserver.startWatching();
        } catch (Exception e) {
            fileObserver = null;
            TACrashLogger.error(TAG,
                    "AnrHandler fileObserver startWatching failed", e);
        }
    }

    void notifyJavaCrashed() {
        if (fileObserver != null) {
            try {
                fileObserver.stopWatching();
            } catch (Exception e) {
                TACrashLogger.error(TAG,
                        "AnrHandler fileObserver stopWatching failed", e);
            } finally {
                fileObserver = null;
            }
        }
    }

    private void handleAnr(String filepath) {
        Date anrTime = new Date();

        if (anrTime.getTime() - lastTime < anrTimeoutMs) {
            return;
        }

        if (this.checkProcessState) {
            if (Utils.checkProcessAnrState(
                    TACrash.getInstance().config.context, anrTimeoutMs)) {
                return;
            }
        }

        String trace = getTrace(filepath, anrTime.getTime());
        if (TextUtils.isEmpty(trace)) {
            return;
        }

        lastTime = anrTime.getTime();

        String emergency = null;
        try {
            emergency = getEmergency(anrTime, trace);
        } catch (Exception e) {
            TACrashLogger.error(TAG, "AnrHandler getEmergency failed", e);
        }

        File logFile = null;
        try {
            String logPath = String.format(Locale.US, "%s/%s_%d_%s_%s_%s",
                    logDir,
                    TAConstants.logPrefix,
                    anrTime.getTime(),
                    appVersion, processName, TAConstants.anrLogSuffix);
            logFile = Utils.createFile(logPath);
        } catch (Exception e) {
            TACrashLogger.error(TAG, "AnrHandler createLogFile failed", e);
        }

        if (logFile != null) {
            RandomAccessFile raf = null;
            try {
                raf = new RandomAccessFile(logFile, "rws");

                if (emergency != null) {
                    raf.write(emergency.getBytes("UTF-8"));
                }

                emergency = null;

            } catch (Exception e) {
                TACrashLogger.error(TAG, "AnrHandler write log file failed", e);
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
                    callback.onCrash(CrashType.ANR_CRASH, logFile.getAbsolutePath(), emergency);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private String getEmergency(Date anrTime, String trace) {
        return Utils.getLogHeader(startTime, anrTime, TAConstants.anrCrashType, appId, appVersion)
                + "pid: " + pid + "  >>> " + processName + " <<<\n"
                + "\n"
                + TAConstants.sepOtherThreads
                + "\n"
                + trace
                + "\n"
                + TAConstants.sepOtherThreadsEnding
                + "\n\n";
    }

    private String getTrace(String filepath, long anrTime) {
        BufferedReader br = null;
        String line;
        Matcher matcher;
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US);
        StringBuilder sb = new StringBuilder();
        boolean found = false;

        try {
            br = new BufferedReader(new FileReader(filepath));
            while ((line = br.readLine()) != null) {
                if (!found && line.startsWith("----- pid ")) {
                    matcher = patPidTime.matcher(line);
                    if (!matcher.find() || matcher.groupCount() != 2) {
                        continue;
                    }
                    String ssPid = matcher.group(1);
                    String ssLogTime = matcher.group(2);
                    if (ssPid == null || ssLogTime == null) {
                        continue;
                    }
                    if (pid != Integer.parseInt(ssPid)) {
                        continue;
                    }
                    Date ddLogTime = dateFormat.parse(ssLogTime);
                    if (ddLogTime == null) {
                        continue;
                    }
                    long logTime = ddLogTime.getTime();
                    if (Math.abs(logTime - anrTime) > anrTimeoutMs) {
                        continue;
                    }
                    line = br.readLine();
                    if (line == null) {
                        break;
                    }
                    matcher = patProcessName.matcher(line);
                    if (!matcher.find() || matcher.groupCount() != 1) {
                        continue;
                    }
                    String ppName = matcher.group(1);
                    if (ppName == null || !(ppName.equals(this.processName))) {
                        continue;
                    }

                    found = true;

                    sb.append(line).append('\n');
                    sb.append("Mode: Watching /data/anr/*\n");

                    continue;
                }

                if (found) {
                    if (line.startsWith("----- end ")) {
                        break;
                    } else {
                        sb.append(line).append('\n');
                    }
                }
            }
            return sb.toString();
        } catch (Exception ignored) {
            return null;
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
    }
}
