#include <pthread.h>

typedef struct Node
{
	void *data;
	struct Node *next, *prev;
}Node;

typedef struct LinkedList
{
	struct Node *root, *tail;
	size_t _size;
	pthread_mutex_t mutex;
}LinkedList;

struct Node* constructNode(void *data);
struct LinkedList* constructList();
int add(struct LinkedList *list, void *data);
int add_no_lock(struct LinkedList *list, void *data);
void clear(struct LinkedList *list);
int removeNode(struct LinkedList *list, void *data);
int removeNode_no_lock(struct LinkedList *list, void *data);
void removeFirst(struct LinkedList *list,void (*data_operation)(void* data));
void removeFirst_no_lock(struct LinkedList *list,void (*data_operation)(void* data));
void destroy(struct LinkedList *list,void (*destroyDataFunc)(void* data));
void trasverse(LinkedList* list,void* msg ,void(*function)(void*,void*));
void trasverse_no_lock(LinkedList* list,void* msg ,void(*function)(void*,void*));
void* findby(LinkedList* list,int(*function)(void*,void*), void* condition);
void custom_operation(LinkedList* list, void (*operation)(LinkedList* list));
