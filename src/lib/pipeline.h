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

#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include <functional>

#include "pipelineparameters.h"

// Reasons for pipeline termination
enum class PipelineTerminationReason
{
    // no reason or unknown.
    NONE,
    // an internal pipeline execution error has occured.
    INTERNAL_ERROR,
    // normal pipeline temrination when stream ended.
    END_OF_STREAM,
    // media format has not been detected and the pipeline did not start.
    FORMAT_NOT_DETECTED,
    // maximum media duration has been reached.
    ALLOWED_DURATION_EXCEEDED,
    // maximum media rate has been exceeded and enforcement policy is ERROR.
    RATE_EXCEEDED,
    // no input payload was given in specified time window.
    READ_TIMEOUT,
    // pipeline did not start in the required time.
    STREAM_START_TIMEOUT,
    // execution was cancelled.
    CANCELLED
};

/**
 * Abstract class for all pipeline implementation.
 */
class Pipeline
{
public:
    ~Pipeline() {};

    /**
     * Start the pipeline.
     * 
     * \param consumer lambda function to be called when the pipeline outputs data.
     */
    virtual void start(const std::function<int(const char *, int)> &consumer) = 0;
    /**
     * Immediately stops the pipeline.
     */
    virtual void stop() = 0;

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
    virtual int addData(const char *buffer, int size) = 0;
    /**
     * Indicates that last data has been added and the pipeline
     * may finish processing and stop.
     */
    virtual void endData() = 0;
    /**
     * Block the caller until the pipeline has finished processing
     * all the buffers and has completely stopped.
     */
    virtual void waitUntilCompleted() = 0;
    /**
     * Obtain the termination reason for the pipeline.
     * This method is useful when addData() returned an error.
     * 
     * \return pipeline termination reason.
     */
    virtual PipelineTerminationReason getTerminationReason() const = 0;
    /**
     * Obtain the termination reason message.
     * 
     * \return termination reason message.
     */
    virtual std::string getTerminationMessage() const = 0;

    /**
     * Get how many bytes have been processed by the pipeline.
     * 
     * \notice current this number is not accurate as it indicates number 
     * of bytes buffered as well that may have not been processed yet.
     * \return number of processed bytes.
     */
    virtual unsigned long getProcessedInputBytes() const = 0;
    /**
     * Get how many bytes have been output by the pipeline.
     * 
     * \return number of output bytes.
     */
    virtual unsigned long getProcessedOutputBytes() const = 0;
    /**
     * Get duration in seconds of media stream time that have been processed.
     * 
     * \return seconds of stream media time processed.
     */
    virtual double getProcessedTime() const = 0;
};

#endif
