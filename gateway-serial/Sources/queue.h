/*
 * queue
 *
 *  Created on: Feb 20, 2018
 *      Author: andrei.tatar
 */

#ifndef QUEUE_
#define QUEUE_

#include <stddef.h>
#include <stdlib.h>

template<class T>
struct QueueLink {
	QueueLink<T> *next;
	T value;
};

template<class T>
class Queue {
public:
	T pop();
	T peek();
	bool push(T value);
private:
	volatile QueueLink<T> *head = NULL, *tail = NULL;
};


template<class T>
T Queue<T>::pop() {
	if (head == NULL) {
		return NULL;
	}
	
	auto oldHead = (QueueLink<T>*)head;
	head = head->next;
	if (head == NULL) {
		tail = NULL;
	}
	
	T value = oldHead->value;
	free(oldHead);
	return value;
}

template<class T>
T Queue<T>::peek() {
	auto h = head;
	return h == NULL ? NULL : h->value;
} 

template<class T>
bool Queue<T>::push(T value) {
	auto add = (QueueLink<T>*)malloc(sizeof(QueueLink<T>));
	if (add == NULL) return false;
	add->next = NULL;
	add->value = value;
	
	if (tail != NULL) {
		tail->next = add;
		tail = add;
	} else {
		head = add;
		tail = add;
	}
	return true;
}

#endif /* QUEUE_ */
