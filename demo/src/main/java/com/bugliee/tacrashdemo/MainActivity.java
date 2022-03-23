package com.bugliee.tacrashdemo;

import static android.os.FileObserver.CLOSE_WRITE;

import android.app.Activity;
import android.os.Bundle;
import android.os.FileObserver;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;

import cn.thinkingdata.android.crash.CrashLogListener;
import cn.thinkingdata.android.crash.NativeHandler;
import cn.thinkingdata.android.crash.TACrash;
import cn.thinkingdata.android.crash.TACrashLogger;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        this.findViewById(R.id.btn_crash).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                throw new RuntimeException("this is a test RuntimeException!");
            }
        });
        this.findViewById(R.id.btn_anr).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                SystemClock.sleep(20000);
            }
        });
        this.findViewById(R.id.btn_native).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                NativeHandler.getInstance().testNativeCrash(0);
            }
        });

        TACrash.getInstance().init(this)
                .enableLog()
                .initJavaCrashHandler(true)
                .initANRHandler()
                .initNativeCrashHandler(true, true, true, true)
                .initCrashLogListener(new CrashLogListener() {
                    @Override
                    public void onFile(File logFile) {
                        //数据入库并删除文件
                    }
                });
        String path = this.getCacheDir().getAbsolutePath() + File.separator + "tacrash";
        checkFile(path);
        //数据入库并删除文件
    }

    private void checkFile(String path) {
        Runtime.getRuntime().addShutdownHook(new Thread(){
            @Override
            public void run() {
                File file = new File(path);
                File[] list = file.listFiles();
                if (list != null) {
                    for (File f : list) {
                        Log.d("bugliee", " f path " + f.getAbsolutePath());
                    }
                }
            }
        });
        Log.d("bugliee", "checkFile == >");
        FileObserver observer = new FileObserver(new File(path), CLOSE_WRITE) {
            @Override
            public void onEvent(int event, String path) {
                Log.d("bugliee", "event " + event + " path " + path);
            }
        };
        Log.d("bugliee", "checkFile 2== >");
        try {
            observer.startWatching();
        } catch (Exception e) {
            observer = null;
            TACrashLogger.error("bugliee",
                    "fileObserver startWatching failed", e);
        }
        Log.d("bugliee", "checkFile 3== >");

        /*File logF = new File(config.logDir);
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
        }*/
    }
    
    public static String readFileContent(File file) {
//        File file = new File(fileName);
        BufferedReader reader = null;
        StringBuffer sbf = new StringBuffer();
        try {
            reader = new BufferedReader(new FileReader(file));
            String tempStr;
            while ((tempStr = reader.readLine()) != null) {
                sbf.append(tempStr);
                sbf.append("\n");
            }
            reader.close();
            return sbf.toString();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e1) {
                    e1.printStackTrace();
                }
            }
        }
        return sbf.toString();
    }
}