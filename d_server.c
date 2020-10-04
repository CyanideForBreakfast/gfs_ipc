#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>

#define BUFFER_SIZE 200
#define MAX_CHUNK_SIZE 30

#define SEM_NAME_CLIENT "CLIENT"
#define SEM_NAME_M_SERVER "M_SERVER"
#define SEM_NAME_D_SERVER "D_SERVER"

int d_server_id; //id of the d_server

char buffer[BUFFER_SIZE];

sem_t *s_client;
sem_t *s_m_server;
sem_t *s_d_server;

/* message queue ids for m_server and d_server
 * */
mqd_t m_server_mq;
mqd_t d_server_mq;
mqd_t client_mq;

void handler(int);
int main(int argc, char* argv[]){
	d_server_id = atoi(argv[1]);
	d_server_mq = msgget(ftok("./d_server.c",99),0);

	//set semaphores
	s_client = sem_open(SEM_NAME_CLIENT, O_RDWR);
	s_m_server = sem_open(SEM_NAME_M_SERVER, O_RDWR);
	s_d_server = sem_open(SEM_NAME_D_SERVER, O_RDWR);

	//setting mq_ids for m_server and d_server
	char client_path[] = "/client.c"; char m_server_path[] = "/m_server.c"; char d_server_path[] = "/d_server.c";
	client_mq = mq_open(client_path,O_WRONLY);
	if(client_mq==-1) printf("client_mq %s client.c\n",strerror(errno));
	m_server_mq = mq_open(m_server_path,O_WRONLY);
	if(m_server_mq==-1) printf("m_server_mq %s client.c\n",strerror(errno));
	d_server_mq = mq_open(d_server_path,O_RDONLY);
	if(d_server_mq==-1) printf("d_server_mq %s client.c\n",strerror(errno));

	signal(SIGUSR1,handler);
	signal(SIGUSR2,handler);

	while(1){
		//wait till a signal is recieved
		pause();
	}
	return 0;
}
struct actual_chunk
{
	long int type;
	int chunk_num;
	long int chunk_id;
	char data[MAX_CHUNK_SIZE];
};
void handler(int signum){
	//wait for semaphore release
	sem_wait(s_d_server);

	if(signum == SIGUSR1){
		struct actual_chunk* ac;
		//recieve chunk
		if(mq_receive(d_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
		ac = (struct actual_chunk*)buffer;
		printf("%s\n",ac->data);

		sem_post(s_client);
		sem_trywait(s_d_server);
	}
	if(signum == SIGUSR2){
		printf("Someone called me %d sigusr2\n",getpid());
	}
}
