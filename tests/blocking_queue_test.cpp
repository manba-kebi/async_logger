#include "asynclogger/blocking_queue.h"

#include <cassert>
#include <thread>
#include <vector>

void test_push_pop() {
	asynclogger::BlockingQueue<int> queue(2);
	assert(queue.push(42));
	
	int value = 0;
	assert(queue.pop(value));
	assert(value == 42);
}

void test_try_push_full() {
	asynclogger::BlockingQueue<int> queue(1);
	assert(queue.try_push(1));
	assert(!queue.try_push(2));
}

void test_close_empty_pop() {
	asynclogger::BlockingQueue<int> queue(1);
	queue.close();

	int value = 0;
	assert(!queue.pop(value));
}

void test_multi_thread_push_pop() {
	constexpr int kProducerCount = 4;
	constexpr int kItemsPerProducer = 500;
	constexpr int kExpected = kProducerCount * kItemsPerProducer;
	//constexpr 用来声明常量表达式，核心作用是告诉编译器“这个东西可以在编译期就算出来”。

	asynclogger::BlockingQueue<int> queue(64);
	int consumed = 0;

	std::thread consumer([&] {
		int value = 0;
		while(queue.pop(value)) {
			++consumed;
		}
		});

	std::vector<std::thread> producers;
	for (int i = 0; i < kProducerCount; ++i) {
		producers.emplace_back([&] {
			for (int n = 0; n < kItemsPerProducer; ++n) {
				assert(queue.push(n));
			}
			});
	}
	for (auto& producer : producers) {
		producer.join();
	}

	queue.close();
	consumer.join();
	
	assert(consumed == kExpected);
}

int main() {
	test_push_pop();
	test_try_push_full();
	test_close_empty_pop();
	test_multi_thread_push_pop();

	return 0;
}