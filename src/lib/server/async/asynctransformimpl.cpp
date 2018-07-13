#include "asynctransformimpl.h"

#include "../serverpipelinefactory.h"

#include <fmt/format.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace gst_transformer {
namespace service {

AsyncTransformImpl::AsyncTransformImpl(
    std::shared_ptr<spdlog::logger> &globalLogger,
    GRunLoop *runloop,
    GstTransformer::AsyncService *service,
    ::grpc::ServerCompletionQueue *completionQueue,
    const ServiceParametersStruct *params) 
    : responder(&this->serverContext), factory(*params)
{
    this->globalLogger = globalLogger;
    this->runloop = runloop;
    this->service = service;
    this->completionQueue = completionQueue;
    this->params = params;
    this->nextWriteCallback = nullptr;
    this->writeState = AsyncWriteState::Idle;
    this->logger = nullptr;
    this->setup();
}

AsyncTransformImpl::~AsyncTransformImpl()
{
    if (this->logger)
        this->logger->trace("AsyncTransormImpl destructor called");
    else
        this->globalLogger->trace("AsyncTransormImpl destructor called");
}

void AsyncTransformImpl::setup()
{
    this->readReady = false;
    this->writeReady = true;
    this->samplesAvailable = 0;
    this->writeBufferedSize = 0;
    this->terminating = false;
    this->eos = false;

    this->wrapperWriteCallback = [&] (bool ok) {
        this->writeCallback(ok);
    };

    this->configFunction = [&] (bool ok) {
        if (!ok) {
            this->globalLogger->debug("AsyncTransformImpl configFunction ok is false, quitting");
            delete this;
            return;
        }

        new AsyncTransformImpl(this->globalLogger, this->runloop, this->service, this->completionQueue, this->params);

        auto metadata = this->serverContext.client_metadata();
        auto iterator = metadata.find(ClientMetadata_Name(ClientMetadata::requestid));
        if (iterator == metadata.end()) {
            auto message = fmt::format("request ID not set: {0}", ClientMetadata_Name(ClientMetadata::requestid));
            this->globalLogger->error(message);

            this->responder.Finish(
                ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, message), 
                &this->finishFunction);
            return;
        }

        this->requestId = std::string(iterator->second.data(), iterator->second.size());
        this->globalLogger->debug("request ID {0}", requestId);
        // TODO: not ideal but convenient. global lock.
        this->logger = spdlog::stderr_logger_mt(fmt::format("asyncserviceimpl.Transform/{0}", requestId));

        this->responder.Read(&this->request, &this->startFunction);
    };

    this->startFunction = [&] (bool ok) {
        if (!this->request.has_config()) {
            this->logger->error("config message not sent");
            this->responder.Finish(
                ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "config message not sent"), 
                &this->finishFunction);
            return;
        }

        this->config = request.config();
        auto params = this->config.pipeline_parameters();
        logger->debug("request config {0}", this->config.ShortDebugString());
        try {
            this->validateConfig(this->config);
        }
        catch(std::exception &e) {
            auto message = fmt::format("invalid config: {0}", e.what());
            logger->error(message);
            this->responder.Finish(
                ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, message), 
                &this->finishFunction);
            return;
        }
        logger->debug("request config with limits applied {0}", this->config.ShortDebugString());

        try {
            this->pipeline = this->factory.get(requestId, this->config);
        }
        catch(std::exception &e) {
            auto message = fmt::format("cannot create pipeline: {0}", e.what());
            logger->error(message);
            this->responder.Finish(
                ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, message), 
                &this->finishFunction);
            return;
        }

        this->pipeline->setSampleAvailableCallback([&] () {
            this->runloop->execute([=] {
                this->logger->trace("sample callback invoked");
                this->samplesAvailable++;
                if (this->writeReady)
                    this->pullSample();
            });
        });

        this->pipeline->setEnoughDataCallback([&] {
            this->runloop->execute([=] {
                this->runloop->assertOnLoop();
                this->logger->trace("setting read ready to false");
                this->readReady = false;
            });
        });

        this->pipeline->setNeedDataCallback([&] () {
            this->runloop->execute([=] {
                if (!this->readReady) {
                    this->logger->trace("setting read ready to true");
                    this->readReady = true;
                    this->responder.Read(&this->request, &this->readDoneFunction);
                }
            });
        });

        this->pipeline->setEOSCallback([&] () {
            this->runloop->execute([=] {
                this->logger->debug("got EOS from pipeline");
                // flush buffered data if any
                this->eos = true;
                if (this->writeReady) {
                    this->logger->trace("EOS and writeReady, finalizing");
                    this->finalizeWrites();
                }
                else {
                    this->logger->trace("EOS and !writeReady");
                }
            });
        });

        this->pipeline->start(
            [&] (bool force) {
                if (this->terminating)
                    return;
                this->runloop->execute([=] {
                    this->logger->trace("error callback invoked, force: {0}", force);
                    if (force) {
                        this->terminating = true;
                        if (this->writeState < AsyncWriteState::WritingSummary) {
                            if (this->writeReady) {
                                this->logger->trace("writeReady, jumping to summaries");
                                this->summaryFunction(true);
                            }
                            else {
                                this->logger->trace("write not ready, setting next state to summaries");
                                this->nextWriteCallback = this->summaryFunction;
                            }
                        }
                    }
                });
            });

        // wait for need data callback to initiate read
    };

    this->readDoneFunction = [&] (bool ok) {
        this->runloop->execute([=] {
            this->logger->trace("read callback called ok: {0}", ok);
            if (ok) {
                if (!this->request.has_payload()) {
                    auto message = "no payload in request message";
                    logger->error(message);
                    this->responder.Finish(
                        ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, message), &this->finishFunction);
                }

                auto pipelineError = false;
                auto payloads = request.payload();
                for (int i=0; i<payloads.data_size(); i++) {
                    auto data = payloads.data(i);
                    if (pipeline->addData(data.data(), data.size()) == -1) {
                        logger->error("pipeline returned error adding data");
                        pipelineError = true;
                        break;
                    }
                }
                this->request.Clear();
                if (!pipelineError && readReady)
                    this->responder.Read(&request, &this->readDoneFunction);
            }
            else {
                logger->trace("ending data stream");
                pipeline->endData();
            }
        });
    };

    this->writeSampleDoneFunction = [&] (bool ok) {
        this->logger->trace("writeSample callback called, ok: {0}", ok);
        if (ok) {
            if (!this->eos) {
                if (this->samplesAvailable) {
                    this->logger->trace("writeSampleDoneFunction: sample ready, pulling {0} samples", this->samplesAvailable);
                    this->pullSample();
                }
                else {
                    this->logger->trace("writeSampleDoneFunction: no sample ready");
                }
            }
            else {
                this->logger->trace("writeSampleDoneFunction: finalizeWrites");
                this->finalizeWrites();
            }
            this->logger->trace("writeSampleDoneFunction: done");
        }
        else {
            this->logger->error("not ok in write");
        }
    };

    this->writeRemainderDoneFunction = [&] (bool ok) {
        this->logger->trace("writerRemainderDoneFunction: callback, ok: {0}", ok);
        if (ok) 
            this->summaryFunction(ok);
        else
            this->logger->error("writerRemainderDoneFunction: not ok");
    };

    this->summaryFunction = [&] (bool ok) {
        this->logger->trace("summaryFunction: callback, ok: {0}", ok);
        if (ok) {
            TransformResponse finalResponse;
            auto completion = finalResponse.mutable_transform_completed();
            completion->set_termination_reason((TerminationReason)this->pipeline->getTerminationReason());
            completion->set_termination_message(this->pipeline->getTerminationMessage());
            completion->set_processed_input_bytes(this->pipeline->getProcessedInputBytes());
            completion->set_processed_output_bytes(this->pipeline->getProcessedOutputBytes());
            completion->set_processed_time(this->pipeline->getProcessedTime());
            logger->trace("writing summary");
            this->write(finalResponse, AsyncWriteState::WritingSummary, this->finishSuccessFunction);
        }
        else {
            this->logger->error("unable to perform flush write");
        }
    };

    this->finishSuccessFunction = [&] (bool ok) {
        this->logger->trace("finishSuccessFunction: callback, ok: {0}", ok);
        this->responder.Finish(::grpc::Status::OK, &this->finishFunction);
    };

    this->finishFunction = [&] (bool ok) {
        this->logger->debug("call finished, ok: {0}", ok);
        delete this;
    };

    this->globalLogger->trace("RequestTransform");
    this->service->RequestTransform(
        &this->serverContext,
        &this->responder,
        this->completionQueue,
        this->completionQueue,
        &this->configFunction);    
}

void AsyncTransformImpl::pullSample()
{
    // called with writeReady and sampleReady
    if (this->terminating)
        return;

    auto samples = this->pipeline->getPendingSample(this->samplesAvailable);
    for(auto sample : samples) {
        this->response.mutable_payload()->add_data(std::string(sample));
        this->writeBufferedSize += sample.length();
    }
    if (this->writeBufferedSize > config.pipeline_output_buffer()) {
        this->write(this->response, AsyncWriteState::WritingSamples, this->writeSampleDoneFunction);
        this->response.Clear();
        this->writeBufferedSize = 0;
    }

    this->samplesAvailable = 0;
}

void AsyncTransformImpl::finalizeWrites()
{
    this->runloop->assertOnLoop();

    // called with writeReady
    if (this->terminating)
        return;

    if (this->response.payload().data_size() > 0) {
        logger->debug("flushing {0} buffers to client", this->response.payload().data_size());
        this->write(this->response, AsyncWriteState::WritingSamplesRemainder, this->writeRemainderDoneFunction);
        this->response.Clear();
        this->writeBufferedSize = 0;
    }
    else {
        this->summaryFunction(true);
    }
}

void AsyncTransformImpl::write(const TransformResponse &m, AsyncWriteState writeState, const std::function<void(bool)> &nextCallback)
{
    assert(!this->nextWriteCallback);
    assert(this->writeReady);
    this->runloop->assertOnLoop();

    this->writeState = writeState;
    this->nextWriteCallback = nextCallback;
    this->responder.Write(m, &this->wrapperWriteCallback);
    this->writeReady = false;
}

void AsyncTransformImpl::writeCallback(bool ok)
{
    this->runloop->execute([=] {
        assert(!this->writeReady);

        this->writeReady = true;
        auto callback = this->nextWriteCallback;
        this->nextWriteCallback = nullptr;
        if (callback)
            callback(ok);
    });
}

void AsyncTransformImpl::validateConfig(TransformConfig &transformConfig)
{
    auto pipelineParams = transformConfig.pipeline_parameters();

    if (!transformConfig.pipeline().empty() && !this->params->allow_dynamic_pipelines())
        throw std::invalid_argument("dynamic pipelines in requests are disabled");
    
    if (this->params->max_rate() != 0 && this->params->max_rate() != -1) {
        if (pipelineParams.rate() > this->params->max_rate() || pipelineParams.rate() == -1)
            throw std::invalid_argument(
                fmt::format("requested rate {0} exceeds allowed max rate {1}", 
                pipelineParams.rate(), 
                this->params->max_rate()));
    }
    if (this->params->max_length_millis() != 0) {
        if (pipelineParams.length_limit_milliseconds() > this->params->max_length_millis())
            throw std::invalid_argument(
                fmt::format("requested length limit {0} exceeds allowed max {1}",
                pipelineParams.length_limit_milliseconds(),
                this->params->max_length_millis()));
        transformConfig.mutable_pipeline_parameters()->set_length_limit_milliseconds(this->params->max_length_millis());
    }
    if (this->params->max_start_tolerance_bytes() != 0) {
        if (pipelineParams.start_tolerance_bytes() > this->params->max_start_tolerance_bytes())
            throw std::invalid_argument(
                fmt::format("requested start tolerance bytes {0} exceeds allowed max {1}",
                pipelineParams.start_tolerance_bytes(),
                this->params->max_start_tolerance_bytes()));
        transformConfig.mutable_pipeline_parameters()->set_start_tolerance_bytes(this->params->max_start_tolerance_bytes());
    }
    if (this->params->max_read_timeout_millis() != 0) {
        if (pipelineParams.read_timeout_milliseconds() > this->params->max_read_timeout_millis())
            throw std::invalid_argument(
                fmt::format("requested read timeout {0} exceeds allowed max {1}",
                pipelineParams.read_timeout_milliseconds(),
                this->params->max_read_timeout_millis()));
        transformConfig.mutable_pipeline_parameters()->set_read_timeout_milliseconds(this->params->max_read_timeout_millis());
    }
    if (this->params->max_pipeline_output_buffer() != 0) {
        if (transformConfig.pipeline_output_buffer() > this->params->max_pipeline_output_buffer()) 
            throw std::invalid_argument(
                fmt::format("requested pipeline output buffer {0} exceeds allowed max {1}",
                transformConfig.pipeline_output_buffer(),
                this->params->max_pipeline_output_buffer()));
    }
}

}
}
