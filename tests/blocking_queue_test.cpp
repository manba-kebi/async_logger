#include "asynclogger/blocking_queue.h"

#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

#include "test_support.h"

namespace {

	using namespace std::chrono_literals;

	//测试设置队列大小为 0 时，是否能正确的改为 1
	void test_zero_capacity_becomes_one() {
		asynclogger::BlockingQueue<int> queue(0);
		test::require_equal(
			queue.capacity(),
			std::size_t{1},
			"zero capacity must be normalized to one"
		);
	}

	//测试该阻塞队列是否能正确的入队、出队元素（先进先出）
	void test_fifo_push_and_pop() {
		asynclogger::BlockingQueue<int> queue(3);

		test::require(queue.push(10),"first push must succeed");
		test::require(queue.push(20),"second push must succeed");
		test::require(queue.push(30),"third push must succeed");

		int value = 0;

		test::require(queue.pop(value),"first pop must succeed");		//将队列首个元素赋值给value，并弹出队首元素
		test::require_equal(value,10,"queue must preserve FIFO order");

		test::require(queue.pop(value),"second pop must succeed");
		test::require_equal(value,20,"queue must preserve FIFO order");

		test::require(queue.pop(value),"third pop must succeed");
		test::require_equal(value,30,"queue must preserve FIFO order");
	}

	//测试 try_push 在队列满时放入元素是否能正确返回 false
	void test_try_push_fails_when_full() {
		asynclogger::BlockingQueue<int> queue(1);

		test::require(queue.try_push(1),"first try_push must succeed");
		test::require(!queue.try_push(2),"try_push must fail immediately when queue is full");
	}

	void test_close_rejects_new_items() {
		asynclogger::BlockingQueue<int> queue(2);
		queue.close();

		test::require(queue.closed(),"queue must report closed state");
		test::require(!queue.push(1),"push after close must fail");
		test::require(!queue.try_push(1),"try_push after close must fail");
	}

	//测试队列关闭后，能不能正确的排出元素，以及队列空之后排除元素能否返回 false
	void test_close_drains_existing_items() {
		asynclogger::BlockingQueue<int> queue(2);

		test::require(queue.push(7),"push before close must succeed");
		test::require(queue.push(8),"push before close must succeed");
		queue.close();

		int value = 0;

		test::require(queue.pop(value),"first queued item must still be readable");
		test::require_equal(value,7,"first drained item is incorrect");

		test::require(queue.pop(value),"second queued item must still be readable");
		test::require_equal(value,8,"second drained item is incorrect");

		test::require(!queue.pop(value),"pop must return false after closed queue is drained");
	}

	//测试关闭队列后，唤醒消费者，消费者能否正常工作
	void test_close_wakes_blocked_consumer() {
		asynclogger::BlockingQueue<int> queue(1);
		std::atomic<bool> pop_returned{false};
		std::atomic<bool> pop_result{true};

		std::thread consumer([&] {
			int value = 0;
			pop_result.store(queue.pop(value));
			pop_returned.store(true);
		});

		std::this_thread::sleep_for(20ms);		//只是用来给新线程一个机会进入阻塞状态。
		queue.close();
		consumer.join();		//把 join 放在程序中间，那么当前线程（比如主线程）就会在这个位置“卡住”，一直等到子线程跑完，才能继续执行 join 后面的代码。
		//等待consume执行完成

		test::require(pop_returned.load(),"blocked consumer must wake after close");
		test::require(!pop_result.load(),"consumer must receive false when closed queue is empty");
	}

	//测试队列满后，关闭队列，生产者放入元素能否返回 false
	void test_close_wakes_blocked_producer() {
		asynclogger::BlockingQueue<int> queue(1);
		test::require(queue.push(1),"initial push must fill the queue");	//制造“满队列阻塞”的前提

		std::atomic<bool> push_returned{false};
		std::atomic<bool> push_result{true};

		std::thread producer([&] {
			push_result.store(queue.push(2));
			//如果是因为队列满的原因并不会返回值，而是该线程会一直卡死在这一句
			//现在队列满的情况下，只有当close_为true时才会返回false给push_result
			push_returned.store(true);
		});

		std::this_thread::sleep_for(20ms);
		queue.close();
		producer.join();

		test::require(push_returned.load(),"blocked producer must wake after close");
		test::require(!push_result.load(),"blocked push must fail when queue closes");
		//在这个测试里，只要 push_result 变成了 false，就铁证如山地证明了 close() 成功唤醒了阻塞线程，并且线程正确地识别到了“关闭”状态。
	}

	//测试生产者与消费者能否正常生产日志放入队列，从队列中取出日志消费日志
	void test_multiple_producers_do_not_lose_items() {
		constexpr int kProducerCount = 4;
		constexpr int kItemsPerProducer = 500;
		constexpr int kExpectedCount = kProducerCount * kItemsPerProducer;

		asynclogger::BlockingQueue<int> queue(64);
		std::atomic<int> consumed_count{0};

		std::thread consumer([&] {
			int value = 0;
			while (queue.pop(value)) {
				consumed_count.fetch_add(1);
			}
		});

		std::vector<std::thread> producers;
		producers.reserve(kProducerCount);

		for (int producer_index = 0;producer_index<kProducerCount;producer_index++) {
			producers.emplace_back([&,producer_index] {
				for (int item_index = 0;item_index<kItemsPerProducer;item_index++) {
					const int value = producer_index*kItemsPerProducer+item_index;

					if (!queue.push(value)) {
						return;
					}
				}
			});
		}

		for (auto& producer : producers) {
			producer.join();
		}

		queue.close();
		consumer.join();

		test::require_equal(consumed_count.load(),kExpectedCount,"all produced items must be consumed");
		test::require_equal(queue.size(),std::size_t{0},"queue must be empty after consumer exits");

	}

}	//namespace


int main() {
	int failures = 0;

	failures += test::run("zero capacity becomes one",test_zero_capacity_becomes_one);
	failures += test::run("FIFO push and pop",test_fifo_push_and_pop);
	failures += test::run("try_push fails when full",test_try_push_fails_when_full);
	failures += test::run("close rejects new items",test_close_rejects_new_items);
	failures += test::run("close drains existing items",test_close_drains_existing_items);
	failures += test::run("close wakes blocked consumer",test_close_wakes_blocked_consumer);
	failures += test::run("close wakes blocked producer",test_close_wakes_blocked_producer);
	failures += test::run("multiple producers do not lose items",test_multiple_producers_do_not_lose_items);

	return failures == 0?0:1;
}