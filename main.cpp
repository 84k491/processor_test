#include "Processor.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

bool g_some_test_failed = false;
class TestChecker {
public:
    TestChecker(std::string_view test_name)
        : m_test_name(test_name)
    {
        std::cout << "Running test: " << m_test_name << "..." << std::endl;
    }
    ~TestChecker()
    {
        if (m_failed) {
            std::cout << "FAILED! On check: " << m_error_message << std::endl;
        } else {
            std::cout << "OK." << std::endl;
        }
    }

    void EXPECT_TRUE(bool v, std::string_view msg = "")
    {
        if (!v) {
            m_failed = true;
            g_some_test_failed = true;
            m_error_message = msg;
        }
    }

    template <class T>
    void EXPECT_EQ(const T& l, const T& r, std::string_view msg = "")
    {
        EXPECT_TRUE(l == r, msg);
    }

    void EXPECT_FALSE(bool v, std::string_view msg = "") { EXPECT_TRUE(!v, msg); }

private:
    bool m_failed = false;
    std::string m_error_message;
    std::string m_test_name;
};

template <typename KeyT, typename ValueT>
class MockProducer {
public:
    MockProducer(Processor<KeyT, ValueT>& p)
        : m_processor(p)
    {
    }
    ~MockProducer() { m_thread->join(); }

    void produce(std::vector<KeyT> keys, ValueT value, size_t amount)
    {
        m_thread = std::make_unique<std::thread>([=] {
            for (size_t i = 0; i < amount; ++i) {
                void(!m_processor.try_push(keys[i % keys.size()], ValueT(value)));
            }
        });
    }

    Processor<KeyT, ValueT>& m_processor;
    std::unique_ptr<std::thread> m_thread;
};

template <typename KeyT, typename ValueT>
class MockConsumer final : public IConsumer<KeyT, ValueT> {
public:
    MockConsumer(bool store_values = true)
        : m_store_values(store_values)
    {
    }

    void Consume(KeyT, const ValueT& value) override
    {
        if (m_store_values) {
            m_consumed.push_back(value);
        }
    }

    auto consumed_amount() const { return m_consumed.size(); }
    auto first_consumed() const { return m_consumed[0]; }

private:
    std::vector<ValueT> m_consumed;
    bool m_store_values = true;
};

void push_before_subscribe()
{
    TestChecker c("push_before_subscribe");

    MockConsumer<std::string, std::string> consumer;
    std::unique_ptr processor = std::make_unique<Processor<std::string, std::string>>();

    c.EXPECT_TRUE(processor->try_push("one", "first_value"), "first push");
    c.EXPECT_TRUE(processor->try_push("one", "second_value"), "second push");
    c.EXPECT_TRUE(processor->try_subscribe("one", consumer), "subscribe");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor.reset(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    c.EXPECT_EQ(consumer.consumed_amount(), 2ul, "consumed amount");
}

void push_after_subscribe()
{
    TestChecker c("push_after_subscribe");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_subscribe(1, consumer), "subscribe");
    c.EXPECT_TRUE(processor->try_push(1, 2.2), "first push");
    c.EXPECT_TRUE(processor->try_push(1, 2.3), "second push");
    c.EXPECT_TRUE(processor->try_push(1, 2.4), "third push");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor.reset(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    c.EXPECT_EQ(consumer.consumed_amount(), 3ul, "consumed amount");
}

void push_subscribe_push()
{
    TestChecker c("push_subscribe_push");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_push(2, 3.2), "first push");
    c.EXPECT_TRUE(processor->try_push(2, 3.3), "second push");
    c.EXPECT_TRUE(processor->try_push(2, 3.4), "third push");
    c.EXPECT_TRUE(processor->try_push(2, 4.2), "4 push");
    c.EXPECT_TRUE(processor->try_push(2, 4.3), "5 push");
    c.EXPECT_TRUE(processor->try_push(2, 4.4), "6 push");
    c.EXPECT_TRUE(processor->try_subscribe(2, consumer), "subscribe");

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    c.EXPECT_EQ(consumer.consumed_amount(), 6ul, "consumed amount");
}

void many_ids_one_sub()
{
    TestChecker c("many_ids_one_sub");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_subscribe(2, consumer), "subscribe");
    c.EXPECT_TRUE(processor->try_push(2, 3.2), "first push");
    c.EXPECT_TRUE(processor->try_push(3, 3.3), "second push");
    c.EXPECT_TRUE(processor->try_push(4, 3.4), "third push");
    c.EXPECT_TRUE(processor->try_push(5, 4.2), "4 push");
    c.EXPECT_TRUE(processor->try_push(6, 4.3), "5 push");
    c.EXPECT_TRUE(processor->try_push(7, 4.4), "6 push");

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    c.EXPECT_EQ(consumer.consumed_amount(), 1ul, "consumed amount");
    c.EXPECT_TRUE(consumer.first_consumed() - 3.2 < 0.00001);
}

void overflow()
{
    TestChecker c("overflow");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>(5);

    c.EXPECT_TRUE(processor->try_push(1, 3.2), "first push");
    c.EXPECT_TRUE(processor->try_push(1, 3.3), "second push");
    c.EXPECT_TRUE(processor->try_push(1, 3.4), "third push");
    c.EXPECT_TRUE(processor->try_push(1, 4.2), "4 push");
    c.EXPECT_TRUE(processor->try_push(1, 4.3), "5 push");

    c.EXPECT_FALSE(processor->try_push(1, 4.4), "6 push");

    processor.reset(nullptr);
}

void multithreaded_producers()
{
    TestChecker c("multithreaded_producers");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_subscribe(1, consumer), "subscribe");
    MockProducer<int, double> prod1(*processor.get());
    MockProducer<int, double> prod2(*processor.get());
    prod1.produce({ 1 }, 1.6, 1000);
    prod2.produce({ 1 }, 1.7, 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);
    c.EXPECT_EQ(consumer.consumed_amount(), 2000ul, "consumed amount");
}

void two_consumers()
{
    TestChecker c("two_consumers");

    MockConsumer<int, double> consumer1;
    MockConsumer<int, double> consumer2;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_push(1, 3.2), "first push");
    c.EXPECT_TRUE(processor->try_push(1, 3.3), "second push");
    c.EXPECT_TRUE(processor->try_push(1, 3.4), "third push");
    c.EXPECT_TRUE(processor->try_push(2, 4.2), "4 push");
    c.EXPECT_TRUE(processor->try_push(2, 4.8), "5 push");
    c.EXPECT_TRUE(processor->try_push(3, 4.3), "6 push");
    c.EXPECT_TRUE(processor->try_subscribe(1, consumer1));
    c.EXPECT_TRUE(processor->try_subscribe(2, consumer2));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);

    c.EXPECT_EQ(consumer1.consumed_amount(), 3ul, "consumed amount 1");
    c.EXPECT_EQ(consumer2.consumed_amount(), 2ul, "consumed amount 2");
}

void subscribe_twice()
{
    TestChecker c("subscribe_twice");

    MockConsumer<int, double> consumer1;
    MockConsumer<int, double> consumer2;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_subscribe(1, consumer1), "sub 1");
    c.EXPECT_FALSE(processor->try_subscribe(1, consumer2), "sub 2");
}

void unsubscribe()
{
    TestChecker c("unsubscribe");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    c.EXPECT_TRUE(processor->try_subscribe(1, consumer));
    c.EXPECT_TRUE(processor->try_push(1, 1.1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor->unsubscribe(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    c.EXPECT_TRUE(processor->try_push(1, 1.1));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor.reset(nullptr);

    c.EXPECT_EQ(consumer.consumed_amount(), 1ul, "consumed amount");
}

void unsubscribe_with_no_sub()
{
    TestChecker c("unsubscribe_with_no_sub");

    MockConsumer<int, double> consumer;
    std::unique_ptr processor = std::make_unique<Processor<int, double>>();

    processor->unsubscribe(1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor.reset(nullptr);
}

void multithreaded_many_subscribers_many_producers()
{
    TestChecker c("multithreaded_many_subscribers_many_producers");

    std::unique_ptr processor = std::make_unique<Processor<std::string, std::string>>();
    MockProducer<std::string, std::string> prod1(*processor.get());
    MockProducer<std::string, std::string> prod2(*processor.get());

    MockConsumer<std::string, std::string> consumer1;
    MockConsumer<std::string, std::string> consumer2;
    MockConsumer<std::string, std::string> consumer3;
    void(processor->try_subscribe("one", consumer1));
    void(processor->try_subscribe("two", consumer2));
    void(processor->try_subscribe("three", consumer3));

    prod1.produce({ "one", "two", "three" }, "first_value", 10000);
    prod2.produce({ "one", "two" }, "second_value", 10000);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);

    size_t sum = consumer1.consumed_amount() + consumer2.consumed_amount() + consumer3.consumed_amount();
    c.EXPECT_EQ(sum, 20000ul);
}

bool g_copy_ctor_invoked = false;
struct TestValueType {
    TestValueType() = default;

    TestValueType(const TestValueType& v)
    {
        g_copy_ctor_invoked = true;
        m_value = v.m_value;
    }

    TestValueType& operator=(const TestValueType& v)
    {
        g_copy_ctor_invoked = true;
        m_value = v.m_value;
        return *this;
    }

    TestValueType(TestValueType&& v) = default;
    TestValueType& operator=(TestValueType&& v) = default;
    ~TestValueType() = default;

    int m_value = 0;
};

void no_copy_ctor_for_value()
{
    TestChecker c("no_copy_ctor_for_value");

    MockConsumer<int, TestValueType> consumer(false);
    std::unique_ptr processor = std::make_unique<Processor<int, TestValueType>>();

    c.EXPECT_TRUE(processor->try_push(1, TestValueType()));
    c.EXPECT_TRUE(processor->try_subscribe(1, consumer));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    processor.reset(nullptr);

    c.EXPECT_FALSE(g_copy_ctor_invoked, "copy ctor check");
}

int main()
{
    push_before_subscribe();
    push_after_subscribe();
    push_subscribe_push();
    many_ids_one_sub();
    overflow();
    multithreaded_producers();
    two_consumers();
    subscribe_twice();
    unsubscribe();
    unsubscribe_with_no_sub();
    multithreaded_many_subscribers_many_producers();
    no_copy_ctor_for_value();
    return g_some_test_failed ? 1 : 0;
}
