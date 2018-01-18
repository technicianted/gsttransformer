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

/**
 * An implementation of a media pipeline that uses gst launch syntax for pipeline
 * specs.
 * 
 * It works by prepending an appsrc and appending an appsink to drive the pipeline.
 * 
 * \notice this type of pipelines can only be used once.
 */
class DynamicPipeline : public Pipeline
{
public:
     ~DynamicPipeline();

    /**
     * Start the pipeline.
     * 
     * \param consumer lambda function to be called when the pipeline outputs data.
     */
    void start(const std::function<int(const char *, int)> &consumer);
    /**
     * Immediately stops the pipeline.
     */
    void stop();

    /**
     * Enqueues data to be processed by the pipeline.
     * Depending on the rate enforcement policy, this method may
     * either block until there is room for data, or return an error
     * if the policy is set to ERROR.
     * 
     * \param buffer input data buffer.
     * \param size input buffer size.
     * \return number of bytes copied, -1 on error.
     */
    int addData(const char *buffer, int size);
    /**
     * Indicates that last data has been added and the pipeline
     * may finish processing and stop.
     */
    void endData();
    /**
     * Block the caller until the pipeline has finished processing
     * all the buffers and has completely stopped.
     */
    void waitUntilCompleted();
    /**
     * Obtain the termination reason for the pipeline.
     * This method is useful when addData() returned an error.
     * 
     * \return pipeline termination reason.
     */
    PipelineTerminationReason getTerminationReason() const;
    /**
     * Obtain the termination reason message.
     * 
     * \return termination reason message.
     */
    std::string getTerminationMessage() const;

    /**
     * Get how many bytes have been processed by the pipeline.
     * 
     * \notice current this number is not accurate as it indicates number 
     * of bytes buffered as well that may have not been processed yet.
     * \return number of processed bytes.
     */
    unsigned long getProcessedInputBytes() const;
    /**
     * Get how many bytes have been output by the pipeline.
     * 
     * \return number of output bytes.
     */
    unsigned long getProcessedOutputBytes() const;
    /**
     * Get duration in seconds of media stream time that have been processed.
     * 
     * \return seconds of stream media time processed.
     */
    double getProcessedTime() const;

    /**
     * Create a new pipeline instances from gst specs.
     * 
     * \param parameters pipeline execution parameters.
     * \param pipelineId pipeline identification for logging.
     * \param specs gst pipeline specs.
     * \return a new pipeline instance.
     */
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
