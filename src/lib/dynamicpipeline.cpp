#include "dynamicpipeline.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unistd.h>
#include <iostream>

const std::string DynamicPipeline::SOURCE_NAME = "psource";
const std::string DynamicPipeline::SINK_NAME = "psink";
std::thread DynamicPipeline::mainLoopThread;
GMainLoop *DynamicPipeline::mainLoop = NULL;
std::mutex DynamicPipeline::mainLoopLock;

DynamicPipeline::~DynamicPipeline()
{
}

void DynamicPipeline::start(const std::function<int(const char *, int)> &consumer)
{
    this->logger->debug("Starting pipeline");

    std::unique_lock<std::mutex> lock(this->mainLoopLock);
    if (mainLoop == NULL) {
        mainLoopThread = std::thread([]{
            mainLoop = g_main_loop_new (NULL, FALSE);
            g_main_loop_run(mainLoop);
        });
        mainLoopThread.detach();
    }

    this->done = false;
    this->terminationReason = PipelineTerminationReason::NONE;
    this->terminationMessage.clear();
    this->consumer = consumer;
    this->totalBytesRead = 0;
    this->totalBytesWritten = 0;
    this->processedTime = 0;
    this->lastWriteTime = std::chrono::steady_clock::now();

    gst_element_set_state(this->pipeline, GST_STATE_PLAYING);
    if (this->parameters.getRate() > 0) {
        auto event = gst_event_new_step(GST_FORMAT_PERCENT, 100, this->parameters.getRate(), FALSE, FALSE);
        gst_element_send_event(GST_ELEMENT(this->sink), event);
    }
}

void DynamicPipeline::endData()
{
    if (this->terminationReason == PipelineTerminationReason::NONE) {
        this->terminatePipeline(
            PipelineTerminationReason::END_OF_STREAM,
            "end of stream",
            false);
    }
}

void DynamicPipeline::waitUntilCompleted()
{
    this->logger->debug("wait until completed");  
    std::unique_lock<std::mutex> doneLock(this->doneMutex);
    while (!this->done)
        this->doneCond.wait(doneLock);
    this->logger->debug("wait completed");  
}

PipelineTerminationReason DynamicPipeline::getTerminationReason() const
{
    return this->terminationReason;
}

std::string DynamicPipeline::getTerminationMessage() const
{
    return this->terminationMessage;
}

unsigned long DynamicPipeline::getProcessedInputBytes() const
{
    // TODO: this counts bytes including currently queued in appsrc
    return this->totalBytesWritten;
}

unsigned long DynamicPipeline::getProcessedOutputBytes() const
{
    return this->totalBytesWritten;
}

double DynamicPipeline::getProcessedTime() const
{
    return (double)this->processedTime / GST_SECOND;
}

void DynamicPipeline::stop()
{
    this->logger->debug("stopping");  
    std::unique_lock<std::mutex> lock(this->doneMutex);
    this->terminationReason = PipelineTerminationReason::CANCELLED;
    gst_element_set_state(this->pipeline, GST_STATE_NULL);
}

int DynamicPipeline::addData(const char *buffer, int size)
{
    if (this->parameters.getReadTimeoutMilliseconds() > 0) {
        auto timeNow = std::chrono::steady_clock::now();
        auto diffMillis = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - this->lastWriteTime).count();
        if (diffMillis > this->parameters.getReadTimeoutMilliseconds()) {
            this->terminatePipeline(
                PipelineTerminationReason::READ_TIMEOUT,
                fmt::format("read timeout of {0}ms exceeded", this->parameters.getReadTimeoutMilliseconds()));
            return -1;
        }
    }

    auto gbuffer = gst_buffer_new_and_alloc(size);
    GstMapInfo info; 
    gst_buffer_map(gbuffer, &info, GST_MAP_WRITE);
    memcpy(info.data, buffer, size);
    gst_buffer_unmap(gbuffer, &info);

    GstFlowReturn ret = gst_app_src_push_buffer(this->source, gbuffer);
    if (ret != GST_FLOW_OK) {
        this->logger->debug("write returned error: {0}", (int)ret);
        // pipeline could be EOSing (normal shutdown) or FLUSHING for forced shutdown
        if (ret != GST_FLOW_EOS && ret != GST_FLOW_FLUSHING) {
            this->terminationReason = PipelineTerminationReason::INTERNAL_ERROR;
        }

        return -1;
    }

    // refresh after potential block
    this->lastWriteTime = std::chrono::steady_clock::now();
    // TODO: this is not accurate representation of "processed" bytes
    this->totalBytesRead += size;

    return size;
}

void DynamicPipeline::terminatePipeline(PipelineTerminationReason reason, const std::string &message, bool force)
{
    this->logger->debug("terminating pipeline: {0}: {1}", (int)reason, message);
    this->terminationReason = reason;
    this->terminationMessage = message;
    if (!force) {
        gst_app_src_end_of_stream(this->source);
    } else {
        g_idle_add(gstTerminateIdleCallback, this);
    }
}

DynamicPipeline * DynamicPipeline::createFromSpecs(const PipelineParameters &parameters, const std::string &pipelineId, const std::string &specs)
{
    // TODO: not ideal but convenient. global lock.
    auto logger = spdlog::stderr_logger_mt(fmt::format("dynamicpipeline/{0}", pipelineId));

    auto desc = fmt::format("appsrc name={0} ! {1} ! appsink name={2}", SOURCE_NAME, specs, SINK_NAME);
    logger->debug("spec: {0}", desc);
    logger->debug("parameters: {0}", parameters.debugString());

    GError *error = NULL;
    auto pipeline = gst_parse_launch(desc.c_str(), &error);
    if (!pipeline) {
        auto message = fmt::format("could not create pipeline {0}: {1}", pipelineId, error->message);
        logger->notice(message);
        g_error_free(error);
        throw std::invalid_argument(message);
    }

    return new DynamicPipeline(logger, parameters, pipelineId, pipeline);
}

DynamicPipeline::DynamicPipeline(std::shared_ptr<spdlog::logger> &logger, const PipelineParameters &parameters, const std::string &pipelineId, GstElement *pipeline)
{
    this->logger = logger;

    this->parameters = parameters;
    this->pipelineId = pipelineId;
    this->pipeline = pipeline;

    this->bus = gst_pipeline_get_bus(GST_PIPELINE(this->pipeline));
    gst_bus_add_watch(this->bus, (GstBusFunc) gstBusMessage, this);

    this->source = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(this->pipeline), SOURCE_NAME.c_str()));
    if (!this->source)
        throw std::logic_error(fmt::format("Unable to obtain source bin for {0}", this->pipelineId));
    g_assert(GST_IS_APP_SRC(this->source));
    this->logger->debug("created appsrc");    

    this->sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(this->pipeline), SINK_NAME.c_str()));
    if (!this->sink)
        throw std::logic_error(fmt::format("Unable to obtain sink bin for {0}", this->pipelineId));
    g_assert(GST_IS_APP_SINK(this->sink));
    this->logger->debug("created appsink");    

    if (this->parameters.getInputBufferSize() > 0)
        gst_app_src_set_max_bytes(this->source, this->parameters.getInputBufferSize());
    
    this->logger->debug("appsrc max bytes {0}", gst_app_src_get_max_bytes(this->source));
    g_object_set(this->source, 
        "block", TRUE, 
        NULL);
    g_object_set(this->sink, 
        "emit-signals", TRUE, 
        "sync", (this->parameters.getRate() <= 0 ? FALSE : TRUE),
        NULL);
    g_signal_connect(this->sink, "new-sample", G_CALLBACK(gstNewSample), this);
    g_signal_connect(this->source, "enough-data", G_CALLBACK(gstEnoughData), this);

    this->logger->info("created pipeline");
}

gboolean DynamicPipeline::gstBusMessage(GstBus * bus, GstMessage * message, gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);

    p->logger->debug("bus got message {0}", gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

    switch(GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *err = NULL;
            gchar *dbg_info = NULL;

            gst_message_parse_error(message, &err, &dbg_info);
            p->logger->error("error from element {0}: {1}", GST_OBJECT_NAME(message->src), err->message);
            p->logger->error("debugging info: {0}", (dbg_info) ? dbg_info : "none");
            g_error_free(err);
            g_free(dbg_info);
    
            p->terminationReason = PipelineTerminationReason::INTERNAL_ERROR;
            gst_element_set_state(p->pipeline, GST_STATE_NULL);
    
            break;
        }

        case GST_MESSAGE_EOS:
        {
            std::lock_guard<std::mutex> lock(p->doneMutex);
            p->done = true;
            p->doneCond.notify_one();
            break;
        }

        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
            p->logger->debug("element {0} changed state from {1} to {2}",
                GST_OBJECT_NAME (message->src),
                gst_element_state_get_name (old_state),
                gst_element_state_get_name (new_state));
            break;
        }

        case GST_MESSAGE_STREAM_STATUS:
        {
            GstStreamStatusType type;
            GstElement *owner;
            gst_message_parse_stream_status(message, &type, &owner);
            p->logger->debug("status type is {0} from {1}", type, GST_OBJECT_NAME(owner));
            break;
        }
        case GST_MESSAGE_STREAM_START:
        {
            p->logger->debug("stream start from {0}", GST_OBJECT_NAME(message->src));
            break;
        }

        default:
            break;
    }

    return TRUE;
}

void DynamicPipeline::gstNewSample(GstElement *sink, gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    GstSample *sample;
    g_signal_emit_by_name(p->sink, "pull-sample", &sample);
    if (sample) {
        auto buffer = gst_sample_get_buffer(sample);
        GstMapInfo info; 
        gst_buffer_map(buffer, &info, GST_MAP_READ);
        p->consumer((const char *)info.data, info.size);
        gst_buffer_unmap(buffer, &info);
        gst_sample_unref(sample);

        p->totalBytesWritten += info.size;
        gint64 pos;
        auto r = gst_element_query_position(p->pipeline, GST_FORMAT_TIME, &pos);
        if (!r)
            p->logger->notice("unable to query position");
        if (pos > p->processedTime)
            p->processedTime = pos;
        if (p->parameters.getLengthLimit() > 0 ) {
            if (p->processedTime >= p->parameters.getLengthLimit() * GST_MSECOND) {
                    p->terminatePipeline(
                        PipelineTerminationReason::ALLOWED_DURATION_EXCEEDED, 
                        fmt::format("max duration exceeded: {0}ms", p->parameters.getLengthLimit()));
            }
        }
    }
}

void DynamicPipeline::gstEnoughData(GstElement * pipeline, guint size, gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    if (p->parameters.getRateEnforcemnetPolicy() == RateEnforcementPolicy::ERROR &&
        p->parameters.getRate() > 0) {
        p->terminatePipeline(
            PipelineTerminationReason::RATE_EXCEEDED,
            fmt::format("rate exceeded: {0}rt", p->parameters.getRate()));
    }
}

gboolean DynamicPipeline::gstTerminateIdleCallback(gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    p->logger->debug("termination idle callback");

    gst_element_set_state(p->pipeline, GST_STATE_NULL);
    std::lock_guard<std::mutex> lock(p->doneMutex);
    p->done = true;
    p->doneCond.notify_one();

    return G_SOURCE_REMOVE;
}
