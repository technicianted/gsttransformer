#include "serviceimpl.h"
#include "serverpipelinefactory.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace gst_transformer {
namespace service {

ServiceImpl::ServiceImpl(const ServiceParametersStruct &params) : factory(params)
{
    this->globalLogger = spdlog::stderr_logger_mt("serviceimpl");
    this->params = params;
    this->globalLogger->info("starting service with params:\n{0}", params.DebugString());
}

::grpc::Status ServiceImpl::Transform(
    ::grpc::ServerContext* context, 
    ::grpc::ServerReaderWriter< ::gst_transformer::service::TransformResponse, ::gst_transformer::service::TransformRequest>* stream)
{
    auto metadata = context->client_metadata();
    auto iterator = metadata.find(ClientMetadata_Name(ClientMetadata::requestid));
    if (iterator == metadata.end()) {
        auto message = fmt::format("request ID not set: {0}", ClientMetadata_Name(ClientMetadata::requestid));
        this->globalLogger->notice(message);
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, message);
    }

    std::string requestId = std::string(iterator->second.data(), iterator->second.size());
    this->globalLogger->debug("request ID {0}", requestId);
    // TODO: not ideal but convenient. global lock.
    auto logger = spdlog::stderr_logger_mt(fmt::format("serviceimpl.Transform/{0}", requestId));

    TransformRequest request;
    stream->Read(&request);
    if (!request.has_config()) {
        logger->notice("config message not sent");
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "config message not sent");
    }

    auto config = request.config();
    auto params = config.pipeline_parameters();
    logger->debug("request config {0}", config.ShortDebugString());
    try {
    this->validateConfig(config);
    }
    catch(std::exception &e) {
        auto message = fmt::format("invalid config: {0}", e.what());
        logger->notice(message);
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, message);
    }
    logger->debug("request config with limits applied {0}", config.ShortDebugString());

    std::shared_ptr<Pipeline> pipeline;
    try {
        pipeline.reset(factory.get(requestId, config));
    }
    catch(std::exception &e) {
        auto message = fmt::format("cannot create pipeline: {0}", e.what());
        logger->notice(message);
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, message);
    }

    TransformResponse response;
    unsigned int bufferedSize = 0;
    int totalWrites = 0;
    int totalWritten = 0;
    pipeline->start(
        [stream, &response, &config, &bufferedSize, &totalWrites, &totalWritten] (const char *buffer, int size) -> int {
            response.mutable_payload()->add_data(std::string(buffer, size));
            bufferedSize += size;
            if (bufferedSize > config.pipeline_output_buffer()) {
                stream->Write(response);
                response.clear_payload();
                bufferedSize = 0;
                totalWrites++;
            }
            totalWritten += size;
            return size;
        });

    bool pipelineError = false;
    while(!pipelineError && stream->Read(&request)) {
        if (!request.has_payload()) {
            logger->notice("no payload in request message");
            return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "payload message not sent");
        }

        auto payloads = request.payload();
        for (int i=0; i<payloads.data_size(); i++) {
            auto data = payloads.data(i);
            if (pipeline->addData(data.data(), data.size()) == -1) {
                logger->notice("pipeline returned error adding data");
                pipelineError = true;
                break;
            }
        }
    }

    logger->trace("ending data stream");
    pipeline->endData();
    logger->trace("waiting for pipeline to complete");
    pipeline->waitUntilCompleted();
 
    if (!pipelineError) {
        // flush buffered data if any
        if (response.payload().data_size() > 0) {
            logger->debug("flushing {0} to client", response.payload().data_size());
            stream->Write(response);
        }
    }

    logger->debug("totalWritten {0}, writes {1}", totalWritten, totalWrites);

    TransformResponse finalResponse;
    auto completion = finalResponse.mutable_transform_completed();
    completion->set_termination_reason((TerminationReason)pipeline->getTerminationReason());
    completion->set_termination_message(pipeline->getTerminationMessage());
    completion->set_processed_input_bytes(pipeline->getProcessedInputBytes());
    completion->set_processed_output_bytes(pipeline->getProcessedOutputBytes());
    completion->set_processed_time(pipeline->getProcessedTime());
    stream->Write(finalResponse);

    logger->debug("call done: {0}", completion->ShortDebugString());
    return ::grpc::Status::OK;
}

::grpc::Status ServiceImpl::TransformProducer(
    ::grpc::ServerContext* context, 
    ::grpc::ServerReader< ::gst_transformer::service::TransformRequest>* reader, 
    ::gst_transformer::service::TransformProducerResponse* response)
{
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

::grpc::Status ServiceImpl::TransformConsumer(
    ::grpc::ServerContext* context, 
    const ::gst_transformer::service::TransformConsumerRequest* request, 
    ::grpc::ServerWriter< ::gst_transformer::service::TransformResponse>* writer)
{
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

void ServiceImpl::validateConfig(TransformConfig &transformConfig)
{
    auto pipelineParams = transformConfig.pipeline_parameters();

    if (!transformConfig.pipeline().empty() && !this->params.allow_dynamic_pipelines())
        throw std::invalid_argument("dynamic pipelines in requests are disabled");
    
    if (this->params.max_rate() != 0 && this->params.max_rate() != -1) {
        if (pipelineParams.rate() > this->params.max_rate() || pipelineParams.rate() == -1)
            throw std::invalid_argument(
                fmt::format("requested rate {0} exceeds allowed max rate {1}", 
                pipelineParams.rate(), 
                this->params.max_rate()));
    }
    if (this->params.max_length_millis() != 0) {
        if (pipelineParams.length_limit_milliseconds() > this->params.max_length_millis())
            throw std::invalid_argument(
                fmt::format("requested length limit {0} exceeds allowed max {1}",
                pipelineParams.length_limit_milliseconds(),
                this->params.max_length_millis()));
        transformConfig.mutable_pipeline_parameters()->set_length_limit_milliseconds(this->params.max_length_millis());
    }
    if (this->params.max_start_tolerance_bytes() != 0) {
        if (pipelineParams.start_tolerance_bytes() > this->params.max_start_tolerance_bytes())
            throw std::invalid_argument(
                fmt::format("requested start tolerance bytes {0} exceeds allowed max {1}",
                pipelineParams.start_tolerance_bytes(),
                this->params.max_start_tolerance_bytes()));
        transformConfig.mutable_pipeline_parameters()->set_start_tolerance_bytes(this->params.max_start_tolerance_bytes());
    }
    if (this->params.max_read_timeout_millis() != 0) {
        if (pipelineParams.read_timeout_milliseconds() > this->params.max_read_timeout_millis())
            throw std::invalid_argument(
                fmt::format("requested read timeout {0} exceeds allowed max {1}",
                pipelineParams.read_timeout_milliseconds(),
                this->params.max_read_timeout_millis()));
        transformConfig.mutable_pipeline_parameters()->set_read_timeout_milliseconds(this->params.max_read_timeout_millis());
    }
    if (this->params.max_pipeline_output_buffer() != 0) {
        if (transformConfig.pipeline_output_buffer() > this->params.max_pipeline_output_buffer()) 
            throw std::invalid_argument(
                fmt::format("requested pipeline output buffer {0} exceeds allowed max {1}",
                transformConfig.pipeline_output_buffer(),
                this->params.max_pipeline_output_buffer()));
    }
}

}
}