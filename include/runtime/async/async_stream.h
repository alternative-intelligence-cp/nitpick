// Async streams and generators
// Lazy async sequences with backpressure

#ifndef ARIA_RUNTIME_ASYNC_STREAM_H
#define ARIA_RUNTIME_ASYNC_STREAM_H

#include "runtime/async/future.h"
#include "runtime/async/executor.h"
#include <functional>
#include <vector>
#include <queue>
#include <optional>

namespace aria {
namespace runtime {

/**
 * Stream item - either value or end-of-stream
 */
template<typename T>
struct StreamItem {
    bool is_end;
    T value;
    
    StreamItem() : is_end(true) {}
    StreamItem(const T& v) : is_end(false), value(v) {}
    
    static StreamItem end() { return StreamItem(); }
};

/**
 * Async stream - lazy sequence of values
 * 
 * Similar to async iterators in JavaScript/Python.
 * Supports backpressure and cancellation.
 */
template<typename T>
class AsyncStream {
public:
    /**
     * Get next item from stream
     * @return Future<StreamItem<T>> - next item or end
     */
    virtual Future* next() = 0;
    
    /**
     * Close stream and release resources
     */
    virtual void close() = 0;
    
    virtual ~AsyncStream() = default;
};

/**
 * Stream from vector
 */
template<typename T>
class VectorStream : public AsyncStream<T> {
private:
    std::vector<T> items;
    size_t index;
    
public:
    VectorStream(const std::vector<T>& items)
        : items(items), index(0) {}
    
    Future* next() override {
        Future* future = new Future(sizeof(StreamItem<T>));
        
        if (index < items.size()) {
            StreamItem<T> item(items[index++]);
            future->setValue(&item, sizeof(item));
        } else {
            StreamItem<T> end = StreamItem<T>::end();
            future->setValue(&end, sizeof(end));
        }
        
        return future;
    }
    
    void close() override {
        index = items.size();
    }
};

/**
 * Stream from function (generator)
 */
template<typename T>
class GeneratorStream : public AsyncStream<T> {
private:
    std::function<Future*()> generator;
    bool closed;
    
public:
    GeneratorStream(std::function<Future*()> gen)
        : generator(gen), closed(false) {}
    
    Future* next() override {
        if (closed) {
            Future* future = new Future(sizeof(StreamItem<T>));
            StreamItem<T> end = StreamItem<T>::end();
            future->setValue(&end, sizeof(end));
            return future;
        }
        
        return generator();
    }
    
    void close() override {
        closed = true;
    }
};

/**
 * Buffered stream - buffers items for backpressure
 */
template<typename T>
class BufferedStream : public AsyncStream<T> {
private:
    AsyncStream<T>* source;
    std::queue<T> buffer;
    size_t buffer_size;
    bool source_ended;
    
public:
    BufferedStream(AsyncStream<T>* src, size_t size = 16)
        : source(src), buffer_size(size), source_ended(false) {}
    
    ~BufferedStream() {
        delete source;
    }
    
    Future* next() override {
        // If buffer has items, return immediately
        if (!buffer.empty()) {
            Future* future = new Future(sizeof(StreamItem<T>));
            StreamItem<T> item(buffer.front());
            buffer.pop();
            future->setValue(&item, sizeof(item));
            return future;
        }
        
        // If source ended, return end
        if (source_ended) {
            Future* future = new Future(sizeof(StreamItem<T>));
            StreamItem<T> end = StreamItem<T>::end();
            future->setValue(&end, sizeof(end));
            return future;
        }
        
        // Fetch from source
        return source->next();
    }
    
    void close() override {
        source->close();
        while (!buffer.empty()) {
            buffer.pop();
        }
    }
};

/**
 * Stream operators/combinators
 */

// Map - transform each item
template<typename T, typename U>
class MapStream : public AsyncStream<U> {
private:
    AsyncStream<T>* source;
    std::function<U(const T&)> mapper;
    
public:
    MapStream(AsyncStream<T>* src, std::function<U(const T&)> fn)
        : source(src), mapper(fn) {}
    
    ~MapStream() {
        delete source;
    }
    
    Future* next() override {
        Future* source_future = source->next();
        Future* result_future = new Future(sizeof(StreamItem<U>));
        
        // Poll source future for item, apply mapper, produce result
        if (source_future->isReady()) {
            StreamItem<T>* src_item = static_cast<StreamItem<T>*>(source_future->getValue());
            if (src_item && !src_item->is_end) {
                StreamItem<U> mapped(mapper(src_item->value));
                result_future->setValue(&mapped, sizeof(mapped));
            } else {
                StreamItem<U> end = StreamItem<U>::end();
                result_future->setValue(&end, sizeof(end));
            }
            delete source_future;
        } else {
            // Source not ready yet — poll until ready
            while (!source_future->isReady()) {
                source_future->poll();
            }
            StreamItem<T>* src_item = static_cast<StreamItem<T>*>(source_future->getValue());
            if (src_item && !src_item->is_end) {
                StreamItem<U> mapped(mapper(src_item->value));
                result_future->setValue(&mapped, sizeof(mapped));
            } else {
                StreamItem<U> end = StreamItem<U>::end();
                result_future->setValue(&end, sizeof(end));
            }
            delete source_future;
        }
        
        return result_future;
    }
    
    void close() override {
        source->close();
    }
};

// Filter - keep only matching items
template<typename T>
class FilterStream : public AsyncStream<T> {
private:
    AsyncStream<T>* source;
    std::function<bool(const T&)> predicate;
    
public:
    FilterStream(AsyncStream<T>* src, std::function<bool(const T&)> pred)
        : source(src), predicate(pred) {}
    
    ~FilterStream() {
        delete source;
    }
    
    Future* next() override {
        // Keep fetching until we find a matching item or reach end
        while (true) {
            Future* source_future = source->next();
            
            // Poll until ready
            while (!source_future->isReady()) {
                source_future->poll();
            }
            
            StreamItem<T>* src_item = static_cast<StreamItem<T>*>(source_future->getValue());
            if (!src_item || src_item->is_end) {
                // End of stream — pass through
                return source_future;
            }
            
            if (predicate(src_item->value)) {
                // Item matches filter — return it
                return source_future;
            }
            
            // Item doesn't match — discard and try next
            delete source_future;
        }
    }
    
    void close() override {
        source->close();
    }
};

// Take - take first N items
template<typename T>
class TakeStream : public AsyncStream<T> {
private:
    AsyncStream<T>* source;
    size_t limit;
    size_t count;
    
public:
    TakeStream(AsyncStream<T>* src, size_t n)
        : source(src), limit(n), count(0) {}
    
    ~TakeStream() {
        delete source;
    }
    
    Future* next() override {
        if (count >= limit) {
            Future* future = new Future(sizeof(StreamItem<T>));
            StreamItem<T> end = StreamItem<T>::end();
            future->setValue(&end, sizeof(end));
            return future;
        }
        
        count++;
        return source->next();
    }
    
    void close() override {
        source->close();
    }
};

/**
 * Stream utilities
 */

// Collect stream into vector
template<typename T>
Future* collect_stream(AsyncStream<T>* stream) {
    Future* future = new Future(sizeof(std::vector<T>));
    std::vector<T> result;
    
    // Drain stream into vector
    while (true) {
        Future* item_future = stream->next();
        while (!item_future->isReady()) {
            item_future->poll();
        }
        
        StreamItem<T>* item = static_cast<StreamItem<T>*>(item_future->getValue());
        if (!item || item->is_end) {
            delete item_future;
            break;
        }
        
        result.push_back(item->value);
        delete item_future;
    }
    
    future->setValue(&result, sizeof(result));
    return future;
}

// For each - execute function for each item
template<typename T>
Future* for_each_stream(AsyncStream<T>* stream, std::function<void(const T&)> fn) {
    Future* future = new Future(sizeof(bool));
    
    // Iterate stream and call fn for each item
    while (true) {
        Future* item_future = stream->next();
        while (!item_future->isReady()) {
            item_future->poll();
        }
        
        StreamItem<T>* item = static_cast<StreamItem<T>*>(item_future->getValue());
        if (!item || item->is_end) {
            delete item_future;
            break;
        }
        
        fn(item->value);
        delete item_future;
    }
    
    bool success = true;
    future->setValue(&success, sizeof(success));
    return future;
}

// Fold - reduce stream to single value
template<typename T, typename U>
Future* fold_stream(AsyncStream<T>* stream, U initial, std::function<U(U, const T&)> fn) {
    Future* future = new Future(sizeof(U));
    
    // Reduce stream to single value
    U accumulator = initial;
    while (true) {
        Future* item_future = stream->next();
        while (!item_future->isReady()) {
            item_future->poll();
        }
        
        StreamItem<T>* item = static_cast<StreamItem<T>*>(item_future->getValue());
        if (!item || item->is_end) {
            delete item_future;
            break;
        }
        
        accumulator = fn(accumulator, item->value);
        delete item_future;
    }
    
    future->setValue(&accumulator, sizeof(accumulator));
    return future;
}

/**
 * Stream builders
 */

// Range stream - numbers from start to end
class RangeStream : public AsyncStream<int64_t> {
private:
    int64_t current;
    int64_t end;
    int64_t step;
    
public:
    RangeStream(int64_t start, int64_t end, int64_t step = 1)
        : current(start), end(end), step(step) {}
    
    Future* next() override {
        Future* future = new Future(sizeof(StreamItem<int64_t>));
        
        if ((step > 0 && current < end) || (step < 0 && current > end)) {
            StreamItem<int64_t> item(current);
            current += step;
            future->setValue(&item, sizeof(item));
        } else {
            StreamItem<int64_t> end_item = StreamItem<int64_t>::end();
            future->setValue(&end_item, sizeof(end_item));
        }
        
        return future;
    }
    
    void close() override {
        current = end;
    }
};

// Repeat stream - repeat value N times
template<typename T>
class RepeatStream : public AsyncStream<T> {
private:
    T value;
    size_t count;
    size_t index;
    
public:
    RepeatStream(const T& val, size_t n)
        : value(val), count(n), index(0) {}
    
    Future* next() override {
        Future* future = new Future(sizeof(StreamItem<T>));
        
        if (index < count) {
            StreamItem<T> item(value);
            index++;
            future->setValue(&item, sizeof(item));
        } else {
            StreamItem<T> end = StreamItem<T>::end();
            future->setValue(&end, sizeof(end));
        }
        
        return future;
    }
    
    void close() override {
        index = count;
    }
};

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_ASYNC_STREAM_H
