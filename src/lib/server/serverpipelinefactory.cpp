#include "serverpipelinefactory.h"
#include "dynamicpipeline.h"

Pipeline * ServerPipelineFactory::get(const std::string &requestId, const gst_transformer::service::TransformConfig &config)
{
    auto requestedParams = config.pipeline_parameters();
    PipelineParameters params;

    if (requestedParams.rate())
        params.setRate(requestedParams.rate());
    params.setRateEnforcementPolicy((RateEnforcementPolicy)requestedParams.rate_enforcement_policy());

    if (requestedParams.length_limit_milliseconds())
        params.setLengthLimit(requestedParams.length_limit_milliseconds());

    Pipeline *pipeline = NULL;
    if (config.pipeline_name().empty()) {
        if (config.pipeline().empty()) 
            throw std::invalid_argument("No dynamic pipeline specs specified");

        pipeline =  DynamicPipeline::createFromSpecs(
        params, 
        requestId,
        config.pipeline());
    }
    else {
        throw std::invalid_argument("Not supported");
    }
    
    return pipeline;
}
