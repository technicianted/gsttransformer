#include "serviceparams.h"

#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace gst_transformer {
namespace service {

ServiceParams::ServiceParams()
{

}

void ServiceParams::loadFromJsonStream(std::istream &stream)
{
    json j;
    stream >> j;
    if (j.find("limits") != j.end()) {
        auto limits = j.at("limits");
        if (limits.find("allowDynamicPipelines") != limits.end())
            this->set_allow_dynamic_pipelines(limits.at("allowDynamicPipelines"));
        if (limits.find("rate") != limits.end()){
            auto rate = limits.at("rate");
            if (rate.find("max") != rate.end())
                this->set_max_rate(rate.at("max").get<double>());
        }
        if (limits.find("lengthLimitMillis") != limits.end()) {
            auto lengthLimit = limits.at("lengthLimitMillis");
            if (lengthLimit.find("max") != lengthLimit.end())
                this->set_max_length_millis(lengthLimit.at("max").get<unsigned long>());
        }
        if (limits.find("startToleranceBytes") != limits.end()) {
            auto startToleranceBytes = limits.at("startToleranceBytes");
            if (startToleranceBytes.find("max") != startToleranceBytes.end())
                this->set_max_start_tolerance_bytes(startToleranceBytes.at("max").get<unsigned long>());
        }
        if (limits.find("readTimeoutMillis") != limits.end()) {
            auto readTimeout = limits.at("readTimeoutMillis");
            if (readTimeout.find("max") != readTimeout.end())
                this->set_max_read_timeout_millis(readTimeout.at("max").get<unsigned long>());
        }
        if (limits.find("pipelineOutputBuffer") != limits.end()) {
            auto pipelineOutputBuffer = limits.at("pipelineOutputBuffer");
            if (pipelineOutputBuffer.find("max") != pipelineOutputBuffer.end())
                this->set_max_pipeline_output_buffer(pipelineOutputBuffer.at("max").get<unsigned long>());
        }
    }
    if (j.find("pipelines") != j.end()) {
        auto pipelines = j.at("pipelines");
        for(auto iter = pipelines.begin(); iter != pipelines.end(); iter++) {
            auto pipeline = *iter;
            PipelineStruct entry;
            entry.set_id(pipeline.at("id").get<std::string>());
            entry.set_specs(pipeline.at("specs").get<std::string>());
            (*this->mutable_pipelines())[entry.id()] = entry;
        }
    }
    else if (!this->allow_dynamic_pipelines()) {
        throw std::invalid_argument("dynamic pipelines is disabled and no pipelines specified");
    }
}

void ServiceParams::loadFromJsonString(const std::string &json)
{
    std::stringstream stream(json);
    this->loadFromJsonStream(stream);
}

void ServiceParams::loadFromJsonFile(const std::string &json)
{
    std::ifstream ifs(json);
    if (ifs.fail())
        throw std::invalid_argument(fmt::format("Unable to open file {0}", json));
    this->loadFromJsonStream(ifs);
}

}
}
