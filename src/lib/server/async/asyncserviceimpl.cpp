#include "asyncserviceimpl.h"
#include "asynctransformimpl.h"

#include <functional>

#include "../grunloop.h"

namespace gst_transformer {
namespace service {

AsyncServiceImpl::AsyncServiceImpl(
    GstTransformer::AsyncService *service,
    ::grpc::ServerCompletionQueue *completionQueue,
    const ServiceParametersStruct &params)
{
    this->globalLogger = spdlog::stderr_logger_mt("asyncserviceimpl");

    this->service = service;
    this->completionQueue = std::move(completionQueue);
    this->params = params;
}

AsyncServiceImpl::~AsyncServiceImpl()
{
}

void AsyncServiceImpl::start()
{
    new AsyncTransformImpl(this->globalLogger, GRunLoop::main(), service, completionQueue, &this->params);

    void* tag;
    bool ok;
    bool shutdown = false;
    while (!shutdown) {
        shutdown = !this->completionQueue->Next(&tag, &ok);
        if (!shutdown) {
            auto func = *static_cast<std::function<void(bool)>*>(tag);
            func(ok);
        }
    }
}

void AsyncServiceImpl::stop()
{
    this->completionQueue->Shutdown();
}

}
}
