#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

#define NAME_SIZE 20
#define BUFFER_SIZE 200
#define FILE_DIR_NAME_SIZE 20
#define PATH_SIZE 20
#define NUM_SUBDIRS 10
#define NUM_FILES 10
#define MAX_CHUNK_SIZE 500

#define SEM_NAME_CLIENT "CLIENT"
#define SEM_NAME_M_SERVER "M_SERVER"
#define SEM_NAME_D_SERVER "D_SERVER"

char buffer[BUFFER_SIZE];

sem_t *s_client;
sem_t *s_m_server;
sem_t *s_d_server;

mqd_t m_server_mq;
mqd_t client_mq;

/* store unused chunk ids here
 * when assigning chunk ids make sure num_unused_chunk_ids is 0
 * if not extract one chunk_id from it and decrement num_ununsed_chunk_ids
 */
int num_unused_chunk_ids = 0;
long int unused_chunk_ids[1000];

/* details given in digram
 * chunks stores d_server_id of d_servers storing the chunk
*/
typedef struct file
{
	char name[FILE_DIR_NAME_SIZE];
	int chunk_num;
	int* chunks;
} file;

typedef struct dir
{
	char name[FILE_DIR_NAME_SIZE];
	struct dir **subdirs;
	int num_subdir;
	file files[NUM_FILES];
	int num_files;
} dir;

dir *root;
/*
 * will be the format initial command is sent
 * command type 0 - addf, 1 - rm, 2 - mv, 3 - cp
 * src - localfile_path (might be empty for some commands)
 * dest - destination_path
 * int chunk_size
 * */
struct command
{
	long int type;
	int command_type;
	char src[PATH_SIZE];
	char dest[PATH_SIZE];
	int chunk_size;
};

/*
 * will be used to return status to client to continue further steps
 * regarding: 
 * 0 - addf - file added to hierarchy success
 * */
struct status
{
	long int type;
	int regarding;
	int status;
};
/* specific to addf funcionality
 * struct add_chunk_request requests m_sever to add chunk for the file
 * struct chunk_added is returned by m_server with chunk id and three d_server_ids
 * struct actual_chunk is sent to a d_server
 * struct chunk_stored is returned by d_server to client
 * */
struct add_chunk_request{
	long int type;
	int chunk_num;
    char file_path[PATH_SIZE];
	int term;
};

struct chunk_added
{
	long int type;
	long int chunk_id;
	int d_servers[3];
};

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

void handle_command(struct command);
int main()
{
	s_client = sem_open(SEM_NAME_CLIENT, O_RDWR);
	s_m_server = sem_open(SEM_NAME_M_SERVER, O_RDWR);
	s_d_server = sem_open(SEM_NAME_D_SERVER, O_RDWR);

	char client_path[] = "/client.c"; char m_server_path[] = "/m_server.c"; char d_server_path[] = "/d_server.c";
	client_mq = mq_open(client_path,O_WRONLY);
	if(client_mq==-1) printf("client_mq %s m_server.c\n",strerror(errno));
	m_server_mq = mq_open(m_server_path,O_RDONLY);
	if(m_server_mq==-1) printf("m_server_mq %s m_server.c\n",strerror(errno));

	printf("M_server %d %d\n",client_mq,m_server_mq);
	
	root = (dir *)malloc(sizeof(dir));
	root->num_subdir = 0;

	struct command* recieved_command;
	while (1)
	{
		sem_wait(s_m_server);
		if(mq_receive(m_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
		recieved_command = (struct command*)buffer;
		handle_command(*recieved_command);
	}
	return 0;
}

void handle_command(struct command recieved_command){
	switch(recieved_command.command_type){
		case 0:
			//addf
			
			//find file location and add file properly
			printf("%s added to %s successfully\n",recieved_command.src,recieved_command.dest);

			//send status
			struct status stat; stat.type = 1;
			stat.regarding = 0;
			stat.status = 1;
			//msgsnd(m_server_mq,&stat,sizeof(struct status),0);
			if(mq_send(client_mq,(const char*)&stat,sizeof(struct status)+1,0)==-1) printf("%s",strerror(errno));

			printf("m_server message sent\n");
			sem_post(s_client);
			sem_trywait(s_m_server);

			struct add_chunk_request* acr;
			struct chunk_added ca; ca.type = 2; ca.chunk_id = 3;
			int count = 3;
			do{
				printf("m_server waiting...\n");
				sem_wait(s_m_server);
				printf("m_server wait over!\n");

				if(mq_receive(m_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
				acr = (struct add_chunk_request*) buffer;
				if((*acr).term==1) {break;}
				ca.chunk_id = count++;;
				ca.d_servers[0] = count++; ca.d_servers[1] = count++; ca.d_servers[2] = count++;
				if(mq_send(client_mq,(const char*)&ca, sizeof(struct chunk_added)+1,0)==-1) printf("%s\n",strerror(errno));

				sem_post(s_client);
				sem_trywait(s_d_server);

				printf("recieved %d\n",(*acr).chunk_num);
			} while((*acr).term!=1);
			printf("command terminated \n");

			sem_trywait(s_m_server);
			sem_post(s_client);
			return;
	}
}

/*
 * finds file pointer to the path
 * creates new if not present
 * */
file *find_location(char *path)
{

}
