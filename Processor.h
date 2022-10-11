#pragma once

#include "IConsumer.h"
#include "SignalDispatcher.h"
#include "ThreadSafeQueue.h"

#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

template <typename KeyT, typename ValueT>
class Processor {

    enum class QueueCreatePolicy {
        DontCreate,
        CanCreate,
    };
    using QueueAndMaybeConsumer = std::pair<ThreadSafeQueue<ValueT>,
        std::atomic<IConsumer<KeyT, ValueT>*>>;

public:
    Processor(size_t max_queue_capacity = 10000)
        : m_max_queue_capacity(max_queue_capacity)
        , m_thread([this] { process(); })
    {
    }

    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    Processor(Processor&&) = delete;
    Processor& operator=(Processor&&) = delete;

    ~Processor()
    {
        m_running = false;
        m_signal_dispatcher.signal();
        m_thread.join();
    }

    [[nodiscard]] bool try_subscribe(
        const KeyT& id,
        IConsumer<KeyT, ValueT>& consumer)
    {
        bool result = false;
        do_for_queue_pair(
            id,
            [&result, &consumer](QueueAndMaybeConsumer& queue_pair) {
                if (!queue_pair.second) {
                    queue_pair.second.store(&consumer);
                    result = true;
                }
            },
            QueueCreatePolicy::CanCreate);
        if (result) {
            m_signal_dispatcher.signal();
        }
        return result;
    }

    void unsubscribe(const KeyT& id)
    {
        do_for_queue_pair(
            id,
            [](QueueAndMaybeConsumer& queue_pair) {
                queue_pair.second.store(nullptr);
            },
            QueueCreatePolicy::DontCreate);
    }

    [[nodiscard]] bool try_push(const KeyT& id, ValueT&& value)
    {
        bool result = false;
        do_for_queue_pair(
            id,
            [this, &result, &value](QueueAndMaybeConsumer& queue_pair) {
                auto& [queue, _] = queue_pair;
                if (queue.size() < m_max_queue_capacity) {
                    queue.push(std::move(value));
                    result = true;
                }
            },
            QueueCreatePolicy::CanCreate);
        if (result) {
            m_signal_dispatcher.signal();
        }
        return result;
    }

private:
    void do_for_queue_pair(
        const KeyT& id, std::function<void(QueueAndMaybeConsumer&)>&& callback,
        QueueCreatePolicy create_policy = QueueCreatePolicy::DontCreate)
    {
        {
            std::shared_lock read_lock { m_queue_shared_mutex };
            if (auto it = m_queues.find(id); it != m_queues.end()) {
                callback(it->second);
                return;
            }
        }
        if (QueueCreatePolicy::CanCreate == create_policy) {
            std::lock_guard write_lock { m_queue_shared_mutex };
            callback(m_queues[id]);
        }
    }

    void process()
    {
        while (m_running) {
            m_signal_dispatcher.wait_for_signal();

            std::shared_lock read_lock { m_queue_shared_mutex };

            for (auto& key_and_pair : m_queues) {
                auto& key = key_and_pair.first;
                auto& [queue, consumer_ptr] = key_and_pair.second;

                if (auto consumer_ptr_copy = consumer_ptr.load(); consumer_ptr_copy) {
                    for (auto sptr = queue.try_pop(); sptr; sptr = queue.try_pop()) {
                        consumer_ptr_copy->Consume(key, *sptr);
                    }
                }
            }
        }
    }

private:
    std::unordered_map<KeyT, QueueAndMaybeConsumer> m_queues;
    std::shared_mutex m_queue_shared_mutex;

    SignalDispatcher m_signal_dispatcher;
    const size_t m_max_queue_capacity = 10000;
    std::atomic_bool m_running = true;
    std::thread m_thread;
};
