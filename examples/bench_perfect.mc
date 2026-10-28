// Benchmark: perfect numbers below 500 (about 1.9 million VM instructions), using for, %, && and ||.
int is_prime(int n) {
    if (n < 2) { return 0; }
    for (int d = 2; d * d <= n; d = d + 1) {
        if (n % d == 0) { return 0; }
    }
    return 1;
}
int is_perfect(int n) {
    int sum = 0;
    for (int d = 1; d < n; d = d + 1) {
        if (n % d == 0) { sum = sum + d; }
    }
    return n > 1 && sum == n;
}
int main() {
    for (int i = 2; i < 30; i = i + 1) {
        if (is_prime(i)) { print(i); }
    }
    print(0 - 1);                   // separator
    for (int i = 1; i < 500; i = i + 1) {
        if (is_perfect(i)) { print(i); }
    }
    return 0;
}
