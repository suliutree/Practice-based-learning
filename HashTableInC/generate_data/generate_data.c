#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    FILE* file = fopen("data.txt", "w");
    if (!file) {
        fprintf(stderr, "Error opening data.txt for writing.\n");
        return 1;
    }

    srand((unsigned int)time(NULL));

    for (int i = 0; i < 500; ++i) {
        char key[128];
        char value[128];

        sprintf(key, "key_%d", rand());
        sprintf(value, "value_%d", rand());
        fprintf(file, "%s %s\n", key, value);
    }

    fclose(file);

    return 0;
}