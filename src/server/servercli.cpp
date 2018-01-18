#include <getopt.h>
#include <stdlib.h>
#include <string>
#include <iostream>

std::string logLevel = "info";
std::string configurationFile;
std::string endpoint;

int parse_opt(int argc, char **argv)
{
	int key;
	while ((key = getopt(argc, argv, "+d:c:")) != -1) {
		switch (key) {
			case 'c':
                configurationFile = optarg;
				break;

			case 'd':
                logLevel = optarg;
                break;

            default:
                std::cerr << "Unknown option" << std::endl;
                return -1;
		}
	}

    if ((argc - optind) != 1)
        return -1;
    endpoint = argv[optind];

	return 0;
}

void usage()
{
	std::cerr << "Usage: gsttransformerserver [OPTION...] <endpoint>" << std::endl;
    std::cerr << "  -c FILE\tjson configuration file." << std::endl;
	std::cerr << "  -d LEVEL\tDebug level {trace|debug|info|notice|error}." << std::endl;
    std::cerr << "endpoint: grpc style endpoint" << std::endl;

    exit(1);
}
