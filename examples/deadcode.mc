// With -O the optimiser also removes code that can never run:
// statements after return, if (0) branches and while (0) loops.
int pick(int x) {
    if (1) {
        return x * 2;
    }
    print(999);
    return 0;
}

int main() {
    if (0) { print(111); } else { print(222); }
    while (0) { print(333); }
    print(pick(21));
    return 0;
    print(444);
}
