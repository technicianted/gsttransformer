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

#ifndef __DYNAMICPIPELINE_H__
#define __DYNAMICPIPELINE_H__

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <glib.h>

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <spdlog/spdlog.h>

#include "pipelineparameters.h"
#include "pipeline.h"

class DynamicPipeline : public Pipeline
{
public:
     ~DynamicPipeline();

    void start(const std::function<int(const char *, int)> &consumer);
    void stop();

    int addData(const char *buffer, int size);
    void endData();
    void waitUntilCompleted();
    PipelineTerminationReason getTerminationReason() const;
    std::string getTerminationMessage() const;

    unsigned long getProcessedInputBytes() const;
    unsigned long getProcessedOutputBytes() const;
    double getProcessedTime() const;

    static DynamicPipeline * createFromSpecs(const PipelineParameters &parameters, const std::string &pipelineId, const std::string &specs);

private:
    static const std::string SOURCE_NAME;
    static const std::string SINK_NAME;
    static std::thread mainLoopThread;
    static GMainLoop *mainLoop;
    static std::mutex mainLoopLock;

    std::shared_ptr<spdlog::logger> logger;
    std::function<int(const char *, int)> consumer;
    PipelineParameters parameters;
    std::string pipelineId;
    GstElement *pipeline;
    GstBus *bus;
    GstAppSrc *source;
    GstAppSink *sink;
    PipelineTerminationReason terminationReason;
    std::string terminationMessage;
    std::chrono::steady_clock::time_point lastWriteTime;
    unsigned long totalBytesRead;
    unsigned long totalBytesWritten;
    gint64 processedTime;
    bool done;
    std::mutex doneMutex;
    std::condition_variable doneCond;

    DynamicPipeline(std::shared_ptr<spdlog::logger> &logger, const PipelineParameters &parameters, const std::string &pipelineId, GstElement *pipeline);
    void terminatePipeline(PipelineTerminationReason reason, const std::string &message, bool force = true);

    static gboolean gstBusMessage(GstBus * bus, GstMessage * message, gpointer user_data);
    static void gstEnoughData(GstElement * pipeline, guint size, gpointer user_data);
    static void gstNewSample(GstElement *sink, gpointer user_data);
    static gboolean gstTerminateIdleCallback(gpointer user_data);
};

#endif
