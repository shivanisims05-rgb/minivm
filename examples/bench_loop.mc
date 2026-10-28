// Benchmark: a tight loop with arithmetic and a branch (3,000,000 iterations).
// Adds i mod 7 each time (written out as i - (i / 7) * 7 so the benchmark stays the same).
int main() {
    int i = 0;
    int s = 0;
    while (i < 3000000) {
        s = s + i - (i / 7) * 7;
        if (s > 10000000) {
            s = s - 10000000;
        }
        i = i + 1;
    }
    print(s);
    return 0;
}
