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
#include <unistd.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <fstream>
#include <thread>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "servercli.h"
#include "serviceparams.h"
#include "server/async/asyncserviceimpl.h"

using namespace gst_transformer::service;

void runAsyncServer(const std::string &endpoint, const ServiceParams &params)
{
    GstTransformer::AsyncService service;
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::ServerCompletionQueue> completionQueue = builder.AddCompletionQueue();
    auto server = builder.BuildAndStart();

    AsyncServiceImpl asyncService(&service, completionQueue.get(), params);
    std::cout << "Async server listening on " << endpoint << std::endl;
    asyncService.start();
}

int main(int argc, char **argv)
{
    spdlog::set_pattern("%+ %t");

    gst_init (&argc, &argv);

    if (parse_opt(argc, argv) == -1) {
        usage();
    }

    gst_transformer::service::ServiceParams params;
    if (!configurationFile.empty()) {
        params.loadFromJsonFile(configurationFile);
    }

    for(unsigned int i=0; i<sizeof(spdlog::level::level_names); i++) {
        if (logLevel == spdlog::level::level_names[i]) {
            spdlog::set_level((spdlog::level::level_enum)i);
            break;
        }
    }

    runAsyncServer(endpoint, params);
}
