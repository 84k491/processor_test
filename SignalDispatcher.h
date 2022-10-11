#pragma once

#include <condition_variable>
#include <mutex>

class SignalDispatcher {
public:
    void signal()
    {
        std::lock_guard l { m_mutex };
        m_flag = true;
        m_cond_var.notify_all();
    }

    void wait_for_signal()
    {
        std::unique_lock ul { m_mutex };
        m_cond_var.wait(ul, [this] { return m_flag; });
        m_flag = false;
    }

private:
    bool m_flag = false;
    std::mutex m_mutex;
    std::condition_variable m_cond_var;
};
