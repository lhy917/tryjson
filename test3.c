#include<stdio.h>
#include<string.h>
#include <stdlib.h>
 
char *replace(char *src, char *sub, char *dst)
{
    int pos = 0;
    int offset = 0;
    int srcLen, subLen, dstLen;
    char *pRet = NULL;
 
    srcLen = strlen(src);
    subLen = strlen(sub);
    dstLen = strlen(dst);
    pRet = (char *)malloc(srcLen + dstLen - subLen + 1);//(外部是否该空间)
    if (NULL != pRet)
    {
        pos = strstr(src, sub) - src;
        printf("%d", pod)
        memcpy(pRet, src, pos);
        offset += pos;
        memcpy(pRet + offset, dst, dstLen);
        offset += dstLen;
        memcpy(pRet + offset, src + pos + subLen, srcLen - pos - subLen);
        offset += srcLen - pos - subLen;
        *(pRet + offset) = '\0';
    }
    return pRet;
}

int main()
{
 
   	char str1[1024]; 
	char str2[100],str3[100];	 	
   	
    gets(str1);
   	gets(str2);
    gets(str3);
	
    char *result = replace(str1, str2, str3);
	printf("%s",result);        
}