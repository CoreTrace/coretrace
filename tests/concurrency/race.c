// SPDX-License-Identifier: Apache-2.0
// Two workers increment a global with no lock: a data race (CWE-362) the concurrency analyzer
// reports at the increment, with the conflicting access as a related location.
#include <pthread.h>
#include <stddef.h>

int counter = 0;

static void* worker(void* argument)
{
    (void)argument;
    for (int i = 0; i < 1000; ++i)
    {
        counter = counter + 1;
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
