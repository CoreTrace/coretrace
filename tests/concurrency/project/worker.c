// SPDX-License-Identifier: Apache-2.0
// The unit that owns the shared counter and the thread body. On its own it is silent: nothing
// here shows that two threads run `worker`.
#include <stddef.h>

int shared_counter = 0;

void* worker(void* argument)
{
    (void)argument;
    for (int i = 0; i < 1000; ++i)
    {
        shared_counter = shared_counter + 1;
    }
    return NULL;
}
