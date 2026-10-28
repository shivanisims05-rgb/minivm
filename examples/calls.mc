// Nested calls, argument order and discarding a result.
int twice(int x) { return x * 2; }
int add3(int a, int b, int c) { return a + b * 10 + c * 100; }
int noisy(int v) { print(v); return v; }

int main() {
    print(twice(twice(3)));
    print(add3(1, 2, 3));
    print(add3(noisy(7), noisy(8), noisy(9)));
    twice(5);
    return 0;
}
