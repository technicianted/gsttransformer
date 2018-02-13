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

#ifndef __ASYNCTRANSFORIMPL_H__
#define __ASYNCTRANSFORIMPL_H__

#include <grpc++/grpc++.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <functional>

#include "serviceparameters.pb.h"
#include "gsttransformer.grpc.pb.h"
#include "asyncserviceimpl.h"
#include "../serverpipelinefactory.h"
#include "../grunloop.h"

namespace gst_transformer {
namespace service {

enum class AsyncWriteState {
    Idle,
    WritingSamples,
    WritingSamplesRemainder,
    WritingSummary,
    WritingFinished
};

class AsyncTransformImpl
{
public:
    AsyncTransformImpl(
        std::shared_ptr<spdlog::logger> &globalLogger,
        GRunLoop *runloop,
        GstTransformer::AsyncService *service,
        ::grpc::ServerCompletionQueue *completionQueue,
        const ServiceParametersStruct *params);
    ~AsyncTransformImpl();

private:
    std::shared_ptr<spdlog::logger> globalLogger;
    std::shared_ptr<spdlog::logger> logger;
    std::string requestId;

    GstTransformer::AsyncService *service;
    ::grpc::ServerCompletionQueue *completionQueue;
    const ServiceParametersStruct *params;
    GRunLoop *runloop;

    ::grpc::ServerContext serverContext;
    ::grpc::ServerAsyncReaderWriter<TransformResponse, TransformRequest> responder;

    std::function<void(bool)> configFunction;
    std::function<void(bool)> startFunction;
    std::function<void(bool)> writeRemainderDoneFunction;
    std::function<void(bool)> summaryFunction;
    std::function<void(bool)> finishSuccessFunction;
    std::function<void(bool)> finishFunction;

    std::function<void(bool)> wrapperWriteCallback;
    std::function<void(bool)> nextWriteCallback;

    ServerPipelineFactory factory;
    TransformRequest request;
    bool readReady;
    std::function<void(bool)> readDoneFunction;
    
    bool writeReady;
    int samplesAvailable;
    TransformResponse response;
    unsigned int writeBufferedSize;
    std::function<void(bool)> writeSampleDoneFunction;
    bool terminating;
    bool eos;
    AsyncWriteState writeState;

    std::unique_ptr<Pipeline> pipeline;
    TransformConfig config;

    void setup();
    void finalizeWrites();
    void pullSample();
    void write(const TransformResponse &m, AsyncWriteState writeState, const std::function<void(bool)> &nextCallback);
    void writeCallback(bool ok);
    void validateConfig(TransformConfig &transformConfig);
};

}
}

#endif
