/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Debug;
import android.system.Os;
import android.text.TextUtils;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Utils.
 *
 * @author bugliee
 * @version 1.0.0
 */
public class Utils {
    private static final String TAG = TDConstants.TAG + ".Utils";

    private static final String memInfoFmt = "%21s %8s\n";
    private static final String memInfoFmt2 = "%21s %8s %21s %8s\n";
    private static final String[] suPathname = {
        "/data/local/su",
        "/data/local/bin/su",
        "/data/local/xbin/su",
        "/system/xbin/su",
        "/system/bin/su",
        "/system/bin/.ext/su",
        "/system/bin/failsafe/su",
        "/system/sd/xbin/su",
        "/system/usr/we-need-root/su",
        "/sbin/su",
        "/su/bin/su"};

    /**
     * create a file with a path if this file isn't exist.
     *
     * @param path 路径
     */
    public static File createFile(String path) {
        String dirStr = path.substring(0, path.lastIndexOf("/"));
        if (!checkAndCreateDir(dirStr)) {
            return null;
        }
        File file = new File(path);

        try {
            if (file.createNewFile()) {
                return file;
            } else {
                TDCrashLogger.error(TAG,
                        "createFile by createNewFile failed, file already exists");
                return null;
            }
        } catch (Exception e) {
            TDCrashLogger.error(TAG,
                    "createFile by createNewFile failed", e);
            return null;
        }
    }

    /**
     * create a Directory with a path if this Directory isn't exist.
     *
     * @param path path
     */
    public static boolean checkAndCreateDir(String path) {
        File dir = new File(path);
        try {
            if (!dir.exists()) {
                boolean mkdirSuccess = dir.mkdirs();
                if (mkdirSuccess) {
                    return dir.exists() && dir.isDirectory();
                } else {
                    return false;
                }
            } else {
                return dir.isDirectory();
            }
        } catch (Exception ignored) {
            return false;
        }
    }

    /**
     * Get System's MemoryInfo.
     */
    public static String getMemoryInfo() {
        return "memory info:\n"
                + " System Summary (From: /proc/meminfo)\n"
                + getFileContent("/proc/meminfo")
                + "-\n"
                + " Process Status (From: /proc/PID/status)\n"
                + getFileContent("/proc/self/status")
                + "-\n"
                + " Process Limits (From: /proc/PID/limits)\n"
                + getFileContent("/proc/self/limits")
                + "-\n"
                + getProcessMemoryInfo()
                + "\n";
    }

    static String getProcessMemoryInfo() {
        StringBuilder sb = new StringBuilder();
        sb.append(" Process Summary (From: android.os.Debug.MemoryInfo)\n");
        sb.append(String.format(Locale.US, memInfoFmt, "", "Pss(KB)"));
        sb.append(String.format(Locale.US, memInfoFmt, "", "------"));

        try {
            Debug.MemoryInfo mi = new Debug.MemoryInfo();
            Debug.getMemoryInfo(mi);

            if (Build.VERSION.SDK_INT >= 23) {
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Java Heap:", mi.getMemoryStat("summary.java-heap")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Native Heap:", mi.getMemoryStat("summary.native-heap")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Code:", mi.getMemoryStat("summary.code")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Stack:", mi.getMemoryStat("summary.stack")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Graphics:", mi.getMemoryStat("summary.graphics")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Private Other:", mi.getMemoryStat("summary.private-other")));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "System:", mi.getMemoryStat("summary.system")));
                sb.append(String.format(Locale.US, memInfoFmt2,
                        "TOTAL:", mi.getMemoryStat("summary.total-pss"),
                        "TOTAL SWAP:", mi.getMemoryStat("summary.total-swap")));
            } else {
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Java Heap:", "~ " + mi.dalvikPrivateDirty));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Native Heap:", mi.nativePrivateDirty));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "Private Other:", "~ " + mi.otherPrivateDirty));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "System:", "~ " + (mi.getTotalPss() - mi.getTotalPrivateDirty())));
                sb.append(String.format(Locale.US, memInfoFmt,
                        "TOTAL:", mi.getTotalPss()));
            }
        } catch (Exception e) {
            TDCrashLogger.info(TAG, "Util getProcessMemoryInfo failed", e);
        }

        return sb.toString();
    }

    static String getFds() {
        StringBuilder sb = new StringBuilder("open files:\n");

        try {
            File dir = new File("/proc/self/fd");
            File[] fds = dir.listFiles((dir1, name) -> TextUtils.isDigitsOnly(name));

            int count = 0;
            if (fds != null) {
                for (File fd : fds) {
                    String path = null;
                    try {
                        if (Build.VERSION.SDK_INT >= 21) {
                            path = Os.readlink(fd.getAbsolutePath());
                        } else {
                            path = fd.getCanonicalPath();
                        }
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    if (path != null) {
                        sb.append("    fd ").append(fd.getName()).append(": ")
                                .append(TextUtils.isEmpty(path) ? "???" : path.trim()).append('\n');
                    }

                    count++;
                    if (count > 1024) {
                        break;
                    }
                }

                if (fds.length > 1024) {
                    sb.append("    ......\n");
                }

                sb.append("    (number of FDs: ").append(fds.length).append(")\n");
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        sb.append('\n');
        return sb.toString();
    }

    private static String getFileContent(String pathname) {
        return getFileContent(pathname, 0);
    }

    private static String getFileContent(String pathname, int limit) {
        StringBuilder sb = new StringBuilder();
        BufferedReader br = null;
        String line;
        int n = 0;
        try {
            br = new BufferedReader(new FileReader(pathname));
            while (null != (line = br.readLine())) {
                String p = line.trim();
                if (p.length() > 0) {
                    n++;
                    if (limit == 0 || n <= limit) {
                        sb.append("  ").append(p).append("\n");
                    }
                }
            }
            if (limit > 0 && n > limit) {
                sb.append("  ......\n").append("  (number of records: ").append(n).append(")\n");
            }
        } catch (Exception e) {
            TDCrashLogger.error(TAG, "Utils getInfo(" + pathname + ") failed", e);
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        return sb.toString();
    }

    static String getLogcat(int logcatMainLines, int logcatSystemLines, int logcatEventsLines) {
        int pid = android.os.Process.myPid();
        StringBuilder sb = new StringBuilder();

        sb.append("logcat:\n");

        if (logcatMainLines > 0) {
            getLogcatByBufferName(pid, sb, "main", logcatMainLines, 'D');
        }
        if (logcatSystemLines > 0) {
            getLogcatByBufferName(pid, sb, "system", logcatSystemLines, 'W');
        }
        if (logcatEventsLines > 0) {
            getLogcatByBufferName(pid, sb, "events", logcatSystemLines, 'I');
        }

        sb.append("\n");

        return sb.toString();
    }

    private static void getLogcatByBufferName(int pid,
                                              StringBuilder sb,
                                              String bufferName,
                                              int lines,
                                              char priority) {
        boolean withPid = (android.os.Build.VERSION.SDK_INT >= 24);
        String pidString = Integer.toString(pid);
        String pidLabel = " " + pidString + " ";

        //command for ProcessBuilder
        List<String> command = new ArrayList<>();
        command.add("/system/bin/logcat");
        command.add("-b");
        command.add(bufferName);
        command.add("-d");
        command.add("-v");
        command.add("threadtime");
        command.add("-t");
        command.add(Integer.toString(withPid ? lines : (int) (lines * 1.2)));
        if (withPid) {
            command.add("--pid");
            command.add(pidString);
        }
        command.add("*:" + priority);

        //append the command line
        Object[] commandArray = command.toArray();
        sb.append("--------- tail end of log ").append(bufferName);
        sb.append(" (").append(android.text.TextUtils.join(" ", commandArray)).append(")\n");

        //append logs
        BufferedReader br = null;
        String line;
        try {
            Process process = new ProcessBuilder().command(command).start();
            br = new BufferedReader(new InputStreamReader(process.getInputStream()));
            while ((line = br.readLine()) != null) {
                if (withPid || line.contains(pidLabel)) {
                    sb.append(line).append("\n");
                }
            }
        } catch (Exception e) {
            TDCrashLogger.warn(TAG, "Util run logcat command failed", e);
        } finally {
            if (br != null) {
                try {
                    br.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * Get System's NetworkInfo.
     */
    public static String getNetworkInfo() {
        if (Build.VERSION.SDK_INT >= 29) {
            return "network info:\n"
                    + "Not supported on Android Q (API level 29) and later.\n"
                    + "\n";
        } else {
            return "network info:\n"
                    + " TCP over IPv4 (From: /proc/PID/net/tcp)\n"
                    + getFileContent("/proc/self/net/tcp", 1024)
                    + "-\n"
                    + " TCP over IPv6 (From: /proc/PID/net/tcp6)\n"
                    + getFileContent("/proc/self/net/tcp6", 1024)
                    + "-\n"
                    + " UDP over IPv4 (From: /proc/PID/net/udp)\n"
                    + getFileContent("/proc/self/net/udp", 1024)
                    + "-\n"
                    + " UDP over IPv6 (From: /proc/PID/net/udp6)\n"
                    + getFileContent("/proc/self/net/udp6", 1024)
                    + "-\n"
                    + " ICMP in IPv4 (From: /proc/PID/net/icmp)\n"
                    + getFileContent("/proc/self/net/icmp", 256)
                    + "-\n"
                    + " ICMP in IPv6 (From: /proc/PID/net/icmp6)\n"
                    + getFileContent("/proc/self/net/icmp6", 256)
                    + "-\n"
                    + " UNIX domain (From: /proc/PID/net/unix)\n"
                    + getFileContent("/proc/self/net/unix", 256)
                    + "\n";
        }
    }

    /**
     * Get ProcessName.
     *
     * @param ctx Context
     * @param pid pid
     */
    public static String getProcessName(Context ctx, int pid) {
        try {
            ActivityManager manager
                    = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
            if (manager != null) {
                List<ActivityManager.RunningAppProcessInfo> processInfoList
                        = manager.getRunningAppProcesses();
                if (processInfoList != null) {
                    for (ActivityManager.RunningAppProcessInfo processInfo : processInfoList) {
                        if (processInfo.pid == pid && !TextUtils.isEmpty(processInfo.processName)) {
                            return processInfo.processName;
                        }
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        //get from /proc/PID/cmdline
        BufferedReader br = null;
        try {
            br = new BufferedReader(new FileReader("/proc/" + pid + "/cmdline"));
            String processName = br.readLine();
            if (!TextUtils.isEmpty(processName)) {
                processName = processName.trim();
                if (!TextUtils.isEmpty(processName)) {
                    return processName;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                if (br != null) {
                    br.close();
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        //failed
        return null;
    }

    static String getLogHeader(Date startTime,
                               Date crashTime,
                               String crashType,
                               String appId,
                               String appVersion) {
        DateFormat timeFormatter = new SimpleDateFormat(TDConstants.timeFormatterStr, Locale.US);

        return TDConstants.sepHead + "\n"
                + "Tombstone maker: '" + TDConstants.fullVersion + "'\n"
                + "Crash type: '" + crashType + "'\n"
                + "Start time: '" + timeFormatter.format(startTime) + "'\n"
                + "Crash time: '" + timeFormatter.format(crashTime) + "'\n"
                + "App ID: '" + appId + "'\n"
                + "App version: '" + appVersion + "'\n"
                + "Rooted: '" + (Utils.isRoot() ? "Yes" : "No") + "'\n"
                + "API level: '" + Build.VERSION.SDK_INT + "'\n"
                + "OS version: '" + Build.VERSION.RELEASE + "'\n"
                + "ABI list: '" + Utils.getAbiList() + "'\n"
                + "Manufacturer: '" + Build.MANUFACTURER + "'\n"
                + "Brand: '" + Build.BRAND + "'\n"
                + "Model: '" + Utils.getMobileModel() + "'\n"
                + "Build fingerprint: '" + Build.FINGERPRINT + "'\n";
    }

    /**
     * Get SystemProperty.
     *
     * @param key          key
     * @param defaultValue defaultValue
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    public static String getSystemProperty(String key, String defaultValue) {
        try {
            Class<?> clz = Class.forName("android.os.SystemProperties");
            Method get = clz.getMethod("get", String.class, String.class);
            return (String) get.invoke(clz, key, defaultValue);
        } catch (NoSuchMethodException
                | IllegalAccessException
                | InvocationTargetException
                | ClassNotFoundException var4) {
            var4.printStackTrace();
        }

        return defaultValue;
    }

    public static boolean isMIUI() {
        String property = getSystemProperty("ro.miui.ui.version.name", "");
        return !TextUtils.isEmpty(property);
    }

    /**
     * Get MobileModel.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    public static String getMobileModel() {
        String mobileModel = null;
        if (isMIUI()) {
            String deviceName = "";

            try {
                Class<?> systemProperties = Class.forName("android.os.SystemProperties");
                Method get = systemProperties.getDeclaredMethod("get", String.class, String.class);
                deviceName = (String) get.invoke(systemProperties, "ro.product.marketname", "");
                if (TextUtils.isEmpty(deviceName)) {
                    deviceName = (String) get.invoke(systemProperties, "ro.product.model", "");
                }
            } catch (InvocationTargetException
                    | NoSuchMethodException
                    | IllegalAccessException
                    | ClassNotFoundException var3) {
                var3.printStackTrace();
            }

            mobileModel = deviceName;
        } else {
            mobileModel = Build.MODEL;
        }

        if (mobileModel == null) {
            mobileModel = "";
        }

        return mobileModel;
    }

    static boolean isRoot() {
        try {
            for (String path : suPathname) {
                File file = new File(path);
                if (file.exists()) {
                    return true;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    static String getAbiList() {
        if (android.os.Build.VERSION.SDK_INT >= 21) {
            return android.text.TextUtils.join(",", Build.SUPPORTED_ABIS);
        } else {
            String abi = Build.CPU_ABI;
            String abi2 = Build.CPU_ABI2;
            if (TextUtils.isEmpty(abi2)) {
                return abi;
            } else {
                return abi + "," + abi2;
            }
        }
    }

    static String getFileMD5(File file) {
        String s;

        if (!file.exists()) {
            return null;
        }

        FileInputStream in;
        byte[] buffer = new byte[1024];
        int len;

        try {
            MessageDigest md = MessageDigest.getInstance("MD5");
            in = new FileInputStream(file);
            while ((len = in.read(buffer, 0, 1024)) != -1) {
                md.update(buffer, 0, len);
            }
            in.close();
            BigInteger bigInt = new BigInteger(1, md.digest());
            s = String.format("%032x", bigInt);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        return s;
    }

    static String getAppVersion(Context ctx) {
        String appVersion = null;

        try {
            appVersion = ctx.getPackageManager()
                    .getPackageInfo(ctx.getPackageName(), 0).versionName;
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (TextUtils.isEmpty(appVersion)) {
            appVersion = "unknown";
        }

        return appVersion;
    }

    static boolean checkProcessAnrState(Context ctx, long timeoutMs) {
        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) {
            return false;
        }

        int pid = android.os.Process.myPid();
        long poll = timeoutMs / 500;
        for (int i = 0; i < poll; i++) {
            List<ActivityManager.ProcessErrorStateInfo> processErrorList
                    = am.getProcessesInErrorState();
            if (processErrorList != null) {
                for (ActivityManager.ProcessErrorStateInfo errorStateInfo : processErrorList) {
                    TDCrashLogger.error(TAG, "pid = " + errorStateInfo.pid + " condition  = " + errorStateInfo.condition);
                    if (errorStateInfo.pid
                            == pid && errorStateInfo.condition
                            == ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) {
                        return true;
                    }
                }
            }

            try {
                Thread.sleep(500);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        return false;
    }
}
