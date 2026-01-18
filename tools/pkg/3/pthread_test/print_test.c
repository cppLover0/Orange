#include <stdio.h>
#include <stdlib.h>

int main() {
    char *env_var = getenv("zzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzxzzxzzxzzxzzxzzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzzxzvxzc"); 

    if (env_var != NULL) {
        printf("rrr: %s\n", env_var);
    } else {
        printf("fuck :(.\n");
    }
    
    return 0;
}