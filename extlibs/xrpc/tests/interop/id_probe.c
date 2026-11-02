/* Line-pair probe used by tools/test_xrpc_id_semantics.py. Each input pair
 * contains two complete one-line numeric IDs; output is 1/0 for key equality. */
#include <xrpc.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
	char A[8192],B[8192];unsigned char Left[8192],Right[8192];size_t Count=0;
	while(fgets(A,sizeof(A),stdin)){
		if(!strchr(A,'\n') || !fgets(B,sizeof(B),stdin) || !strchr(B,'\n'))return 2;
		size_t L=0,R=0;
		if(!xrpcIdKeyWrite(Left,sizeof(Left),(xstrview){A,strlen(A)},NULL,&L) ||
			!xrpcIdKeyWrite(Right,sizeof(Right),(xstrview){B,strlen(B)},NULL,&R))return 3;
		if(puts(L==R && !memcmp(Left,Right,L)?"1":"0")==EOF)return 4;
		++Count;
	}
	return ferror(stdin) || !Count ? 5 : 0;
}
