#include "serviceimpl.h"
#include "serverpipelinefactory.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace gst_transformer {
namespace service {

ServiceImpl::ServiceImpl()
{
    this->globalLogger = spdlog::stderr_logger_mt("serviceimpl");
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

    std::string requestId = iterator->second.data();
    // TODO: not ideal but convenient. global lock.
    auto logger = spdlog::stderr_logger_mt(fmt::format("serviceimpl.Transform/{0}", requestId));

    TransformRequest request;
    stream->Read(&request);
    if (!request.has_config()) {
        logger->notice("config message not sent");
        return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "config message not sent");
    }

    auto config = request.config();
    logger->debug("config: {0}", config.ShortDebugString());
    auto params = config.pipeline_parameters();
    std::unique_ptr<Pipeline> pipeline(factory.get(requestId, config));
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

}
}