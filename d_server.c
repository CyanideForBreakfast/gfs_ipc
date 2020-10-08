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
char dir_name[10];

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
	//create d_server directory
	struct stat st = {0};
	strcpy(dir_name,argv[1]);
	if(stat(dir_name,&st)==-1) mkdir(dir_name,0700);

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
	signal(SIGQUIT,handler);

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
struct chunk_stored
{
	long int type;
	int status;
};

typedef struct d_server_command
{
	char command[MAX_CHUNK_SIZE];
	int d_server;
	char *args;
} d_server_command;

//for m_server to d_server communication
//status will be sent back to m_server using the status struct
struct do_on_chunk{
	int action; //0 for rm and 1 for cp
	long int old_chunk_id;
	long int new_chunk_id;
};

struct status
{
	long int type;
	int regarding;
	int status;
};

void handler(int signum){
	//wait for semaphore release
	sem_wait(s_d_server);

	if(signum == SIGUSR1){
		struct actual_chunk* ac;
		//recieve chunk
		if(mq_receive(d_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
		ac = (struct actual_chunk*)buffer;
		//printf("Recieving: %s\n",ac->data);

		//write chunk to a file with name as chunk_id
		int status=1;
		char file_path[10];
		sprintf(file_path, "%d/%ld",d_server_id,ac->chunk_id); 
		FILE* fptr = fopen(file_path,"w+");
		if(fptr==NULL) status=0;
		fwrite((void*)ac->data,1,MAX_CHUNK_SIZE,fptr);
		if(ferror(fptr)) status=0;
		fclose(fptr);
	
		struct chunk_stored cs;
		cs.status = status;
		if(mq_send(client_mq, (const char*)&cs,sizeof(struct chunk_stored)+1,0)==-1) printf("%s\n",strerror(errno));
		sem_trywait(s_d_server);
		sem_post(s_client);
		return;
	}
	if(signum == SIGUSR2){
		//printf("Someone called me %d sigusr2\n",getpid());
		
		struct do_on_chunk* doc;
		if(mq_receive(d_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));

		doc = (struct do_on_chunk*)buffer;
		if(doc->action==0){
			char file_path[50];
			sprintf(file_path, "rm %d/%ld 2>/dev/null",d_server_id,doc->old_chunk_id);
			system(file_path);
		}
		if(doc->action==1){
			char file_path[50];
			sprintf(file_path, "cp %d/%ld %d/%ld 2>/dev/null",d_server_id,doc->old_chunk_id,d_server_id,doc->new_chunk_id);
			system(file_path);
			//printf("copied\n");
		}

		sem_trywait(s_d_server);
		sem_post(s_m_server);
		return;

	}
	if(signum == SIGQUIT){
		//recieve and execute command
		d_server_command* d;
		if(mq_receive(d_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
		d = (d_server_command*)buffer;
		char command_to_execute[100];
		sprintf(command_to_execute,"cd %d && %s && cd ..",d->d_server,d->command);

		//creating a temp file
		FILE* fptr = fopen("d_server_command_result","w");
		dup2(fileno(fptr),STDOUT_FILENO);
		system(command_to_execute);
		dup2(fileno(stdout),STDOUT_FILENO);
		
		sem_trywait(s_d_server);
		sem_post(s_client);
	}
}
