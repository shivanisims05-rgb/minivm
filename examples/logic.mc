// && || ! with short-circuit evaluation. noisy(v) prints v, so you can see
// which operands were actually evaluated.
int noisy(int v) { print(v); return v; }
int main() {
    print(noisy(0) && noisy(5));   // right side skipped
    print(noisy(1) && noisy(5));   // both evaluated
    print(noisy(1) || noisy(5));   // right side skipped
    print(noisy(0) || noisy(7));   // both evaluated
    print(!0);
    print(!5);
    print(1 && 2);
    print(0 || 0);
    print(1 || 0 && 0);            // && binds tighter than ||
    print((1 || 0) && 0);
    print(2 < 3 && 3 < 4);
    print(5 > 3 && !(2 == 2));
    int x = 4;
    if (x > 0 && x < 10 || x == 100) { print(1); } else { print(0); }
    if (!(x % 2)) { print(1); } else { print(0); }
    return 0;
}
