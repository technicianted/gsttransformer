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

#ifndef __PIPELINEPARAMETERS_H__
#define __PIPELINEPARAMETERS_H__

#include <string>

enum class RateEnforcementPolicy {
    BLOCK = 0,
    ERROR = 1
};

/**
 * Wrapper for pipeline parameters.
 */
class PipelineParameters
{
public:
    PipelineParameters();
    
    RateEnforcementPolicy getRateEnforcemnetPolicy() const;
    PipelineParameters & setRateEnforcementPolicy(RateEnforcementPolicy policy);

    double getRate() const;
    PipelineParameters & setRate(double rate);

    double getLengthLimit() const;
    PipelineParameters & setLengthLimit(double lengthLimit);

    unsigned int getInputBufferSize() const;
    PipelineParameters & setInputBufferSize(unsigned int inputBufferSize);

    unsigned int getStartToleranceBytes() const;
    PipelineParameters & setStartToleranceBytes(unsigned int startToleranceBytes);

    unsigned int getReadTimeoutMilliseconds() const;
    PipelineParameters & setReadTimeoutMilliseconds(unsigned int readTimeoutMilliseconds);

    std::string debugString() const;

private:
    double rate;
    double lengthLimit;
    RateEnforcementPolicy rateEnforcementPolicy;
    unsigned int inputBufferSize;
    unsigned int startToleranceBytes;
    unsigned int readTimeoutMilliseconds;
};

#endif
