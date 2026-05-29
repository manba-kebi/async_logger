#pragma once
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <utility>

namespace asynclogger {
	template <typename T>
	class BlockingQueue {
	//和 queue 的区别在于：BlockingQueue 是线程安全的“阻塞”队列，而 std::queue 只是一个裸的数据容器。
	private:
		mutable std::mutex mutex_;
		std::condition_variable not_empty_;
		std::condition_variable not_full_;
		std::queue<T> queue_;
		std::size_t capacity_{1};
		bool closed_{ false };
	public:
		explicit BlockingQueue(std::size_t capacity) :capacity_(capacity == 0 ? 1 : capacity) {}

		BlockingQueue(const BlockingQueue&) = delete;
		BlockingQueue& operator=(const BlockingQueue&) = delete;

		bool push(T item) {
			std::unique_lock<std::mutex> lock(mutex_);
			not_full_.wait(lock, [this] {
				return closed_ || queue_.size() < capacity_;
				});

			if (closed_) {
				return false;
			}

			queue_.push(std::move(item));
			lock.unlock();
			not_empty_.notify_one();
			return true;
		}

		bool try_push(T item) {
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (closed_ || queue_.size() >= capacity_) {
					return false;
				}

				queue_.push(std::move(item));
			}
			not_empty_.notify_one();
			return true;
		}

		bool pop(T& out) {
			std::unique_lock<std::mutex> lock(mutex_);
			not_empty_.wait(lock, [this] {
				return closed_ || !queue_.empty();
				});

			if (queue_.empty()) {
				return false;
			}

			out = std::move(queue_.front());
			queue_.pop();

			lock.unlock();
			not_full_.notify_one();
			return true;
		}

		void close() {
			{
				std::lock_guard<std::mutex> lock(mutex_);
				closed_ = true;
			}

			not_empty_.notify_all();
			not_full_.notify_all();
		}

		bool closed() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return closed_;
		}

		std::size_t size() const {
			std::lock_guard < std::mutex > lock(mutex_);
			return queue_.size();
		}

		std::size_t capacity() const noexcept {
			return capacity_;
		}
	};
}