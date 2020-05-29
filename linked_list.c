#include <stdlib.h>
#include <stdio.h>
#include "linked_list.h"

// Constructor for the Node.
struct Node* constructNode(void *data){
  struct Node *temp = malloc(sizeof(struct Node));
  temp->data = data;
  temp->prev = NULL;
  temp->next = NULL;
  return temp;
}

// Constructor for the LinkedList.
struct LinkedList* constructList(){
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
int add(struct LinkedList *list, void *data){
  int size;
  struct Node *inserted = constructNode(data);
  pthread_mutex_lock(&list->mutex);
  if (list->_size == 0) {
    list->root = inserted;
    list->root->next = NULL;
    list->root->prev = NULL;
    list->tail = list->root;
  } else {
  inserted->next = list->root;
  list->root->prev = inserted;
  list->root = inserted;
  }

  list->_size++;
  size=list->_size;
  pthread_mutex_unlock(&list->mutex);
  return size;
}

int add_no_lock(struct LinkedList *list, void *data){
  int size;
  struct Node *inserted = constructNode(data);
  if (list->_size == 0) {
    list->root = inserted;
    list->root->next = NULL;
    list->root->prev = NULL;
    list->tail = list->root;
  } else {
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
void clear(struct LinkedList *list) {
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
int removeNode(struct LinkedList *list, void *data) {
  int size;
  pthread_mutex_lock(&list->mutex);
  struct Node *temp = list->root;
  while (temp != NULL){
    if (temp->data == data){
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

// Remove's the first Node whose data matches the parameter, if it exists.
int removeNode_no_lock(struct LinkedList *list, void *data){
  int size;
  struct Node *temp = list->root;
  while (temp != NULL){
    if (temp->data == data){
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
      size = list->_size;
      return size;
    }
    temp = temp->next;
  }
  return -1;
}

// Remove's the first Node in the LinkedList.
// O(1) complexity.
void removeFirst(struct LinkedList *list){
  Node* aux;
  pthread_mutex_lock(&(list->mutex));
  if (list->_size > 0){
    if (list->_size == 1){
      freeNode(list->root);
      list->root = NULL;
      list->tail = NULL;
      list->_size = 0;
    } else {
      aux=list->root;
      list->root = list->root->next;
      list->root->prev = NULL;
      list->_size--;
      freeNode(aux);
    }
  }
  pthread_mutex_unlock(&(list->mutex));
}

void destroy(struct LinkedList *list,void (*destroyDataFunc)(void* data)){
  if(&list->mutex!=NULL)
    pthread_mutex_destroy(&list->mutex);
  Node * current = list->root;
  Node * next = current;

  while(current != NULL){
    next = current->next;
    destroyDataFunc(current->data);
    free(current);
    current = next;
  }
  free(list);
}

/**
 * Name:               trasverse
 * Purpose:            Go throught the list, and make some operation given be
 *                     the second argument, function.
 * Inputs:
 *   Parameters:
 *      (LinkedList *) list - List to go throught
 *              (void) (*function) - operation to make in each element of the list
 *   Globals:          None
 * Outputs:
 *   Parameter:
 *            (void *) msg_array - messages to sent
 * Notes:              Due to the mutexes, syncronnization is ensured. This is
 *                     important if the operation changes values on the list.
 */
void trasverse(LinkedList* list,void* msg ,void(*function)(void*,void*)){
  pthread_mutex_lock(&list->mutex);
  Node * current = list->root;

  while(current != NULL){
    function(current->data,msg);
    current = current->next;
  }
  pthread_mutex_unlock(&list->mutex);
}

/**
 * Name:               trasverse_no_lock
 * Purpose:            Go throught the list, and make some operation given be
 *                     the second argument, function.
 * Inputs:
 *   Parameters:
 *      (LinkedList *) list - List to go throught
 *              (void) (*function) - operation to make in each element of the list
 *   Globals:          None
 * Outputs:
 *   Parameter:
 *            (void *) msg_array - messages to sent
 * Notes:              Syncronnization isn't ensured. This isn't a problem if
 *                     the operation doesn't change values of the list.
 */
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
    //verify if condition imposed by function is met
    if(function(current->data,condition)){
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
