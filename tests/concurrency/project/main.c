// SPDX-License-Identifier: Apache-2.0
// The unit that starts the threads and never sees the body they run. Only a project analysis,
// from the compilation database, relates the two units and finds the race on shared_counter.
#include <pthread.h>
#include <stddef.h>

void* worker(void* argument);

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
