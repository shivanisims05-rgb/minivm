// Primes below 50. There is no % operator, so n mod d is n - (n / d) * d.
int is_prime(int n) {
    if (n < 2) {
        return 0;
    }
    int d = 2;
    while (d * d <= n) {
        if (n - (n / d) * d == 0) {
            return 0;
        }
        d = d + 1;
    }
    return 1;
}

int main() {
    int n = 2;
    while (n < 50) {
        if (is_prime(n)) {
            print(n);
        }
        n = n + 1;
    }
    return 0;
}
