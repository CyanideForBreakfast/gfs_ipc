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

/*
 ****WARNING****
 Do not change these values without making sure the corresponding values are changed across all files.
 Leads to message-size inconsistency across files
*/
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

sem_t *s_client;
sem_t *s_m_server;
sem_t *s_d_server;

/* message queue ids for m_server and d_server
 * */
int m_server_mq;
int d_server_mq;
int client_mq;

struct addf
{
	char local_path[PATH_SIZE];
	int chunk_size;
	char m_server_path[PATH_SIZE];
};

struct rm
{
	char m_server_path[PATH_SIZE];
};

struct mv
{
	char m_server_src[PATH_SIZE];
	char m_server_dest[PATH_SIZE];
};

struct cp
{
	char m_server_src[PATH_SIZE];
	char m_server_dest[PATH_SIZE];
};

typedef union m_server_command
{
	struct addf a;
	struct rm r;
	struct mv m;
	struct cp c;
} m_server_command;

typedef struct d_server_command
{
	char *command;
	int d_server;
	char *args;
} d_server_command;

void take_input();
char *read_command();
void parse_and_execute(char *);
void execute_m_server_commands(m_server_command *, int); //takes command type as second arg - 0:addf,1:rm,2:mv,3:cp
void execute_d_server_commands(d_server_command *);

int main()
{
	s_client = sem_open(SEM_NAME_CLIENT, O_RDWR);
	s_m_server = sem_open(SEM_NAME_M_SERVER, O_RDWR);
	s_d_server = sem_open(SEM_NAME_D_SERVER, O_RDWR);

	//setting mq_ids for m_server and d_server
	m_server_mq = msgget(ftok("./m_server.c", 99), 0666 | IPC_CREAT);
	d_server_mq = msgget(ftok("./d_server.c", 99), 0);
	client_mq = msgget(ftok("./client.c",99),0666|IPC_CREAT);

	printf("client M_SERVER %d D_SERVER %d\n", m_server_mq, d_server_mq);
	printf("addf(src,chunk_size,dest),rm(src),cp(src,dest),mv(src,dest)\n");
	//testing
	/*
	struct hello_msg{
		int type;
		char msg[10];
	};
	struct hello_msg hello;
	hello.type = 3;
	strcpy(hello.msg,"hello\0");
	msgsnd(m_server_mq,&hello,sizeof(hello),0);
	printf("client message sent %s\n",hello.msg);
	*/

	take_input();
	return 0;
}

void take_input()
{
	char *user_command;
	while (1)
	{
		printf("$$$ ");
		/*
		 * read till EOF or \n encountered
		 * store and return pointer to location 
		 */
		user_command = read_command();
		/*
		 * parse user_command
		 * check for new commands like addfile, mv, rm
		 * or other commands and execute them accordingly
		 */
		parse_and_execute(user_command);
		free(user_command);
	}
}

char *read_command()
{
	char *user_command = (char *)malloc(BUFFER_SIZE * sizeof(char));
	int command_size = BUFFER_SIZE - 1, index = 0, reallocated = 1;
	char c = '\0';
	c = getchar();
	while (c != EOF && c != '\n')
	{
		if (0 < command_size--)
		{
			user_command[index++] = c;
			c = getchar();
		}
		else
		{
			user_command = (char *)realloc(user_command, BUFFER_SIZE * sizeof(char) * (++reallocated));
			command_size = BUFFER_SIZE - 1;
			user_command[index++] = c;
			c = getchar();
		}
	}
	user_command[index++] = '\0';
	return user_command;
}
/*
 * handle each command appropriately
 * */
void parse_and_execute(char *user_command)
{
	m_server_command* m_command_stored = (m_server_command*)malloc(sizeof(m_server_command));
	m_server_command mcommand = *m_command_stored;
	char *instruction = (char *)malloc(sizeof(char) * BUFFER_SIZE);
	instruction = strtok(user_command, "(");
	// Trim trailing white-spaces
	if (strcmp(instruction, "addf") == 0)
	{
		instruction = strtok(NULL, ",");
		strcpy(mcommand.a.local_path, instruction);

		instruction = strtok(NULL, ",");
		sscanf(instruction, "%d", &mcommand.a.chunk_size);

		instruction = strtok(NULL, ")");
		strcpy(mcommand.a.m_server_path, instruction);

		execute_m_server_commands(&mcommand, 0);
	}
	else if (strcmp(instruction, "rm") == 0)
	{
		instruction = strtok(NULL, ")");
		strcpy(mcommand.r.m_server_path, instruction);

		execute_m_server_commands(&mcommand, 1);
	}
	else if (strcmp(instruction, "mv") == 0)
	{
		instruction = strtok(NULL, ",");
		strcpy(mcommand.m.m_server_src, instruction);

		instruction = strtok(NULL, ")");
		strcpy(mcommand.m.m_server_dest, instruction);

		execute_m_server_commands(&mcommand, 2);
	}
	else if (strcmp(instruction, "cp") == 0)
	{
		instruction = strtok(NULL, ",");
		strcpy(mcommand.c.m_server_src, instruction);

		instruction = strtok(NULL, ")");
		strcpy(mcommand.c.m_server_dest, instruction);

		execute_m_server_commands(&mcommand, 3);
	}
	else
	{
		printf("\nCommand not defined!!\n");
	}
}
/* function should be triggered when commands are
 *  addf (local_path, chunk_size, m_server_path), 
 *  rm (m_server_path), 
 *  mv (m_server_source, m_server_destination) 
 *  cp (m_server_source, m_server_destination)
 * */

/*
struct addf{
	char* local_path;
	int chunk_size;
	char* m_server_path;
};
struct rm{
	char* m_server_path;
}
struct mv{
	char* m_server_src;
	char* m_server_dest;
}
struct cp{
	char* m_server_src;
	char* m_server_dest;
}
typedef union m_server_command{
	int command_type; //0 for addf, 1 for rm, 2 for mv, 3 for cp
	struct addf a;
	struct rm r;
	struct mv m;
	struct cp c;
} m_server_command;


typedef struct d_server_command{
	int command_type;
	char* command;
	int d_server;
	char* args;
} d_server_command;
*/

/*
 * will be the format for initial command sent
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
struct add_chunk_request
{
	long int type;
	int chunk_num;
	char file_path[PATH_SIZE];
	int term; //refers to whether chunking is over or not 0 for ongoing, 1 for terminated
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

FILE *next_chunk(FILE *, struct actual_chunk *, int);
void execute_m_server_commands(m_server_command *m, int command_type)
{
	struct command msg; msg.type = 1;
	switch (command_type)
	{
		case 0:
			//addf
			/*
				* here src will be the local path to file
				* dest will be the m_server path
				* */
			msg.command_type = 0;
			strcpy(msg.src, m->a.local_path);
			strcpy(msg.dest, m->a.m_server_path);
			msg.chunk_size = m->a.chunk_size;
			//send to add file to hierarchy
			msgsnd(m_server_mq, &msg, sizeof(struct command), 0);

			sem_post(s_m_server);
			sem_trywait(s_client);
			sem_wait(s_client);

			//wait for status
			struct status stat;
			msgrcv(m_server_mq, &stat, sizeof(struct status), 1, 0);
			if (stat.status)
				printf("addf status confirmed by client\n");

			/* in each iteration,
				* call next_chunk() until -1 chunk_num returned
				* call add_chunk_request
				* recieve d_server_ids
				* send chunk to each of d_server
				*/	
			FILE* fptr = fopen(m->a.local_path,"r");
			struct add_chunk_request acr; acr.type = 2;
			struct chunk_added ca;
			struct actual_chunk c;
			c.chunk_num=0;
			acr.term=0;
			int chunk_num = 0;
			while(fptr!=NULL){
				fptr = next_chunk(fptr,&c,m->a.chunk_size);
				acr.chunk_num = chunk_num++;
				if(c.chunk_num==-1 || fptr==NULL) {
					break;
				}
				strcpy(acr.file_path,m->a.m_server_path);
				msgsnd(m_server_mq,&acr,sizeof(struct add_chunk_request),0);
				msgrcv(client_mq,&ca,sizeof(struct chunk_added),2,0);
				printf("recieved id : %ld for chunk %d , %d %d %d\n",ca.chunk_id, chunk_num, ca.d_servers[0],ca.d_servers[1],ca.d_servers[2]);
			};
			acr.term=1;
			msgsnd(m_server_mq,&acr,sizeof(struct add_chunk_request),0); //send chunking terminated message
			break;
	}
}
/*
 * dividing file into chunks
 * reading chunk_size bytes
 * (maybe) append \0 at the end
 * return the new file pointer
 * if eof then chunk_num is -1 (used to detect the last chunk)
 * */
FILE *next_chunk(FILE *fptr, struct actual_chunk *cptr, int chunk_size)
{
	char buf[MAX_CHUNK_SIZE];
	if(feof(fptr)) {
		return NULL;
	}
	int bytes = fread(buf,chunk_size,1,fptr);
	//possible bug : resolve putting \0 at the end of buf when read
	strcpy(cptr->data,buf);
	strcat(cptr->data,"\0");
	return fptr;
}

/* function should be triggered when commands are
 * normal linux commmands like ls, wc etc as mentioned in assignment
 * where arguments given to commands would be d_server_number instead of path
 * */
void execute_d_server_commands(d_server_command *d)
{
}
