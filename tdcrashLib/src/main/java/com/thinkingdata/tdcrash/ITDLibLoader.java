/*
 * Copyright (C) 2022 ThinkingData
 */

package com.thinkingdata.tdcrash;

/**
 * Define the native library loader.
 *
 * <p>In practice, older versions of Android had bugs in PackageManager
 * that caused installation and update of native libraries to be unreliable.
 */
public interface ITDLibLoader {

    /**
     * Loads the native library specified by the libName argument.
     *
     * @param libName the name of the library.
     */
    void loadLibrary(String libName);
}
