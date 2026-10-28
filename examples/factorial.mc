// Recursion: factorial of 1..10.
int fact(int n) {
    if (n < 2) {
        return 1;
    }
    return n * fact(n - 1);
}

int main() {
    int i = 1;
    while (i <= 10) {
        print(fact(i));
        i = i + 1;
    }
    return 0;
}
