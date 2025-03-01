#include <linux/syscalls.h>

SYSCALL_DEFINE1(jgraham5_syscall, char __user *, chrArr) {
    if (chrArr == NULL) {
        return -1; // error if the userspace var is null
    }

    int len = strnlen_user(chrArr, 32); // gets the size of a userspace string
    if (len > 32 || len <= 0) {
        return -1; // error if the string is more than 32 bytes (including '\0') or if another error occurred (0 returned)
    }
    char newStr[len]; // initialize an empty buffer of len bytes
    if (strncpy_from_user(newStr, chrArr, len) == -EFAULT) { // copy the string from userspace to kernelspace newStr
        return -1; // return if error from strncpy
    }

    printk(KERN_ALERT "before: %s\n", newStr); // print the current string

    int changed = 0;
    for (int i = 0; i < len; i++) {
        if (newStr[i] == 'a' || newStr[i] == 'e' || newStr[i] == 'i' || newStr[i] == 'o' || newStr[i] == 'u' || newStr[i] == 'y') { // if char is a vowel...
            newStr[i] = 'j'; // ...change it to a 'j' (first letter of my login name)
            changed++;
        }
    }

    printk(KERN_ALERT "after: %s\n", newStr); // print the changed string

    if (copy_to_user(chrArr, newStr, len) != 0) { // copy the changed data back to the userspace variable
        return -1; // return if error from copy_to_user (should return 0 -> all bytes were copied)
    }

    return changed; // return val -> number of vowels changed
}
