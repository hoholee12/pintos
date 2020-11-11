#include <stdio.h>
#include <syscall.h>
#include <string.h>

int main(int argc, char* argv[]){
    printf("%d %d\n", fibonacci(atoi(argv[1])), 
    max_of_four_int(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4])));

    printf ("exec(\"no-such-file\"): %d\n", exec ("no-such-file"));

    return EXIT_SUCCESS;
}
