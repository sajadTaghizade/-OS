#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// conver each char to int
int atoi_simple(char *s)
{
    int n = 0;
    while (is_digit(*s))
    {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        exit();

    int sum = 0;
    char *p = argv[1];

    // finish when input finished
    while (*p)
    {
        // ingnore none digit
        while (*p && !is_digit(*p))
            p++;

        if (*p == '\0')
            break;

        // conver each chars to int
        sum += atoi_simple(p);

        // go to end of big numbers
        while (*p && is_digit(*p))
            p++;
        }

    int fd;
    if ((fd = open("result.txt", O_CREATE | O_WRONLY)) < 0)
    {
        printf(2, "failed to open result.txt\n");
        exit();
    }

    printf(fd, "%d\n", sum);

    close(fd);
    exit();
}