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

#define SEM_NAME_CLIENT "CLIENT"
#define SEM_NAME_M_SERVER "M_SERVER"
#define SEM_NAME_D_SERVER "D_SERVER"

int m_server_mq, client_mq, d_server_mq;
void close_prog(int);
void close_prog(int sig){
	printf("\nClosing program ....\n");
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
	system("gcc -o client client.c");
	system("gcc -o m_server m_server.c");
	system("gcc -o d_server d_server.c");

	printf("Starting servers .......\n");
	
	/*
	 * creating message queues
	 * there will two message queues
	 * 1. between client and m_server
	 * 2. between client and d_servers*/
	int m_server_mq = msgget(ftok("./m_server.c",99),0666|IPC_CREAT);
	int d_server_mq = msgget(ftok("./d_server.c",99),0666|IPC_CREAT);
	int client_mq = msgget(ftok("./client.c",99),0666|IPC_CREAT);
	//start m_server
	char* m_file[] = {"./m_server",NULL};
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
