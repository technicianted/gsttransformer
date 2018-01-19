FROM ubuntu:16.04

RUN apt-get update && \
	apt-get install -y \
		ca-certificates \
		gstreamer1.0-plugins-base \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-ugly \
		libgstreamer1.0-0 

ADD bin/* run.sh /
ADD libs/* /usr/local/lib/

ENTRYPOINT /run.sh
