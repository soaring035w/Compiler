int main()
{
    int a = 10;
    int b = 5;
    if (a + b)
    {
        a = a + 1;
    }
    else
    {
        b = 0;
    }
    while (b)
    {
        b = b - 1;
    }
    a = a * 2;
    a = b / 3;
    return a;
}