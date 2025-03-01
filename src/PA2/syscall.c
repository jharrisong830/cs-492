#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main() {
    char str1[] = "This string is going to throw an error, I can feel it...";
    printf("str1 before: %s\n", str1);
    int result1 = syscall(451, str1); // our syscall is 451!
    printf("syscall result: %d\n", result1); // should be -1, str1 is too long and throws error
    printf("str1 after: %s\n", str1); // should be unchanged
    
    char str2[] = "This will work for sure!";
    printf("str2 before: %s\n", str2);
    int result2 = syscall(451, str2);
    printf("syscall result: %d\n", result2); // should be 6 (6 vowels replaced)
    printf("str2 after: %s\n", str2); // should be altered, with all vowels replaced with 'j'


    return 0;
}
