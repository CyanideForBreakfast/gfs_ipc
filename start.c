#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

#define SEM_NAME_CLIENT "CLIENT"
#define SEM_NAME_M_SERVER "M_SERVER"
#define SEM_NAME_D_SERVER "D_SERVER"

int m_server_mq, client_mq, d_server_mq;
char client_path[] = "/client.c"; char m_server_path[] = "/m_server.c"; char d_server_path[] = "/d_server.c";
	
void close_prog(int);
void close_prog(int sig){
	printf("\nClosing program ....\n");
	mq_unlink(client_path);
	mq_unlink(m_server_path);
	mq_unlink(d_server_path);
	mq_close(m_server_mq);
	mq_close(client_mq);
	mq_close(d_server_mq);
	system("ipcrm --all");
}

int main(){
	int num_d_servers;
	printf("Number of data servers: ");
	scanf("%d", &num_d_servers);

	//create shared semaphores
	sem_t* s_client = sem_open(SEM_NAME_CLIENT,O_CREAT | O_EXCL,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,1);
	sem_t* s_m_server = sem_open(SEM_NAME_M_SERVER,O_CREAT | O_EXCL,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,0);
	sem_t* s_d_server = sem_open(SEM_NAME_D_SERVER,O_CREAT | O_EXCL,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,0);

	// making sure client.c, m_server.c and d_server.c are compiled with appropriate names
	system("gcc -o client client.c -lpthread -lrt");
	system("gcc -o m_server m_server.c -lpthread -lrt");
	system("gcc -o d_server d_server.c -lpthread -lrt");

	printf("Starting servers .......\n");
	
	/*
	 * creating message queues
	 * there will two message queues
	 * 1. between client and m_server
	 * 2. between client and d_servers*/

	struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 100;
    attr.mq_curmsgs = 0;

	char client_path[] = "/client.c"; char m_server_path[] = "/m_server.c"; char d_server_path[] = "/d_server.c";
	mqd_t client_mq = mq_open(client_path,O_CREAT | O_RDWR,0666,&attr);
	if(client_mq==-1) printf("client_mq %s start.c\n",strerror(errno));
	mqd_t m_server_mq = mq_open(m_server_path,O_CREAT| O_RDWR,0644,&attr);
	if(m_server_mq==-1) printf("m_server_mq %s start.c\n",strerror(errno));
	mqd_t d_server_mq = mq_open(d_server_path,O_CREAT | O_RDWR,0644,&attr);
	if(d_server_mq==-1) printf("d_server_mq %s start.c\n",strerror(errno));


	//start m_server
	char arg_for_m[10];
	sprintf(arg_for_m,"%d",num_d_servers);
	char* m_file[] = {"./m_server",arg_for_m,NULL};
	pid_t pid = fork();	
	if(pid==0) {printf("M_SERVER PID %d\n",getpid());execv("./m_server",m_file);}

	//start d_servers
	for(int i=0;i<num_d_servers;i++){
		char num_str[10];
		sprintf(num_str,"%d",i);
		char* d_file[] = {"./d_server",num_str,NULL};
		pid = fork();
		if(pid==0) execv("./d_server",d_file);
	}
	//start client
	char* c_file[] = {"./client",NULL};
	pid = fork();
	if(pid==0) execv("./client",c_file);

	signal(SIGINT,close_prog);
	pause();
	return 0;
}
