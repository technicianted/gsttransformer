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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <fstream>
#include <thread>
#include <iostream>
#include <spdlog/spdlog.h>

#include "gsttransformer.grpc.pb.h"

void transform(
    std::shared_ptr<spdlog::logger> &logger,
    std::shared_ptr<::grpc::Channel> &channel, 
    std::ifstream &source, 
    std::ofstream &destination, 
    gst_transformer::service::TransformConfig &config);
    
#endif
