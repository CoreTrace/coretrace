int compute(int x)
{
    int y = x + 1;
    if (x > 0) {
        return x * 2;
    }
    return x * 3; // Code mort : jamais atteint car toutes les branches retournent avant.
}

int over_run_compute(int x)
{
    int y = x + 1;
    if (x > 0) {
        return x * 2; // Code mort : jamais atteint car toutes les branches retournent avant.
    }
    return x * 3;
}

int single_compute(void)
{
    int x = 5;

    if (x > 0) {
        return 2; // Code mort : jamais atteint car toutes les branches retournent avant.
    }
    return 3;
}

int main() {
    int a = 5;
    int b = compute(a);
    a = -5;
    int c = over_run_compute(a);
    int d = single_compute();
    return b + c + d;
}
