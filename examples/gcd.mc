// Greatest common divisor by repeated subtraction.
int gcd(int a, int b) {
    while (a != b) {
        if (a < b) {
            b = b - a;
        } else {
            a = a - b;
        }
    }
    return a;
}

int main() {
    print(gcd(48, 18));
    print(gcd(17, 5));
    print(gcd(100, 75));
    return 0;
}
