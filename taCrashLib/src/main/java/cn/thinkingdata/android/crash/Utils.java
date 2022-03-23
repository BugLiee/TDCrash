/*
 * Copyright (C) 2022 ThinkingData
 */

package cn.thinkingdata.android.crash;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build;
import android.text.TextUtils;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
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
    private static final String TAG = TAConstants.TAG + ".Utils";

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
                TACrashLogger.error(TAG,
                        "createFile by createNewFile failed, file already exists");
                return null;
            }
        } catch (Exception e) {
            TACrashLogger.error(TAG,
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
            TACrashLogger.error(TAG, "Utils getInfo(" + pathname + ") failed", e);
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
        DateFormat timeFormatter = new SimpleDateFormat(TAConstants.timeFormatterStr, Locale.US);

        return TAConstants.sepHead + "\n"
                + "Tombstone maker: '" + TAConstants.fullVersion + "'\n"
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

    /**
     * < getActivityFromContext >.
     *
     * @author bugliee
     * @create 2022/3/9
     * @param context context
     * @return {@link Activity}
     */
    public static Activity getActivityFromContext(Context context) {
        Activity activity = null;
        try {
            if (context != null) {
                if (context instanceof Activity) {
                    activity = (Activity) context;
                } else if (context instanceof ContextWrapper) {
                    while (!(context instanceof Activity) && context instanceof ContextWrapper) {
                        context = ((ContextWrapper) context).getBaseContext();
                    }
                    if (context instanceof Activity) {
                        activity = (Activity) context;
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return activity;
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
                    TACrashLogger.error(TAG, "pid = " + errorStateInfo.pid
                            + " condition  = " + errorStateInfo.condition);
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

    /**
     * 判断当前应用是否在前台.
     * */
    public static boolean isForeground(Context context) {
        ActivityManager activityManager
                = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> appProcesses
                = activityManager.getRunningAppProcesses();
        String processName = "";
        for (ActivityManager.RunningAppProcessInfo appProcess : appProcesses) {
            processName = appProcess.processName;
            int p = processName.indexOf(":");
            if (p != -1) {
                processName = processName.substring(0, p);
            }
            if (processName.equals(context.getPackageName())) {
                return appProcess.importance
                        == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
                        || appProcess.importance
                        == ActivityManager.RunningAppProcessInfo.IMPORTANCE_VISIBLE;
            }
        }
        return false;
    }
}
