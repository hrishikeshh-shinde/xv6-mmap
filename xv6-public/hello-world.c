#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"

#include "wmap.h"
#define MMAPBASE 0x60000000 

int main() {

    int length = 1000;
    int flags = (MAP_SHARED || MAP_ANONYMOUS || MAP_FIXED);
    int fd = -1;
    printf(1, "%d\n", flags);
    uint map =  wmap(MMAPBASE,  length,  flags,  fd);

    printf(1, "map : %d\n", map);
    exit();
}

