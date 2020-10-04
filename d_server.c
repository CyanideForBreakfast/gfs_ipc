#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

int d_server_id;
int d_server_mq;

int main(int argc, char* argv[]){
	d_server_id = atoi(argv[1]);
	d_server_mq = msgget(ftok("./d_server.c",99),0);

	//printf("%d d_server started\n",d_server_id);
	printf("%d d_server D_SERVER %d\n",d_server_id, d_server_mq);
	return 0;
}
