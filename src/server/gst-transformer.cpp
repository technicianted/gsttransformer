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

#include "serviceimpl.h"
#include "servercli.h"
#include "serviceparams.h"
using namespace gst_transformer::service;

void runServer(const std::string &endpoint, const ServiceParams &params)
{
    gst_transformer::service::ServiceImpl service(params);

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        std::cout << "Unable to build service" << std::endl;
        exit(2);
    }
    std::cout << "Server listening on " << endpoint << std::endl;
    server->Wait();
}

int main(int argc, char **argv)
{
    gst_init (&argc, &argv);

    if (parse_opt(argc, argv) == -1) {
        usage();
    }

    ServiceParams params;
    if (!configurationFile.empty()) {
        params.loadFromJsonFile(configurationFile);
    }

    for(unsigned int i=0; i<sizeof(spdlog::level::level_names); i++) {
        if (logLevel == spdlog::level::level_names[i]) {
            spdlog::set_level((spdlog::level::level_enum)i);
            break;
        }
    }

    runServer(endpoint, params);
}
