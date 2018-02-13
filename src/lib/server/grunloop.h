/*

Copyright 2018 technicianted

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

*/

#ifndef __GRUNLOOP_H__
#define __GRUNLOOP_H__

#include <functional>
#include <thread>
#include <mutex>
#include <glib.h>

/**
 * Thin wrapper around GLib runloop. Used to track threads and thread execution
 * for callbacks in/out of gstreamer.
 */
class GRunLoop
{
public:
    /**
     * Constructs a new runloop running on a new thread.
     */
    GRunLoop();

    /**
     * Start the new runloop.
     */
    void start();
    /**
     * Stop the runloop.
     */
    void stop();

    /**
     * Executes a function on the runloop thread.
     * 
     * If call is made from the runloop thread, then function is called
     * immediately.
     * 
     * \param func function to call.
     * \return true if execution has been successfully scheduled.
     */
    bool execute(const std::function<void()> &func);

    /**
     * Asserts execution on the runloop thread.
     */
    void assertOnLoop() const;
    /**
     * Check if the execution is on the runloop thread.
     * \return true if execution is on runloop thread.
     */
    bool isOnLoop() const;

    /**
     * Returns singleton for the default runloop.
     * 
     * \return Default runloop.
     */
    static GRunLoop * main();

private:
    GMainLoop *loop;
    GMainContext *context;
    std::thread thread;
    std::thread::id threadId;

    GRunLoop(bool isDefault);

    static GRunLoop * Main;
    static std::mutex MainLock;

    static gboolean _execute(gpointer user_data);
};

#endif
