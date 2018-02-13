#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <fstream>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <uuid/uuid.h>

#include "client.h"
#include "clientcli.h"
#include "gsttransformer.grpc.pb.h"

using namespace gst_transformer::service;

static std::string generateRequestId()
{
    uuid_t requestIdUuid;
    uuid_generate(requestIdUuid);
    char buffer[64];
    uuid_unparse(requestIdUuid, buffer);
    return std::string(buffer);
}

void transform(
    std::shared_ptr<spdlog::logger> &logger,
    std::shared_ptr<::grpc::Channel> &channel, 
    std::ifstream &inf, 
    std::ofstream &of, 
    gst_transformer::service::TransformConfig &config)
{
    std::random_device randomDevice;
    std::default_random_engine randomSource(randomDevice());
    std::uniform_int_distribution<int> randomGenerator(0, writeDelay);

    auto requestId = generateRequestId();
    logger->info("starting request ID {0}", requestId);
    
    auto client = GstTransformer::NewStub(channel);
    ::grpc::ClientContext context;
    context.AddMetadata(
        gst_transformer::service::ClientMetadata_Name(ClientMetadata::requestid),
        requestId);
    auto requestStream = client->Transform(&context);

    TransformRequest request;
    request.mutable_config()->CopyFrom(config);
    requestStream->Write(request);

    unsigned int readCount = 0;
    bool readStreamClosed = false;
    TransformCompleted transformCompleted;
    std::thread readerThread([&requestStream, &readCount, &readStreamClosed, &transformCompleted, &of]{
        TransformResponse response;
        while(requestStream->Read(&response)) {
            if (response.has_payload()) {
                auto payloads = response.payload();
                for(int i=0; i<payloads.data_size(); i++) {
                    auto data = payloads.data(i);
                    of.write(data.data(), data.size());
                }
                readCount++;
            }
            else if (response.has_transform_completed()) {
                transformCompleted.CopyFrom(response.transform_completed());
                // loop should break now since this is always the last message
            }
        }
        readStreamClosed = true;
    });

    int totalWrite = 0;
    bool writeStreamClosed = false;
    while(!readStreamClosed && !writeStreamClosed && !inf.eof()) {
        request.clear_payload();
        char buffer[4096];
        inf.read(buffer, sizeof(buffer));
        auto payload = request.mutable_payload();
        payload->add_data(std::string(buffer, inf.gcount()));
        writeStreamClosed = !requestStream->Write(request);
        totalWrite += inf.gcount();
        logger->trace("written {0}, {1} so far", inf.gcount(), totalWrite);
        if (writeDelay > 0) {
            auto sleepTime = writeDelay;
            if (randomizeWriteDelay) {
                sleepTime = randomGenerator(randomDevice);
            }
            logger->trace("sleeping for {0}ms", sleepTime);
            usleep(sleepTime * 1000);
        }
    }
    
    requestStream->WritesDone();
    readerThread.join();
    logger->debug("finished reading, count {0}", readCount);

    ::grpc::Status status = requestStream->Finish();
    logger->info("status: {0} - '{1}', completion:\n{2}", status.error_code(), status.error_message(), transformCompleted.DebugString());
}
