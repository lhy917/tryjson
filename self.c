#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * strreplace(char const * const original, char const * const pattern, char const * const replacement) {
    size_t replen = strlen(replacement);
    size_t pattlen = strlen(pattern);
    size_t srclen = strlen(original);

    size_t pattcnt = 0;
    const char *src_ptr, *pattloc;

    printf("%s\n", original);
    printf("%s\n", pattern);
    printf("%s\n", replacement);
    for (src_ptr = original; (pattloc = strstr(src_ptr, pattern)); src_ptr = pattloc + pattlen) {
        pattcnt++;
    }
    printf("%ld\n", pattcnt);

    size_t resultsize = srclen + pattcnt * (pattlen - replen);
    char *result = (char*)malloc(sizeof(char) * (resultsize + 1));

    if (result != NULL) {
        char *res_ptr = result;

        for (src_ptr = original; (pattloc = strstr(src_ptr, pattern)); src_ptr = pattloc + pattlen) {
            size_t skip_len = pattloc - src_ptr;
            strncpy(res_ptr, src_ptr, skip_len);
            res_ptr += skip_len;
            strncpy(res_ptr, replacement, replen);
            res_ptr += replen;
        }
        strcpy(res_ptr, src_ptr);
    }
    return result;
}

int main(int argc, char const *argv[])
{
    char str1[1024]; 
	char str2[100],str3[100];	 	
   	
    gets(str1, 1024, stdin);
   	gets(str2, 100, stdin);
    gets(str3, 100, stdin);
	
	char* data=strreplace(str1, str2, str3);
	printf("%s\n", data);
	return 0;
}