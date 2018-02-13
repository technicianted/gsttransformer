#include "grunloop.h"

#include <assert.h>

GRunLoop * GRunLoop::Main = nullptr;
std::mutex GRunLoop::MainLock;

GRunLoop::GRunLoop()
{
    this->loop = nullptr;
    this->context = g_main_context_new();
}

GRunLoop::GRunLoop(bool isDefault)
{
    this->loop = nullptr;
    this->context = g_main_context_new();
    if (isDefault)
        this->context = g_main_context_default();
}

void GRunLoop::start()
{
    this->loop = g_main_loop_new(this->context, FALSE);
    this->thread = std::thread([&] {
        this->threadId = std::this_thread::get_id();
        g_main_loop_run(this->loop);
    });
    this->thread.detach();
}

void GRunLoop::stop()
{
    g_main_loop_quit(this->loop);
    g_main_loop_unref(this->loop);
    this->loop = nullptr;
}

void GRunLoop::assertOnLoop() const
{
    assert(this->isOnLoop());
}

bool GRunLoop::isOnLoop() const
{
    return std::this_thread::get_id() == this->threadId;
}

GRunLoop * GRunLoop::main()
{
    std::unique_lock<std::mutex> lock(MainLock);
    if (Main == nullptr) {
        Main = new GRunLoop(true);
        Main->start();
    }

    return Main;
}

bool GRunLoop::execute(const std::function<void()> &func)
{
    if (!this->isOnLoop()) {
        auto f = new std::function<void()>();
        *f = std::function<void()>([=] {
            std::unique_ptr<std::function<void()>> p(f);
            func();
        });
        if (g_idle_add(&_execute, f) > 0) {
            return true;
        }
        else {
            delete f;
            return false;
        }
    }
    else {
        func();
        return true;
    }
}

gboolean GRunLoop::_execute(gpointer user_data)
{
    auto func = static_cast<std::function<void()> *>(user_data);
    (*func)();

    return FALSE;
}
