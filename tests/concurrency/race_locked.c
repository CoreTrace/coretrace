// SPDX-License-Identifier: Apache-2.0
// The same two workers, with the increment under one mutex: nothing to report.
#include <pthread.h>
#include <stddef.h>

int counter = 0;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

static void* worker(void* argument)
{
    (void)argument;
    for (int i = 0; i < 1000; ++i)
    {
        pthread_mutex_lock(&counter_lock);
        counter = counter + 1;
        pthread_mutex_unlock(&counter_lock);
    }
    return NULL;
}

int main(void)
{
    pthread_t first;
    pthread_t second;

    pthread_create(&first, NULL, worker, NULL);
    pthread_create(&second, NULL, worker, NULL);

    pthread_join(first, NULL);
    pthread_join(second, NULL);
    return 0;
}
