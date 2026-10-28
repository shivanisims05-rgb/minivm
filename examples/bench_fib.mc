// Benchmark: naive recursive Fibonacci (about 400,000 calls).
int fib(int n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    print(fib(27));
    return 0;
}
