#ifndef LOCK_FREE_QUEUE_H_
#define LOCK_FREE_QUEUE_H_
/*
  Linked list implementation of a queue with atomic operations so only one thread can enqueue and dequeue at a time
*/
#include <atomic>
#include <memory>

// https://www.machinet.net/tutorial-eng/implement-custom-lock-free-queue-cpp-multi-threaded
namespace Concurrent
{
	template <typename T>
	class LockFreeQueue;

	template <typename T>
	class LockFreeQueueIterator
	{
		friend class LockFreeQueue<T>;
		friend struct LockFreeQueue<T>::node;
		LockFreeQueue<T> *queue;
		LockFreeQueue<T>::node *current;

	public:
		LockFreeQueueIterator(LockFreeQueue<T> *queue) : queue(queue), current(queue->head.load(std::memory_order_relaxed)) {}
		LockFreeQueueIterator(LockFreeQueue<T> *queue, LockFreeQueue<T>::node *current) : queue(queue), current(current) {}

		static LockFreeQueueIterator<T> begin(LockFreeQueue<T> *queue)
		{
			return LockFreeQueueIterator<T>(queue);
		}

		static LockFreeQueueIterator<T> end(LockFreeQueue<T> *queue)
		{
			return LockFreeQueueIterator<T>(queue, nullptr);
		}

		bool has_next()
		{
			return current != nullptr;
		}
		T next()
		{
			T result = current->value;
			current = current->next.load(std::memory_order_acquire);
			return result;
		}

		T &operator*()
		{
			return current->value;
		}

		T &operator->()
		{
			return current->value;
		}

		LockFreeQueueIterator<T> &operator++()
		{
			current = current->next.load(std::memory_order_acquire);
			return *this;
		}

		bool operator==(LockFreeQueueIterator<T> other)
		{
			return current == other.current;
		}

		bool operator!=(LockFreeQueueIterator<T> other)
		{
			return current != other.current;
		}

		LockFreeQueueIterator<T> find(T value)
		{
			LockFreeQueueIterator<T> it =  LockFreeQueueIterator<T>::begin(queue);
			LockFreeQueueIterator<T> end =  LockFreeQueueIterator<T>::end(queue);
			while (it != end)
			{
				if (*it == value)
				{
					return it;
				}
				++it;
			}
			return end;
		}
	};

	template <typename T>
	class LockFreeQueue
	{
	public:
		struct node
		{
			T value;
			std::atomic<node *> next;
			node(T value) : value(value), next(nullptr) {}
		};
		std::atomic<node *> head;
		std::atomic<node *> tail;

	public:
		LockFreeQueue()
		{
			struct node *node = new struct node(T());
			head.store(node, std::memory_order_relaxed);
			tail.store(node, std::memory_order_relaxed);
		}
		void enqueue(T value)
		{
			struct node *node = new struct node(value);
			struct node *prevNode = tail.exchange(node, std::memory_order_acq_rel);
			prevNode->next.store(node, std::memory_order_relaxed);
		}
		bool dequeue(T &result)
		{
			node *theHead = head.load(std::memory_order_relaxed);
			node *theNext = theHead->next.load(std::memory_order_acq_rel);
			if (theNext != nullptr)
			{
				result = theNext->value;
				head.store(theNext, std::memory_order_release);
				delete theHead;
				return true;
			}
			return false;
		}
		bool peek(T &result)
		{
			node *theHead = head.load(std::memory_order_relaxed);
			node *theNext = theHead->next.load(std::memory_order_acquire);
			if (theNext != nullptr)
			{
				result = theNext->value;
				return true;
			}
			return false;
		}
		bool empty()
		{
			T top;
			return !peek(top);
		}

		void remove(T value)
		{
			node *theHead = head.load(std::memory_order_relaxed);
			node *theNext = theHead->next.load(std::memory_order_acquire);
			while (theNext != nullptr)
			{
				if (theNext->value == value)
				{
					node *prevNode = theHead;
					node *nextNode = theNext->next.load(std::memory_order_acquire);
					if (prevNode->next.compare_exchange_strong(nextNode, nextNode, std::memory_order_acq_rel))
					{
						delete theNext;
					}
					return;
				}
				theHead = theNext;
				theNext = theHead->next.load(std::memory_order_acquire);
			}
		}

		auto begin() { return LockFreeQueueIterator<T>::begin(this); }
		auto end() { return LockFreeQueueIterator<T>::end(this); }
		auto find(T value) { return LockFreeQueueIterator<T>::begin(this).find(value); }
	};
}

#endif // LOCK_FREE_QUEUE_H_