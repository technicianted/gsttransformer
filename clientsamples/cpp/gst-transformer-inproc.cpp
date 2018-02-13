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

#include <gst/gst.h>
#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/server.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/client_context.h>
#include <grpc++/server_builder.h>
#include <spdlog/spdlog.h>

#include "async/asyncserviceimpl.h"
#include "client.h"
#include "clientcli.h"

GST_DEBUG_CATEGORY(appsrc_pipeline_debug);
#define GST_CAT_DEFAULT appsrc_pipeline_debug

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::debug);

    if (parse_opt(argc, argv, false)) {
        usage();
        exit(1);
    }

    gst_init (&argc, &argv);
    GST_DEBUG_CATEGORY_INIT(appsrc_pipeline_debug, "appsrc-pipeline", 0, "gst-transformer");

    std::shared_ptr<spdlog::logger> logger = spdlog::stderr_logger_mt("inproc");
    spdlog::set_level(spdlog::level::trace);

    // default parameters = unlimited
    ServiceParametersStruct params;
    params.set_allow_dynamic_pipelines(true);

    GstTransformer::AsyncService service;
    ::grpc::ServerBuilder builder;
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::ServerCompletionQueue> completionQueue = builder.AddCompletionQueue();
    auto server = builder.BuildAndStart();
    auto channel = server->InProcessChannel(::grpc::ChannelArguments());
    AsyncServiceImpl asyncService(&service, completionQueue.get(), params);
    
    std::thread serviceThread([&] {
        asyncService.start();
    });

    transform(logger, channel, inputFileStream, outputFileStream, transformConfig);

    logger->info("all done, exiting.");

    server->Shutdown();
    logger->info("server shutdown");
    asyncService.stop();
    logger->info("async service shutdown");
    serviceThread.join();
}
