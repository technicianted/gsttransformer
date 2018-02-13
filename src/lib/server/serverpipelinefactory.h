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
#include "serviceparameters.pb.h"

namespace gst_transformer {
namespace service {

/**
 * Factory class to create pipelines from gst-launch specs.
 * It also handles predefined pipelines that can be referenced by name.
 */
class ServerPipelineFactory
{
public:
    /**
     * Construct a new factory.
     * 
     * \param: serviceParams Service parameters used when creating pipelines.
     */
    ServerPipelineFactory(const ServiceParametersStruct &serviceParams);
    /**
     * Obtain a pipeline instance. It can be a predefined pipeline, or a dynamic
     * one created from gst specs in the request config.
     * 
     * \param requestId the request ID for logging.
     * \param config request parameters.
     * \return a pipeline instance ready for use.
     */
    std::unique_ptr<Pipeline> get(const std::string &requestId, const TransformConfig &config);

private:
    ServiceParametersStruct serviceParams;
};

}
}

#endif
