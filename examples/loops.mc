// for loops. The parser rewrites  for (init; cond; step) body  as
//   { init; while (cond) { body; step; } }
int main() {
    int s = 0;
    for (int i = 1; i <= 100; i = i + 1) { s = s + i; }
    print(s);                       // 5050

    int f = 1;
    for (int i = 1; i <= 10; i = i + 1) { f = f * i; }   // a new i: scopes are per loop
    print(f);                       // 3628800

    int t = 0;
    for (int i = 1; i <= 4; i = i + 1) {
        for (int j = 1; j <= 4; j = j + 1) { t = t + i * j; }
    }
    print(t);                       // (1+2+3+4) squared = 100

    int k = 5;
    for (; k > 0; ) { print(k); k = k - 1; }              // all three parts are optional

    int m;
    for (m = 10; m < 13; m = m + 1) { print(m); }         // assignment as the init part
    print(m);                       // 13
    return 0;
}
