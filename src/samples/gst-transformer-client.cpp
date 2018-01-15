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

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/client_context.h>

#include "client.h"
#include "clientcli.h"

int main(int argc, char **argv)
{
    if (parse_opt(argc, argv, true)) {
        usage();
        exit(1);
    }

    std::shared_ptr<spdlog::logger> logger = spdlog::stderr_logger_mt("client");
    spdlog::set_level(spdlog::level::debug);

    auto channel = ::grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
    transform(logger, channel, inputFileStream, outputFileStream, transformConfig);

    logger->info("all done, exiting.");
}
