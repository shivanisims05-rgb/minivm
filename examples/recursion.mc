// Recursion 1000 levels deep.
int sum_to(int n) {
    if (n == 0) {
        return 0;
    }
    return n + sum_to(n - 1);
}

int main() {
    print(sum_to(1000));
    return 0;
}
