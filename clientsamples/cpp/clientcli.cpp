#include <getopt.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>

#include "gsttransformer.grpc.pb.h"
using namespace gst_transformer::service;

TransformConfig transformConfig;
std::string inputFileName = "/dev/stdin";
std::ifstream inputFileStream;
std::string outputFileName = "/dev/stdout";
std::ofstream outputFileStream;
std::string endpoint;
int writeDelay = 0;
bool randomizeWriteDelay = false;

int parse_opt(int argc, char **argv, bool requiresEndpoint)
{
	auto pipelineConfig = transformConfig.mutable_pipeline_parameters();

	int key;
	while ((key = getopt(argc, argv, "+e:r:w:l:b:i:o:s:p:")) != -1) {
		switch (key) {
			case 'e':
				if (!strcmp(optarg, "block"))
					pipelineConfig->set_rate_enforcement_policy(RateEnforcementPolicy::BLOCK);
				else if (!strcmp(optarg, "error"))
					pipelineConfig->set_rate_enforcement_policy(RateEnforcementPolicy::ERROR);
				else {
					std::cerr << "Invalid rate enforcement type " << optarg << std::endl;
					return -1;
				}
				break;
			case 'r':
			{
				double rate = strtod(optarg, NULL);
				if (rate != -1 && rate <= 0) {
					std::cerr << "Invalid rate " << optarg << std::endl;
					return -1;
				}
				pipelineConfig->set_rate(rate);
				break;
			}
			case 'l':
			{
				unsigned int maxLen = strtoul(optarg, NULL, 10);
				pipelineConfig->set_length_limit_milliseconds(maxLen);
				break;
			}
			case 'b':
			{
				unsigned int buffer = strtoul(optarg, NULL, 10);
				transformConfig.set_pipeline_output_buffer(buffer);
				break;
			}
			case 'w':
				writeDelay = strtol(optarg, NULL, 10);
				if (writeDelay < 0) {
					writeDelay = abs(writeDelay);
					randomizeWriteDelay = true;
				}
				break;
			case 'i':
				inputFileName = optarg;
				break;
			case 'o':
				outputFileName = optarg;
				break;
			case 's':
				transformConfig.set_pipeline(optarg);
				break;
			case 'p':
				transformConfig.set_pipeline_name(optarg);
				break;
		}
	}

	if (requiresEndpoint) {
		if ((argc - optind) != 1)
			return -1;
		endpoint = argv[optind];
	}

	if (transformConfig.pipeline().empty() && transformConfig.pipeline_name().empty()) {
		std::cerr << "Pipeline specs (-s) or name (-p) is required." << std::endl;
		return -1;
	}

	if (!transformConfig.pipeline().empty() && !transformConfig.pipeline_name().empty()) {
		std::cerr << "Pipeline specs (-s) and name (-p) cannot be both set." << std::endl;
		return -1;
	}

	inputFileStream = std::ifstream(inputFileName);
	if (inputFileStream.fail()) {
		std::cerr << "Unable to open input file " << inputFileName << std::endl;
		return -1;
	}
	outputFileStream = std::ofstream(outputFileName);
	if (outputFileStream.fail()) {
		std::cerr << "Unable to open output file " << outputFileName << std::endl;
		return -1;
	}

	return 0;
}

void usage()
{
	std::cerr << "Usage: gsttransformerclient [OPTION...] [<endpoint>]" << std::endl;
	std::cerr << "  -b BUFFER\tSet pipeline output buffer size. Default 0 (no buffering)." << std::endl;
	std::cerr << "  -e MODE\tRate enforcement mode {BLOCK|ERROR}. Default BLOCK." << std::endl;
	std::cerr << "  -i FILE\tInput file. Default stdin." << std::endl;
	std::cerr << "  -l LEN\tSet maximum audio duration in milliseconds, 0 unlimited. Default 0." << std::endl;
	std::cerr << "  -o FILE\tOutput file. Default stout." << std::endl;
	std::cerr << "  -p PIPELINE\tExisting pipeline name as defined on the server." << std::endl;
	std::cerr << "  -r RATE\tTransformation rate in double: 1.0 = RT, -1 passthrough. Default 1.0." << std::endl;
	std::cerr << "  -s SPECS\tGStream pipeline specs." << std::endl;
	std::cerr << "  -w MS\tWrite delay in ms, 0 none, <0ms random up to |MS|" << std::endl;

    exit(1);
}
