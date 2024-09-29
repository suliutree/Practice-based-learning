#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"


int main() {

    FILE* file = fopen("../generate_data/data.txt", "r");
    if (!file) {
        fprintf(stderr, "Error opening data.txt\n");
        return 1;
    }

    ht_hash_table* ht = ht_new();

    char line[256];
    char key[128];
    char value[128];

    const int data_count = 400;

    char** keys = malloc(data_count * sizeof(char*));
    char** values = malloc(data_count * sizeof(char*));
    int count = 0;

    while (fgets(line, sizeof(line), file) && count < data_count) {
        if (sscanf(line, "%s %s", key, value) == 2) {
            ht_insert(ht, key, value);

            keys[count] = strdup(key);
            values[count] = strdup(value);
            count++;
        }
    }
    
    fclose(file);

    // Test searching for all keys
    for (int i = 0; i < count; ++i) {
        char* val = ht_search(ht, keys[i]);
        if (val == NULL) {
            printf("Error: Key '%s' not found in hash table.\n", keys[i]);
        } else if (strcmp(val, values[i]) != 0) {
            printf("Error: Value mismatch for key '%s': excepted '%s', got '%s'\n", keys[i], values[i], val);
        }

        if (i == 100 || i == 200) {
            printf("before update index: %d, key: %s, value: %s\n", i, keys[i], val);
        }
    }

    printf("hash table size: %d, count: %d\n", ht->size, ht->count);

    // Test updating existing keys
    for (int i = 0; i < count; ++i) {
        char new_value[128];
        sprintf(new_value, "new_value_%d", i);
        ht_insert(ht, keys[i], new_value);
    }

    // Verify that values have been updated
    for (int i = 0; i < count; ++i) {
        char* val = ht_search(ht, keys[i]);
        if (val == NULL) {
            printf("Error: Key '%s' not found after updating.\n", keys[i]);
        } else if (strcmp(val, values[i]) == 0) {
            printf("Error: Value for key '%s' was not updated.\n", keys[i]);
        } else if (strncmp(val, "new_value_", 10) != 0) {
            printf("Error: Unexpected value for key '%s' after updating: '%s'\n", keys[i], val);
        }

        if (i == 100 || i == 200) {
            printf("after update index: %d, key: %s, value: %s\n", i, keys[i], val);
        }
    }

    // Test deleting every second key
    for (int i = 0; i < count; i += 2) {
        ht_delete(ht, keys[i]);
    }

    // Verify that deleted keys are not found and others are intact
    for (int i = 0; i < count; ++i) {
        char* val = ht_search(ht, keys[i]);
        if (i % 2 == 0) {
            // This key was deleted
            if (val != NULL) {
                printf("Error: Deleted key '%s' still found with value '%s'\n", keys[i], val);
            }
        } else {
            // Key should still be present
            if (val == NULL) {
                printf("Error: Key '%s' should be present but not found.\n", keys[i]);
            } else if (strcmp(val, values[i]) == 0) {
                printf("Error: Value for key '%s' was not updated correctly.\n", keys[i]);
            }
        }
    }

    // Attempt to delete non-existent keys
    for (int i = 0; i < 10; ++i) {
        char non_existent_key[128];
        sprintf(non_existent_key, "non_existent_key_%d", i);
        ht_delete(ht, non_existent_key);
    }

    // Test searching for non-existent keys
    for (int i = 0; i < 10; ++i) {
        char non_existent_key[128];
        sprintf(non_existent_key, "non_existent_key_%d", i);
        char* val = ht_search(ht, non_existent_key);
        if (val != NULL) {
            printf("Error: Non-existent key '%s' found with value '%s'\n", non_existent_key, val);
        }
    }

    // Clean up allocated memory
    for (int i = 0; i < count; ++i) {
        free(keys[i]);
        free(values[i]);
    }
    free(keys);
    free(values);

    ht_del_hash_table(ht);

    printf("Hash table tests completed.\n");


    return 0;
}