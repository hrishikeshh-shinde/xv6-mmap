#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"

#include "wmap.h"
#define MMAPBASE 0x60000000 
#define PGSIZE 0x1000

int main() {

    int flags = (MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED);
    int fd = -1;
    // printf(1, "%d\n", flags);
        // printf(1, "flags: %d\n", flags);

    uint map =  wmap(MMAPBASE, PGSIZE,  flags,  fd);
    char *arr = (char *)map;

    arr[0] = 'a';

    printf(1, "arr[0] = %c\n", arr[0]);


    exit();
}

