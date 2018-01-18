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

#ifndef __SERVICEIMPL_H__
#define __SERVICEIMPL_H__

#include <spdlog/spdlog.h>

#include "gsttransformer.grpc.pb.h"
#include "serviceparameters.pb.h"
#include "serverpipelinefactory.h"

namespace gst_transformer {
namespace service {

/**
 * Concrete implementation of the service.
 * 
 * This implementation uses sync gRPC.
 */
class ServiceImpl : public GstTransformer::Service
{
public:
    /**
     * Construct a new instance with service parameters.
     * 
     * \param param service parameters used to define this RPC.
     */
    ServiceImpl(const ServiceParametersStruct &params);
    
    ::grpc::Status Transform(
        ::grpc::ServerContext* context, 
        ::grpc::ServerReaderWriter< ::gst_transformer::service::TransformResponse, 
        ::gst_transformer::service::TransformRequest>* stream);
    ::grpc::Status TransformProducer(
        ::grpc::ServerContext* context, 
        ::grpc::ServerReader< ::gst_transformer::service::TransformRequest>* reader, 
        ::gst_transformer::service::TransformProducerResponse* response);
    ::grpc::Status TransformConsumer(
        ::grpc::ServerContext* context, 
        const ::gst_transformer::service::TransformConsumerRequest* request, 
        ::grpc::ServerWriter< ::gst_transformer::service::TransformResponse>* writer);
private:
    std::shared_ptr<spdlog::logger> globalLogger;
    ServerPipelineFactory factory;
    ServiceParametersStruct params;

    void validateConfig(TransformConfig &transformConfig);
};

}
}

#endif
