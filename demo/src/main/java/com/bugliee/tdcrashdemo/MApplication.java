package com.bugliee.tdcrashdemo;

import android.app.Application;

import com.thinkingdata.tdcrash.ActivityMonitor;


public class MApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        ActivityMonitor.getInstance().initialize(this);
    }
}
