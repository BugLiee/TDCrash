package com.bugliee.tdcrashdemo;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;

import com.thinkingdata.tdcrash.ActivityMonitor;
import com.thinkingdata.tdcrash.NativeHandler;
import com.thinkingdata.tdcrash.TDCrash;
import com.thinkingdata.tdcrash.TDCrashLogger;

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

        ////

        ActivityMonitor.getInstance().initialize(this.getApplication());
        TDCrashLogger.setLog(true);
        TDCrash.getInstance().init(this.getApplicationContext(), new TDCrash.CheckListener() {
            @Override
            public boolean fileCallback(File needReportFile) {
                //do post
                return false;
            }
        });
        TDCrash.getInstance().initANRHandler();
        TDCrash.getInstance().initJavaCrashHandler(true);
        TDCrash.getInstance().initNativeCrashHandler(true, true, true, true);

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