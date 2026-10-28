int f(int n) {
    return f(n + 1);
}
int main() {
    print(f(0));
    return 0;
}
