/*
 * queue
 *
 *  Created on: Feb 20, 2018
 *      Author: andrei.tatar
 */

#ifndef QUEUE_
#define QUEUE_

#include <stddef.h>

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
	void push(T value);
private:
	volatile QueueLink<T> *head = NULL, *tail = NULL;
};


template<class T>
T Queue<T>::pop() {
	if (head == NULL) return NULL;
	
	auto oldHead = head;
	EnterCritical();
	head = head->next;
	if (head == NULL) {
		tail = NULL;
	}
	ExitCritical();
	
	const T value = oldHead->value;
	free((void*)oldHead);
	return value;
}

template<class T>
T Queue<T>::peek() {
	T headCopy = (T)head;
	return headCopy;
} 

template<class T>
void Queue<T>::push(T value) {
	auto add = (QueueLink<T>*)malloc(sizeof(QueueLink<T>));
	add->value = value;
	
	EnterCritical();
	if (tail != NULL) {
		tail->next = add;
		tail = add;
	} else {
		head = add;
		tail = add;
	}
	ExitCritical();
}

#endif /* QUEUE_ */
