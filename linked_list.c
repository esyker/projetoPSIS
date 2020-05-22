#include <stdlib.h>
#include <stdio.h>
#include "linked_list.h"

// Constructor for the Node.
struct Node* constructNode(void *data)
{
	struct Node *temp = malloc(sizeof(struct Node));
	temp->data = data;
	temp->prev = NULL;
	temp->next = NULL;
	return temp;
}

// Constructor for the LinkedList.
struct LinkedList* constructList()
{
	struct LinkedList *temp = malloc(sizeof(struct LinkedList));
	if (pthread_mutex_init(&temp->mutex, NULL) != 0) {
			free(temp);
			return NULL;
	}
	temp->root = NULL;
	temp->tail = NULL;
	temp->_size = 0;
	return temp;
}

// Inserts a Node at the front of the LinkedList.
int add(struct LinkedList *list, void *data)
{
	int size;
	struct Node *inserted = constructNode(data);
	pthread_mutex_lock(&list->mutex);
	if (list->_size == 0)
	{
		list->root = inserted;
		list->root->next = NULL;
		list->root->prev = NULL;
		list->tail = list->root;
	}
	else
	{
		inserted->next = list->root;
		list->root->prev = inserted;
		list->root = inserted;
	}

	list->_size++;
	size=list->_size;
	pthread_mutex_unlock(&list->mutex);
	return size;
}

int add_no_lock(struct LinkedList *list, void *data)
{
	int size;
	struct Node *inserted = constructNode(data);
	if (list->_size == 0)
	{
		list->root = inserted;
		list->root->next = NULL;
		list->root->prev = NULL;
		list->tail = list->root;
	}
	else
	{
		inserted->next = list->root;
		list->root->prev = inserted;
		list->root = inserted;
	}

	list->_size++;
	size=list->_size;
	return size;
}

void freeNode(Node* node){
	free(node->data);
	node->data=NULL;
	free(node);
}

// Clear the LinkedList of Nodes.
// O(n) complexity.
void clear(struct LinkedList *list)
{
	pthread_mutex_lock(&list->mutex);
  Node * current = list->root;
  Node * next = current;
  while(current != NULL){
    next = current->next;
		freeNode(current);
    current = next;
  }
  list->root = NULL;
	list->tail = NULL;
	list->_size = 0;
	pthread_mutex_unlock(&list->mutex);
}

// Remove's the first Node whose data matches the parameter, if it exists.
int removeNode(struct LinkedList *list, void *data)
{
	int size;
	pthread_mutex_lock(&list->mutex);
	struct Node *temp = list->root;
	while (temp != NULL)
	{
		if (temp->data == data)
		{
				if(temp==list->root){
					list->root=temp->next;
				}
				if(temp==list->tail){
					if(temp->next!=NULL)
						list->tail=temp->next;
					else
						list->tail=temp->prev;
				}
				if(temp->prev!=NULL){
					temp->prev->next = temp->next;
				}
				if(temp->next!=NULL){
					temp->next->prev = temp->prev;
				}
				free(temp);
				list->_size--;
				size=list->_size;
				pthread_mutex_unlock(&list->mutex);
				return size;
		}
		temp = temp->next;
	}
	pthread_mutex_unlock(&list->mutex);
	return -1;
}

// Remove's the first Node in the LinkedList.
// O(1) complexity.
void removeFirst(struct LinkedList *list)
{
	Node* aux;
	pthread_mutex_lock(&(list->mutex));
	if (list->_size > 0)
	{
		if (list->_size == 1){ //clear(list)
			freeNode(list->root);
		  list->root = NULL;
			list->tail = NULL;
			list->_size = 0;
		}
		else
		{
			aux=list->root;
			list->root = list->root->next;
			list->root->prev = NULL;
			list->_size--;
			freeNode(aux);
		}
	}
	pthread_mutex_unlock(&(list->mutex));
}

void trasverse(LinkedList* list,void* msg ,void(*function)(void*,void*)){
	pthread_mutex_lock(&list->mutex);

	Node * current = list->root;

	while(current != NULL){
		function(current->data,msg);
		current = current->next;
	}
	pthread_mutex_unlock(&list->mutex);
}

void trasverse_no_lock(LinkedList* list,void* msg ,void(*function)(void*,void*)){
	Node * current = list->root;

	while(current != NULL){
		function(current->data,msg);
		current = current->next;
	}
}

void* findby(LinkedList* list,int(*function)(void*,void*), void* condition){
	Node * aux;
	pthread_mutex_lock(&list->mutex);
	Node * current = list->root;
	while(current != NULL){
		if(function(current->data,condition))//verify nif condition imposed by function is met
		{
			aux=current->data;
			pthread_mutex_unlock(&list->mutex);
			return aux;
		}
		current = current->next;
	}
	pthread_mutex_unlock(&list->mutex);
	return NULL;
}

void custom_operation(LinkedList* list, void (*operation)(LinkedList* list)){
  pthread_mutex_lock(&(list->mutex));
  operation(list);
  pthread_mutex_unlock(&(list->mutex));
  return;
}


/*
void destroy(struct LinkedList *list)
{
  Node * current = list->root;
  Node * next = current;
  while(current != NULL){
    next = current->next;
    free(current);
    current = next;
  }
  free(list);
}*/

/*
// Prints out the LinkedList to the terminal window.
// O(n) complexity.
void print(struct LinkedList *list)
{
	printf("%c", '{');

	struct Node *temp = list->root;
	while (temp != NULL)
	{
		printf(" %s", temp->data);
		if (temp->next != NULL)
			printf("%c", ',');
		temp = temp->next;
	}

	printf(" }\n");
}*/

/*



// Inserts a Node at a given index inside the LinkedList.
int addAt(struct LinkedList *list, unsigned int index, void *data)
{
	if (index > list->_size)
	{
		fprintf(stderr, "%s%d%s%lu%c\n",
			"Error: Index Out of Bounds. Index was '",
			index, "\' but size is ", list->_size, '.');
		return 1;
	}
	else if (index == 0)
		add(list, data);
	else if (index == list->_size)
		addLast(list, data);
	else
	{
		struct Node *temp = find(list, index),
			*added = constructNode(data);
		pthread_mutex_lock(&added->mutex);
		pthread_mutex_lock(&temp->mutex);
		added->prev = temp->prev;
		added->next = temp;
		temp->prev->next = added;
		temp->prev = added;
		pthread_mutex_unlock(&temp->mutex);
		pthread_mutex_unlock(&added->mutex);
		list->_size++;
	}

	return 0;
}

// Inserts a Node at the end of the LinkedList.
// O(1) complexity.
void addLast(struct LinkedList *list, void *data)
{
	Node* aux;
	if (list->_size == 0)
		add(list, data);
	else
	{
		struct Node *added = constructNode(data);
		pthread_mutex_lock(&added->mutex);
		added->prev = list->tail;
		pthread_mutex_lock(&list->tail->mutex);
		list->tail->next = added;
		aux=list->tail;
		list->tail = added;
		list->_size++;
		pthread_mutex_unlock(&aux->mutex);
		pthread_mutex_unlock(&added->mutex);
	}
}
*/

/*

// Remove's the last Node in the LinkedList.
// O(1) complexity.
void removeLast(struct LinkedList *list)
{
	Node* aux;
	if (list->_size > 0)
	{
		if (list->_size == 1)
			clear(list);
		else
		{
			pthread_mutex_lock(&list->tail->mutex);
			pthread_mutex_lock(&list->tail->prev->mutex);
			aux=list->tail;
			list->tail = list->tail->prev;
			list->tail->next = NULL;
			list->_size--;
			pthread_mutex_unlock(&list->tail->mutex);
			freeNode(aux);
		}
	}
}

*/

/*

// Remove's a Node at a given index.
int removeAt(struct LinkedList *list, unsigned int index)
{
	if (index >= list->_size)
	{
		fprintf(stderr, "%s%d%s%lu%c\n",
			"Error: Index Out of Bounds. Index was '",
			index, "\' but size is ", list->_size, '.');
		return 1;
	}
	else if (index == 0)
		removeFirst(list);
	else if (index == list->_size - 1)
		removeLast(list);
	else
	{
		struct Node *temp = find(list, index);
		pthread_mutex_lock(&temp->mutex);
		temp->prev->next = temp->next;
		temp->next->prev = temp->prev;
		freeNode(temp);
		list->_size--;
	}

	return 0;
}

*/

/*

// Locates a Node within the LinkedList based on the index.
struct Node* find(struct LinkedList *list, unsigned int index)
{
	pthread_mutex_t* temp_mutex;
	if (index > list->_size)
		fprintf(stderr, "%s%d%s%lu%c\n",
			"Error: Index Out of Bounds. Index was '",
			index, "\' but size is ", list->_size, '.');
	else if (list->_size > 0)
	{
		if (index == 0)
			return list->root;
		else if (index == list->_size - 1)
			return list->tail;
		else
		{
			struct Node *temp;

			if ((double)index / list->_size > 0.5)
			{
				// Seek from the tail.
				pthread_mutex_lock(&list->tail->mutex);
				temp = list->tail;
				temp_mutex=&list->tail->mutex;
				for (unsigned int i = list->_size - 1; i > index; i--){
					pthread_mutex_lock(&temp->prev->mutex);
					temp = temp->prev;
					pthread_mutex_unlock(temp_mutex);
					temp_mutex=&temp->mutex;
				}
			}
			else
			{
				// Seek from the root.
				pthread_mutex_lock(&list->root->mutex);
				temp = list->root;
				temp_mutex=&list->root->mutex;
				for (unsigned int i = 0; i < index; i++){
					pthread_mutex_lock(&temp->next->mutex);
					temp = temp->next;
					pthread_mutex_unlock(temp_mutex);
					temp_mutex=&temp->mutex;
				}
				pthread_mutex_unlock(temp_mutex);
			}
			return temp;
		}
	}
	return NULL;
}

*/
