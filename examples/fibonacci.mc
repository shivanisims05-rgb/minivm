// Fibonacci two ways: an iterative loop and a recursive function.
int fib_rec(int n) {
    if (n < 2) {
        return n;
    }
    return fib_rec(n - 1) + fib_rec(n - 2);
}

int main() {
    int a = 0;
    int b = 1;
    int i = 0;
    while (i < 10) {
        print(a);
        int t = a + b;
        a = b;
        b = t;
        i = i + 1;
    }
    print(fib_rec(10));
    return 0;
}
