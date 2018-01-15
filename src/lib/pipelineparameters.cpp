#include "pipelineparameters.h"

#include <fmt/format.h>

PipelineParameters::PipelineParameters()
{
    this->rateEnforcementPolicy = RateEnforcementPolicy::BLOCK;
    this->rate = 1.0;
    this->lengthLimit = 0.0;
    this->inputBufferSize = 0;
    this->readTimeoutMilliseconds = 0;
    this->startToleranceBytes = 0;
}

RateEnforcementPolicy PipelineParameters::getRateEnforcemnetPolicy() const
{
    return this->rateEnforcementPolicy;
}

PipelineParameters & PipelineParameters::setRateEnforcementPolicy(RateEnforcementPolicy policy)
{
    this->rateEnforcementPolicy = policy;
    return *this;
}

double PipelineParameters::getRate() const
{
    return this->rate;
}

PipelineParameters & PipelineParameters::setRate(double rate)
{
    this->rate = rate;
    return *this;
}

double PipelineParameters::getLengthLimit() const
{
    return this->lengthLimit;
}

PipelineParameters & PipelineParameters::setLengthLimit(double lengthLimit)
{
    this->lengthLimit = lengthLimit;
    return *this;
}

unsigned int PipelineParameters::getInputBufferSize() const
{
    return this->inputBufferSize;
}

PipelineParameters & PipelineParameters::setInputBufferSize(unsigned int inputBufferSize)
{
    this->inputBufferSize = inputBufferSize;
    return *this;
}

unsigned int PipelineParameters::getStartToleranceBytes() const
{
    return this->startToleranceBytes;
}

PipelineParameters & PipelineParameters::setStartToleranceBytes(unsigned int startToleranceBytes)
{
    this->startToleranceBytes = startToleranceBytes;
    return *this;
}

unsigned int PipelineParameters::getReadTimeoutMilliseconds() const
{
    return this->readTimeoutMilliseconds;
}

PipelineParameters & PipelineParameters::setReadTimeoutMilliseconds(unsigned int readTimeoutMilliseconds)
{
    this->readTimeoutMilliseconds = readTimeoutMilliseconds;
    return *this;
}

std::string PipelineParameters::debugString() const
{
    return fmt::format(
        "rate: {0}, lengthLimit: {1}, rateEnforcementPolicy: {2}, inputBufferSize: {3}, startToleranceBytes: {4}, readTimeoutMillis: {5}",
        this->rate,
        this->lengthLimit,
        (int)this->rateEnforcementPolicy,
        this->inputBufferSize,
        this->startToleranceBytes,
        this->readTimeoutMilliseconds
    );
}
