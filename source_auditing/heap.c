#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define MAX_STRS 0x100
#define STRLEN_MAX 0x100
enum cmd { CREATE = 0x10, DELETE, FIND };
char *g_strs[MAX_STRS];
int n_strs= 0;

char *find(char *str)
{
    for(int i = 0; i < n_strs; i++) {
        if (!strcmp(str, g_strs[i])) return g_strs[i];
    }
    return NULL;
}

// can be called multiple times with user controlled command and c-string
char* str_store(enum cmd c, char *str)
{
    char *ret = NULL;
    switch (c) {
        case CREATE:
            if (n_strs < MAX_STRS) {
                g_strs[n_strs] = malloc(STRLEN_MAX);
                strcpy(g_strs[n_strs], str);
                ret = g_strs[n_strs++];
            }
            break;
        case DELETE:
            free(find(str));
            break;
        case FIND:
            ret = find(str);
            break;
    }

    return ret;
}

int main(void)
{
    printf("%s\n", str_store(CREATE, "FOO"));
    printf("%s\n", str_store(FIND, "FOO"));
    printf("%p\n", str_store(DELETE, "FOO"));

    return 0;
}

