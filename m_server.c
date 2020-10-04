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
#include <sys/mman.h>

#define NAME_SIZE 20
#define BUFFER_SIZE 200
#define FILE_DIR_NAME_SIZE 20
#define PATH_SIZE 20
#define NUM_SUBDIRS 10
#define NUM_FILES 10
#define MAX_CHUNK_SIZE 30
#define DEF_NUM_CHUNK 30 //represents the increment of filling up of chunks in file
#define MAX_D_SERVERS 50

#define SEM_NAME_CLIENT "CLIENT"
#define SEM_NAME_M_SERVER "M_SERVER"
#define SEM_NAME_D_SERVER "D_SERVER"
#define SHARED_MEMORY_PIDS_NAME "D_SERVERS_PIDS"

char buffer[BUFFER_SIZE];
pid_t d_servers_pids_array[MAX_D_SERVERS];

sem_t *s_client;
sem_t *s_m_server;
sem_t *s_d_server;

mqd_t m_server_mq;
mqd_t client_mq;
mqd_t d_server_mq;

/* store unused chunk ids here
 * when assigning chunk ids make sure num_unused_chunk_ids is 0
 * if not extract one chunk_id from it and decrement num_ununsed_chunk_ids
 */
int num_unused_chunk_ids = 0;
long int unused_chunk_ids[1000];
long int max_chunk_id = 0;

/* will be used to assign d_server_ids
	d_servers returned will be seed, seed+1,seed+2 (ensuring uniform distribution amongst d_servers)
*/
int d_server_id_seed = 0;
int num_d_servers; //number of d_servers - to be passed from start.c

/* details given in digram
 * chunks stores d_server_id of d_servers storing the chunk
*/
typedef struct chunk{
	int chunk_num;
	long int chunk_id;
	int d_servers[3];
} chunk;

typedef struct file
{
	char name[FILE_DIR_NAME_SIZE];
	int chunk_num;
	chunk* chunks;
	int chunk_capacity;
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
file* find_location(char*);
void print_hierarchy(dir*);
int main(int argc,char* argv[])
{
	//read and store pids from shared memory
	num_d_servers = atoi(argv[1]);
	int sm_pids = shm_open(SHARED_MEMORY_PIDS_NAME, O_RDONLY, 0);
	void* ptr = mmap(NULL,MAX_D_SERVERS*sizeof(pid_t),PROT_READ, MAP_SHARED, sm_pids, 0);
	pid_t* pid_t_ptr = (pid_t*)ptr;
	for(int i=0;i<num_d_servers;i++){
		d_servers_pids_array[i] = pid_t_ptr[i];
	}
	
	s_client = sem_open(SEM_NAME_CLIENT, O_RDWR);
	s_m_server = sem_open(SEM_NAME_M_SERVER, O_RDWR);
	s_d_server = sem_open(SEM_NAME_D_SERVER, O_RDWR);

	char client_path[] = "/client.c"; char m_server_path[] = "/m_server.c"; char d_server_path[] = "/d_server.c";
	client_mq = mq_open(client_path,O_WRONLY);
	if(client_mq==-1) printf("client_mq %s m_server.c\n",strerror(errno));
	m_server_mq = mq_open(m_server_path,O_RDONLY);
	if(m_server_mq==-1) printf("m_server_mq %s m_server.c\n",strerror(errno));
	d_server_mq = mq_open(d_server_path,O_WRONLY);
	if(d_server_mq==-1) printf("d_server_mq %s client.c\n",strerror(errno));
	
	printf("M_server %d %d\n",client_mq,m_server_mq);
	
	root = (dir *)malloc(sizeof(dir));
	root->num_subdir = 0;
	strcat(root->name,"/");

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
			
			//add file
			;
			file* f = find_location(recieved_command.dest);
			f->chunk_num=0;
			f->chunk_capacity=DEF_NUM_CHUNK;
			f->chunks = (chunk*)malloc(DEF_NUM_CHUNK*sizeof(chunk));

			printf("%s added to %s successfully\n",recieved_command.src,recieved_command.dest);

			print_hierarchy(root);

			//send status
			struct status stat; stat.type = 1;
			stat.regarding = 0;
			stat.status = 1;
			if(mq_send(client_mq,(const char*)&stat,sizeof(struct status)+1,0)==-1) printf("%s",strerror(errno));

			//printf("m_server message sent\n");
			sem_post(s_client);
			sem_trywait(s_m_server);

			struct add_chunk_request* acr;
			struct chunk_added ca; ca.type = 2;
			do{
				//printf("m_server waiting...\n");
				sem_wait(s_m_server);
				//printf("m_server wait over!\n");

				//recieve add_chunk_request
				if(mq_receive(m_server_mq,buffer,BUFFER_SIZE,NULL)==-1) printf("%s\n",strerror(errno));
				acr = (struct add_chunk_request*) buffer;
				if((*acr).term==1) {break;}

				/*
				 * Decide chunk_id
				 * Decide d_server_ids to return
				 * Update file hierarchy
				 * Return the values
				*/
				//decide chunk_id
				long int assigned_chunk_id;
				if(num_unused_chunk_ids==0) assigned_chunk_id=unused_chunk_ids[--num_unused_chunk_ids];
				else assigned_chunk_id = max_chunk_id++;
				ca.chunk_id = assigned_chunk_id;
				
				//decide d_servers
				ca.d_servers[0] = (d_server_id_seed++)%num_d_servers; ca.d_servers[1] = (d_server_id_seed++)%num_d_servers; ca.d_servers[2] = (d_server_id_seed++)%num_d_servers;
				d_server_id_seed = d_server_id_seed%num_d_servers;
				
				//update file hierarchy
				f = find_location(acr->file_path);
				//if first chunk, init
				/*
				if(acr->chunk_num==0){
					f->chunks = (chunk*)malloc(DEF_NUM_CHUNK*sizeof(chunk));
					f->chunk_capacity = DEF_NUM_CHUNK;
					f->chunk_num = 0;
				}
				*/
				//check if capacity filled, if it is then realloc
				if(f->chunk_num>=f->chunk_capacity-1){
					f->chunks = realloc(f->chunks,f->chunk_capacity+DEF_NUM_CHUNK);
					f->chunk_capacity+=DEF_NUM_CHUNK;
				}
				//put chunk
				f->chunks[f->chunk_num].chunk_id = assigned_chunk_id;
				f->chunks[f->chunk_num].d_servers[0] = ca.d_servers[0];
				f->chunks[f->chunk_num].d_servers[1] = ca.d_servers[1];
				f->chunks[f->chunk_num].d_servers[2] = ca.d_servers[2];
				f->chunk_num++;

				//send values
				if(mq_send(client_mq,(const char*)&ca, sizeof(struct chunk_added)+1,0)==-1) printf("%s\n",strerror(errno));

				sem_post(s_client);
				sem_trywait(s_d_server);

				//printf("recieved %d\n",(*acr).chunk_num);
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
	int dir_depth = 0;
	char temp_path[strlen(path) + 1];
	strcpy(temp_path, path);

	for (char *i = path + 1; *i != '\0'; i++)
	{
		if (*i == '/')
		{
			dir_depth++;
		}
	}

	dir *present_dir = root;	//	iterates starting from root
	char *itr = strtok(temp_path, "/");
	for (int i = 0; i < dir_depth; i++, itr = strtok(NULL, "/"))
	{
		short dir_found = 0;
		for (int j = 0; j < present_dir->num_subdir; j++)
		{
			if (strcmp(itr, present_dir->subdirs[j]->name) == 0)
			{
				present_dir = present_dir->subdirs[j];
				dir_found = 1;
				break;
			}
		}

		if (!dir_found)	//	Make new directory with pointer at subdirs[index]
		{
			int index = present_dir->num_subdir;
			present_dir->subdirs = (dir **)realloc(present_dir->subdirs, ++present_dir->num_subdir * sizeof(dir *));
			
			present_dir->subdirs[index] = (dir *)malloc(sizeof(dir));
			strcpy(present_dir->subdirs[index]->name, itr);
			present_dir->subdirs[index]->num_files = 0;
			present_dir->subdirs[index]->num_subdir = 0;
			present_dir = present_dir->subdirs[index];
		}
	}

	for (int i = 0; i < present_dir->num_files; i++)
	{
		if (strcmp(present_dir->files[i].name, itr) == 0)
		{
			return &present_dir->files[i];
		}
	}

	// File does not exit
	strcpy(present_dir->files[present_dir->num_files].name, itr);
	// printf("file location established\n");

	return &present_dir->files[present_dir->num_files++];
}

void print_hierarchy(dir* d){
	//takes input as directory and prints hierarchy
	printf("%s:\n",d->name);
	printf("dirs: ");
	for(int i=0;i<d->num_subdir;i++){
		printf("%s ",d->subdirs[i]->name);
	}
	printf("\nfiles: ");
	for(int i=0;i<d->num_files;i++){
		printf("%s ",d->files[i].name);
	}
	printf("\n---------------\n");
	for(int i=0;i<d->num_subdir;i++){
		print_hierarchy(d->subdirs[i]);
	}
}
