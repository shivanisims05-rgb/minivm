// Operator precedence: * and / bind tighter than + and -.
int main() {
    print(2 + 3 * 4);
    print((2 + 3) * 4);
    print(100 / 7 - 3);
    print(-5 + 2);
    return 0;
}
