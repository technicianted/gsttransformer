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

#ifndef __SERVERPIPELINEFACTORY_H__
#define __SERVERPIPELINEFACTORY_H__

#include "pipeline.h"
#include "gsttransformer.pb.h"

class ServerPipelineFactory
{
public:
    Pipeline * get(const std::string &requestId, const gst_transformer::service::TransformConfig &config);
};

#endif
