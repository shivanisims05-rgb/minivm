// All six comparisons (built from just LT and EQ) and integer division.
int main() {
    print(3 < 5);   print(5 < 3);
    print(3 > 5);   print(5 > 3);
    print(3 <= 3);  print(4 <= 3);
    print(3 >= 3);  print(2 >= 3);
    print(3 == 3);  print(3 != 3);  print(3 != 4);
    print(7 / 2);
    print(-7 / 2);
    return 0;
}
