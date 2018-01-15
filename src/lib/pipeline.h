#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include <functional>

#include "pipelineparameters.h"

enum class PipelineTerminationReason
{
    NONE,
    INTERNAL_ERROR,
    END_OF_STREAM,
    FORMAT_NOT_DETECTED,
    ALLOWED_DURATION_EXCEEDED,
    RATE_EXCEEDED,
    READ_TIMEOUT,
    STREAM_START_TIMEOUT,
    CANCELLED
};

class Pipeline
{
public:
    ~Pipeline() {};

    virtual void start(const std::function<int(const char *, int)> &consumer) = 0;
    virtual void stop() = 0;

    virtual int addData(const char *buffer, int size) = 0;
    virtual void endData() = 0;
    virtual void waitUntilCompleted() = 0;

    virtual PipelineTerminationReason getTerminationReason() const = 0;
    virtual std::string getTerminationMessage() const = 0;

    virtual unsigned long getProcessedInputBytes() const = 0;
    virtual unsigned long getProcessedOutputBytes() const = 0;
    virtual double getProcessedTime() const = 0;
};

#endif
