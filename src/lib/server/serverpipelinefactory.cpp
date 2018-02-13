#include "serverpipelinefactory.h"
#include "dynamicpipeline.h"

namespace gst_transformer {
namespace service {

ServerPipelineFactory::ServerPipelineFactory(const ServiceParametersStruct &serviceParams)
{
    this->serviceParams = serviceParams;
}
 
std::unique_ptr<Pipeline> ServerPipelineFactory::get(const std::string &requestId, const TransformConfig &config)
{
    auto requestedParams = config.pipeline_parameters();
    ::PipelineParameters params;

    if (requestedParams.rate())
        params.setRate(requestedParams.rate());
    params.setRateEnforcementPolicy((::RateEnforcementPolicy)requestedParams.rate_enforcement_policy());

    if (requestedParams.length_limit_milliseconds())
        params.setLengthLimit(requestedParams.length_limit_milliseconds());

    if (requestedParams.read_timeout_milliseconds())
        params.setReadTimeoutMilliseconds(requestedParams.read_timeout_milliseconds());
 
    std::unique_ptr<Pipeline> pipeline;
    if (!config.pipeline_name().empty() && !config.pipeline().empty())
        throw std::invalid_argument("cannot specify both pipeline name and specs");
    if (config.pipeline_name().empty() && config.pipeline().empty())
        throw std::invalid_argument("must specify either pipeline name or specs");
        
    if (config.pipeline_name().empty()) {
        if (config.pipeline().empty()) 
            throw std::invalid_argument("No dynamic pipeline specs specified");

        pipeline.reset(DynamicPipeline::createFromSpecs(
            params, 
            requestId,
            config.pipeline()));
    }
    else {
        auto iter = this->serviceParams.pipelines().find(config.pipeline_name());
        if (iter == this->serviceParams.pipelines().end())
            throw std::invalid_argument(fmt::format("pipeline name '{0}' not defined", config.pipeline_name()));
        
        pipeline.reset(DynamicPipeline::createFromSpecs(
            params, 
            requestId,
            iter->second.specs()));
    }
    
    return pipeline;
}

}
}

