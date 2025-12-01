/*
 * Hello World for NBOS
 * Demonstrates basic console output
 */

#include <nbos.h>

int main(void) {
    print("Hello, NBOS World!\n");
    print("Press any key to exit...\n");
    
    getkey();
    
    return 0;
}
