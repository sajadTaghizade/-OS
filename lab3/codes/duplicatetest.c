#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{

    char *filename = argv[1];
    int ans = make_duplicate(filename);

    if (ans == 0)
    {
        printf(1, "make_duplicate: Success(in user program)\n");
    }
    else if (ans == -1)
    {
        printf(1, "duplicatetest:FAILED Source file not found \n");
    }
    else
    {
        printf(1, "duplicatetest: FAILED\n");
    }

    exit();
}