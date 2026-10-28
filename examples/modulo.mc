// The % operator. The sign of the result follows the left operand, as in C.
int collatz(int n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) { n = n / 2; } else { n = 3 * n + 1; }
        steps = steps + 1;
    }
    return steps;
}
int digit_sum(int n) {
    int s = 0;
    while (n > 0) { s = s + n % 10; n = n / 10; }
    return s;
}
int reverse(int n) {
    int r = 0;
    while (n > 0) { r = r * 10 + n % 10; n = n / 10; }
    return r;
}
int gcd(int a, int b) {
    while (b != 0) { int t = a % b; a = b; b = t; }
    return a;
}
int main() {
    print(17 % 5);
    print(-17 % 5);
    print(17 % -5);
    print(100 % 7);
    print(6 % 3);
    print(collatz(27));
    print(digit_sum(12345));
    print(reverse(1234));
    print(gcd(48, 18));
    print(gcd(1071, 462));
    return 0;
}
