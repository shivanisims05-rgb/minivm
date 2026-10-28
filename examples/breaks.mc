// break leaves the nearest loop, continue skips to the next round.
// In a for loop, continue still runs the step (i = i + 1).
int first_divisor(int n) {
    for (int d = 2; d < n; d = d + 1) {
        if (n % d == 0) { return d; }
    }
    return n;
}

int main() {
    // odd numbers below 10, using continue
    for (int i = 0; i < 10; i = i + 1) {
        if (i % 2 == 0) { continue; }
        print(i);
    }
    // stop at the first multiple of 7 above 20, using break
    int n = 21;
    while (1) {
        if (n % 7 == 0) { break; }
        n = n + 1;
    }
    print(n);
    // break only leaves the inner loop
    int total = 0;
    for (int a = 1; a <= 3; a = a + 1) {
        for (int b = 1; b <= 10; b = b + 1) {
            if (b > a) { break; }
            total = total + 1;
        }
    }
    print(total);
    print(first_divisor(91));
    print(first_divisor(13));
    return 0;
}
