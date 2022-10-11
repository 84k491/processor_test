#pragma once

#include <atomic>
#include <memory>
#include <mutex>

template <typename T>
class ThreadSafeQueue {

    struct Node {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;
    };

public:
    ThreadSafeQueue()
        : m_head(std::make_unique<Node>())
        , m_tail(m_head.get())
    {
    }

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;
    ~ThreadSafeQueue() = default;

    std::shared_ptr<T> try_pop()
    {
        std::unique_ptr<Node> old_head = pop_head();
        return old_head ? old_head->data : nullptr;
    }

    void push(T&& new_value)
    {
        std::shared_ptr new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr new_node = std::make_unique<Node>();
        Node* const new_tail = new_node.get();

        std::lock_guard tail_lock(tail_mutex);
        m_tail->data = new_data;
        m_tail->next = std::move(new_node);
        m_tail = new_tail;
        m_size.fetch_add(1);
    }

    size_t size() const { return m_size.load(); }

private:
    std::unique_ptr<Node> pop_head()
    {
        std::lock_guard head_lock(head_mutex);
        if (std::lock_guard tail_lock(tail_mutex); m_head.get() == m_tail) {
            return nullptr;
        }

        m_size.fetch_sub(1);
        std::unique_ptr<Node> old_head = std::move(m_head);
        m_head = std::move(old_head->next);
        return old_head;
    }

private:
    std::atomic<size_t> m_size = {};
    std::unique_ptr<Node> m_head;
    Node* m_tail;
    mutable std::mutex head_mutex;
    mutable std::mutex tail_mutex;
};
