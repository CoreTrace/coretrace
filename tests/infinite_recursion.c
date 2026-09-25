// SPDX-License-Identifier: Apache-2.0
// infinite_recursion.c: a self call with no base case overflows the stack at run time.

int countdown(int n)
{
    return countdown(n - 1) + 1;
}

int main(void)
{
    return countdown(3);
}
