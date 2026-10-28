// Block scopes: an inner x hides the outer x only inside its block.
int main() {
    int x = 1;
    {
        int x = 2;
        print(x);
    }
    print(x);
    if (x == 1) {
        int y = 10;
        print(y + x);
    } else {
        print(0);
    }
    return 0;
}
