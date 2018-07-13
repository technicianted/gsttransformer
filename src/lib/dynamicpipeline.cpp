#include "dynamicpipeline.h"

#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <unistd.h>
#include <iostream>

#include "server/grunloop.h"

const std::string DynamicPipeline::SOURCE_NAME = "psource";
const std::string DynamicPipeline::SINK_NAME = "psink";

DynamicPipeline::~DynamicPipeline()
{
    if (this->lastWriteTimer) {
        g_source_remove(this->lastWriteTimer);
        this->lastWriteTimer = 0;
    }

    if (this->bus) {
        gst_bus_remove_watch(this->bus);
        gst_object_unref(this->bus);
    }
    if (this->source)
        gst_object_unref(this->source);
    if (this->sink)
        gst_object_unref(this->sink);
    if (this->pipeline) {
        auto r = gst_element_set_state(this->pipeline, GST_STATE_NULL);
        this->logger->trace("pipeline set state returned {0}", r);
        GstState current, pending;
        r = gst_element_get_state(this->pipeline, &current, &pending, GST_CLOCK_TIME_NONE);
        this->logger->trace("pipeline state before destruction: current {0}, pending {1}, return {2}", 
        gst_element_state_get_name(current), 
        gst_element_state_get_name(pending), 
        r);
        gst_object_unref(this->pipeline);
    }

    if (!GRunLoop::main()->isOnLoop()) {
        std::unique_lock<std::mutex> lock(drainedMutex);
        g_idle_add(&drain, this);
        this->drained = false;
        while(!this->drained)
            this->drainedCond.wait(lock);
    }

    this->logger->trace("DynamicPipeline destructor called");
}

void DynamicPipeline::start(
    const std::function<void(bool)> &termination)
{
    this->logger->debug("Starting pipeline");

    // make sure we have a main loop
    GRunLoop::main();

    this->done = false;
    this->terminationReason = PipelineTerminationReason::NONE;
    this->terminationMessage.clear();
    this->terminationCallback = termination;
    this->totalBytesRead = 0;
    this->totalBytesWritten = 0;
    this->processedTime = 0;
    this->lastWriteTime = std::chrono::steady_clock::now();

    gst_element_set_state(this->pipeline, GST_STATE_PLAYING);
    if (this->parameters.getRate() > 0) {
        auto event = gst_event_new_step(GST_FORMAT_PERCENT, 100, this->parameters.getRate(), FALSE, FALSE);
        gst_element_send_event(GST_ELEMENT(this->sink), event);
    }

    if (this->parameters.getReadTimeoutMilliseconds() > 0) {
        this->logger->debug("starting read timeout timer");
        this->lastWriteTimer = g_timeout_add(
            500,
            writeTimeoutCallback,
            this);
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

    if (this->lastWriteTimer) {
        g_source_remove(this->lastWriteTimer);
        this->lastWriteTimer = 0;
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

std::vector<std::string> DynamicPipeline::getPendingSample(int count)
{
    std::vector<std::string> sampleBuffers;

    for(int i=0; i<count; i++) {
        GstSample *sample = NULL;
        g_signal_emit_by_name(this->sink, "pull-sample", &sample);
        if (sample) {
            auto buffer = gst_sample_get_buffer(sample);
            GstMapInfo info;
            gst_buffer_map(buffer, &info, GST_MAP_READ);
            sampleBuffers.emplace_back((const char *)info.data, info.size);
            gst_buffer_unmap(buffer, &info);
            gst_sample_unref(sample);

            this->totalBytesWritten += info.size;
        }
    }

    return sampleBuffers;
}

PipelineTerminationReason DynamicPipeline::getTerminationReason() const
{
    return this->terminationReason;
}

std::string DynamicPipeline::getTerminationMessage() const
{
    return this->terminationMessage;
}

void DynamicPipeline::setSampleAvailableCallback(const std::function<void()> &callback)
{
    this->sampleAvailableCallback = callback;
}

void DynamicPipeline::setNeedDataCallback(const std::function<void()> &callback)
{
    this->needDataCallback = callback;
}

void DynamicPipeline::setEnoughDataCallback(const std::function<void()> &callback)
{
    this->enoughDataCallback = callback;
}

void DynamicPipeline::setEOSCallback(const std::function<void()> &callback)
{
    this->eosCallback = callback;
}

unsigned long DynamicPipeline::getProcessedInputBytes() const
{
    // TODO: this counts bytes including currently queued in appsrc
    return this->totalBytesRead;
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

    this->terminationCallback(force);
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
        logger->error(message);
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
    this->lastWriteTimer = 0;

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
        "block", FALSE, 
        "emit-signals", TRUE,
        NULL);
    g_object_set(this->sink, 
        "emit-signals", TRUE, 
        "sync", (this->parameters.getRate() <= 0 ? FALSE : TRUE),
        NULL);
    g_signal_connect(this->sink, "new-sample", G_CALLBACK(gstNewSample), this);
    g_signal_connect(this->source, "enough-data", G_CALLBACK(gstEnoughData), this);
    g_signal_connect(this->source, "need-data", G_CALLBACK(gstNeedData), this);

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
            if (p->eosCallback)
                p->eosCallback();
            std::lock_guard<std::mutex> lock(p->doneMutex);
            p->done = true;
            p->doneCond.notify_one();
            break;
        }

        case GST_MESSAGE_STATE_CHANGED: 
        {
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
    if (p->sampleAvailableCallback)
        p->sampleAvailableCallback();

    gint64 pos;
    auto r = gst_element_query_position(p->pipeline, GST_FORMAT_TIME, &pos);
    if (!r)
        p->logger->error("unable to query position");
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

void DynamicPipeline::gstEnoughData(GstElement * pipeline, guint size, gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    if (p->parameters.getRateEnforcemnetPolicy() == RateEnforcementPolicy::ERROR &&
        p->parameters.getRate() > 0) {
        p->terminatePipeline(
            PipelineTerminationReason::RATE_EXCEEDED,
            fmt::format("rate exceeded: {0}rt", p->parameters.getRate()));
    }
    else {
        if (p->enoughDataCallback)
            p->enoughDataCallback();
    }
}

void DynamicPipeline::gstNeedData(GstElement * pipeline, guint size, gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    if (p->needDataCallback)
        p->needDataCallback();
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

gboolean DynamicPipeline::writeTimeoutCallback(gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    
    auto timeNow = std::chrono::steady_clock::now();
    auto diffMillis = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - p->lastWriteTime).count();
    p->logger->trace("writeTimeoutCallback: delta {0}/{1}ms", diffMillis, p->parameters.getReadTimeoutMilliseconds());
    if (diffMillis > p->parameters.getReadTimeoutMilliseconds()) {
        p->terminatePipeline(
            PipelineTerminationReason::READ_TIMEOUT,
            fmt::format("read timeout of {0}ms exceeded", p->parameters.getReadTimeoutMilliseconds()));
        p->lastWriteTimer = 0;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

gboolean DynamicPipeline::drain(gpointer user_data)
{
    auto p = static_cast<DynamicPipeline *>(user_data);
    std::unique_lock<std::mutex> lock(p->drainedMutex);
    p->drained = true;
    p->drainedCond.notify_one();

    return FALSE;
}
