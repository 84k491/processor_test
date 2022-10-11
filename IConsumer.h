#pragma once

template <typename KeyT, typename ValueT>
struct IConsumer {
    virtual void Consume(KeyT id, const ValueT& value) = 0;
};
