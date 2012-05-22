/**

== MIT-License

Copyright (c) 2012 David Sinuela Pastor <siu.4coders@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "threadpool.h"

ThreadPool::ThreadPool(int _maxThreads)
{
	maxThreads = _maxThreads;
	if (maxThreads < 1) maxThreads=1;

	incompleteWork = 0;

	if (pthread_mutex_init(&mutexQueue, NULL))
		printf("ERROR: Error initializing mutexQueue\n");

	if (pthread_mutex_init(&mutexWorkCompletion, NULL))
		printf("ERROR: Error initializing mutexWorkCompletion\n");

	if (pthread_cond_init(&incompleteWorkCond, NULL))
		printf("ERROR: Error initializing incompleteWorkCond\n");

	if (sem_init(&availableWork, 0, 0))
		printf("ERROR: Error initializing availableWork\n");

	initializeThreads();
}

ThreadPool::~ThreadPool()
{
	pthread_mutex_lock(&mutexQueue);
		std::vector<pthread_t*>::iterator it;
		for (it = threads.begin(); it != threads.end(); it++ ) {
			if (*it) {
				pthread_cancel(*(*it));
				pthread_join(*(*it), NULL);
				//pthread_detach(*(*it));
				delete (*it);
			}
		}
		threads.clear();
	pthread_mutex_unlock(&mutexQueue);

	pthread_mutex_destroy(&mutexQueue);
	pthread_mutex_destroy(&mutexWorkCompletion);
	pthread_cond_destroy(&incompleteWorkCond);

	sem_destroy(&availableWork);
}

void ThreadPool::initializeThreads()
{
	threads.resize(maxThreads);
	for (int i = 0; i < maxThreads; ++i) {
		pthread_t* thread = static_cast<pthread_t*>(::operator new(sizeof(pthread_t)));
		int ret =  pthread_create(thread, NULL,
					&ThreadPool::threadExecute,
					static_cast<void*>(this) );
		if (ret) {
			printf("ERROR: cannot create thread\n");
			continue;
		}
		threads.push_back(thread);
	}
}

void ThreadPool::waitEnd()
{
	pthread_mutex_lock(&mutexWorkCompletion);
		while (incompleteWork > 0) {
			pthread_cond_wait(&incompleteWorkCond, &mutexWorkCompletion);
		}
	pthread_mutex_unlock(&mutexWorkCompletion);
}

bool ThreadPool::enqueueWork(ThreadPoolWorker* workerThread)
{
	pthread_mutex_lock(&mutexQueue);
		assert(workerThread);
		workerQueue.push(workerThread);
		pthread_mutex_lock(&mutexWorkCompletion);
			incompleteWork++;
		pthread_mutex_unlock(&mutexWorkCompletion);
	pthread_mutex_unlock(&mutexQueue);

	sem_post(&availableWork);

	return true;
}

bool ThreadPool::fetchWork(ThreadPoolWorker** workerArg)
  /// Block the current thread (on availableWork) until some work is
  /// available, then extract it from the queue and return it in workerArg
{
	sem_wait(&availableWork);

	pthread_mutex_lock(&mutexQueue);
		*workerArg = workerQueue.front();
		workerQueue.pop();
	pthread_mutex_unlock(&mutexQueue);

	return true;
}

void* ThreadPool::threadExecute(void* _pool)
{
	ThreadPoolWorker* worker = NULL;
	ThreadPool* pool = static_cast<ThreadPool*>(_pool);

	while (pool->fetchWork(&worker)) {
		assert(worker);
		(*worker)();
		delete worker;

		pthread_mutex_lock(&(pool->mutexWorkCompletion));
			pool->incompleteWork--; // This work unit is complete
		pthread_mutex_unlock(&(pool->mutexWorkCompletion));

		if (pool->incompleteWork == 0) {
			// incompleteWork reached 0: Signal waitEnd cond_wait
			pthread_cond_signal(&(pool->incompleteWorkCond));
		}
	}
	return 0;
}
