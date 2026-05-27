#include "core/common/BlockingQueue.h"
#include <cassert>
#include <thread>
#include <iostream>

void test_basic_push_pop() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(42);
    int val;
    assert(q.pop(val, 100));
    assert(val == 42);
    std::cout << "  PASS: basic push/pop\n";
}

void test_fifo_order() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(1);
    q.push(2);
    q.push(3);
    int val;
    q.pop(val, 100); assert(val == 1);
    q.pop(val, 100); assert(val == 2);
    q.pop(val, 100); assert(val == 3);
    std::cout << "  PASS: FIFO order\n";
}

void test_max_size_backpressure() {
    BlockingQueue<int> q;
    q.setMaxSize(2);
    assert(q.push(1));
    assert(q.push(2));
    assert(q.isFull());
    assert(!q.push(3));
    std::cout << "  PASS: max size backpressure\n";
}

void test_timeout_pop() {
    BlockingQueue<int> q;
    int val;
    assert(!q.pop(val, 50));
    std::cout << "  PASS: timeout pop\n";
}

void test_concurrent_access() {
    BlockingQueue<int> q;
    q.setMaxSize(100);

    std::thread producer([&]() {
        for (int i = 0; i < 50; i++) {
            q.push(i);
        }
    });

    int sum = 0;
    std::thread consumer([&]() {
        for (int i = 0; i < 50; i++) {
            int val;
            if (q.pop(val, 1000)) {
                sum += val;
            }
        }
    });

    producer.join();
    consumer.join();
    assert(sum == (49 * 50) / 2);
    std::cout << "  PASS: concurrent access\n";
}

void test_clear() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(1);
    q.push(2);
    q.push(3);
    q.clear();
    assert(q.isEmpty());
    assert(q.size() == 0);
    std::cout << "  PASS: clear\n";
}

int main() {
    std::cout << "Running BlockingQueue tests...\n";
    test_basic_push_pop();
    test_fifo_order();
    test_max_size_backpressure();
    test_timeout_pop();
    test_concurrent_access();
    test_clear();
    std::cout << "All BlockingQueue tests passed!\n";
    return 0;
}
