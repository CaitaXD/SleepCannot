
#include <atomic>
#include <memory>

namespace Concurrent
{
    template <typename T>
    class LockFreeQueue
    {
    private:
        struct node
        {
            T value;
            std::atomic<node*> next;
    		node(T value) : value(value), next(nullptr) {}
        };
		std::atomic<node*> head;
		std::atomic<node*> tail;
    public:
		LockFreeQueue() {
			struct node* node = new struct node(T());
 			head.store(node, std::memory_order_relaxed);
    		tail.store(node, std::memory_order_relaxed);
		}
        void enqueue(T value) {
    		struct node* node = new struct node(value);
    		struct node* prevNode = tail.exchange(node, std::memory_order_acq_rel);
    		prevNode->next.store(node, std::memory_order_relaxed);
        }
    	bool dequeue(T& result)
		{
		    node* theHead = head.load(std::memory_order_relaxed);
		    node* theNext = theHead->next.load(std::memory_order_acq_rel);
		    if (theNext != nullptr){
		        result = theNext->value;
		        head.store(theNext, std::memory_order_release);
		        delete theHead;
		        return true; 
		    }
		    return false;
		}
		bool peek(T& result)
		{
			node* theHead = head.load(std::memory_order_relaxed);
			node* theNext = theHead->next.load(std::memory_order_acquire);
			if (theNext != nullptr)
			{
				result = theNext->value;
				return true;
			}
			return false;
		}
	};
}