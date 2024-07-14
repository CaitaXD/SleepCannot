
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
            struct node *next;
        };
    	struct node *head;
	    struct node *tail;
    public:
		LockFreeQueue() {
			head = new struct node();
			tail = head;
			head->value = {};
			head->next = nullptr;
		}
        void enqueue(T value) {
	        struct node *n;
	        struct node *node =  new struct node();
	        node->value = value; 
	        node->next = NULL;
	        while (1) {
	        	n = tail;
	        	if (__sync_bool_compare_and_swap(&(n->next), nullptr, node)) {
	        		break;
	        	} else {
	        		__sync_bool_compare_and_swap(&(tail), n, n->next);
	        	}
	        }
	        __sync_bool_compare_and_swap(&(tail), n, node);
        }
        T* dequeue() {
			if (head == nullptr) {
				return nullptr;
			}
	        struct node *n;
	        T *val;
	        while (1) {
	        	n = head;
	        	if (n->next == NULL) {
                    return NULL;
	        	}
	        	if (__sync_bool_compare_and_swap(&(head), n, n->next)) {
	        		break;
	        	}
	        }
	        val = &n->next->value;
	        delete n;
	        return val;
        }
    };
}