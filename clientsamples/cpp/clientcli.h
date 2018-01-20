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

#ifndef __CLIENTCLI_H__
#define __CLIENTCLI_H__

#include "gsttransformer.grpc.pb.h"

using namespace gst_transformer::service;

extern TransformConfig transformConfig;
extern std::ifstream inputFileStream;
extern std::ofstream outputFileStream;
extern std::string endpoint;

int parse_opt(int argc, char **argv, bool);
void usage();

#endif
