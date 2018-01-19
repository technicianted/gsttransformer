.PHONY=default build push

default: build

build: Dockerfile Makefile 
	cp -a /usr/local/lib/libgrpc* libs/
	cp -a /usr/local/lib/libprotobuf* libs/
	cp -a ../build/libgsttransformer_server.so ../build/libgsttransformer.so libs/
	cp -a ../build/gsttransformerserver bin/

	docker build -t gsttransformer .

push: build
	docker tag gsttransformer technicianted/gsttransformer:experimental
	docker push technicianted/gsttransformer

clean:
	rm -rf libs/*
	rm -rf bin/*
