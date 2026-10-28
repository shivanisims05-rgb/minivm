// Used in the review demo: a function, a loop and a constant expression.
int square(int x) {
    return x * x;
}

int main() {
    int i = 1;
    int total = 0;
    while (i <= 5) {
        total = total + square(i);
        i = i + 1;
    }
    print(total);
    print(2 + 3 * 4);
    return 0;
}
