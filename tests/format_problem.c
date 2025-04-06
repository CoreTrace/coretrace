#include <stdio.h>

void print_input(char *input)
{
    printf(input);
}

int main()
{
    char user_input[100];
    printf("Entrez une chaîne : ");
    fgets(user_input, 100, stdin);
    print_input(user_input);
    return 0;
}
