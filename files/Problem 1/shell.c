#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 100
#define PIPE_SIZE 1000000

FILE *file = NULL;
int commands_stored = 0, file_mode = -1;
char **last_ten_commands;

void run_shell();
void handle_signal(int);
void handle_sigint();
void handle_sigquit();
char *read_command();
void add_command(char *);
void parse_and_execute();

int main()
{
	last_ten_commands = (char **)malloc(10 * sizeof(char *));
	printf("\n");

	signal(SIGQUIT, handle_signal);
	signal(SIGINT, handle_signal);

	run_shell();
	return 0;
}

void run_shell()
{
	char *user_command;
	while (1)
	{
		printf("$$$ ");
		/* read till \n or EOF encountered
		 * store and return pointer to location
		 * */
		user_command = read_command();

		/*set file pointer to handle redirection*/
		char *itr = user_command, redir, *temp = &redir;
		while (*itr != '\0')
		{
			if (*itr == '<' || *itr == '>')
			{
				redir = *itr;
				temp = itr;
				*itr++ = '\0';
				while (*itr == ' ')
				{
					itr++;
				}

				switch (redir)
				{
				case '<':
					file_mode = 0;
					printf("file opened for reading:\t%s\n", itr);
					file = fopen(itr, "r");
					stdin = file;
					break;

				case '>':
					file_mode = 1;
					printf("file opened for writing:\t%s\n", itr);
					file = fopen(itr, "w");
					stdout = file;
					break;

				default:
					break;
				}
				printf("Main command:\t%s", user_command);
				break;
			}
			itr++;
		}

		/*
		 * parse user_command 
		 * check for new operators - || and |||
		 * convert to corresponding list of bash commands
		 * execute all
		 * 
		 */
		parse_and_execute(user_command);
		*temp = redir;
	}
}

/*
 * signal handling
 * handle SIGINT
 * handle SIGQUIT
 * */

void handle_signal(int signum)
{
	if (signum == 3)
	{
		handle_sigquit();
		return;
	}
	if (signum == 2)
	{
		handle_sigint();
		return;
	}
}

void handle_sigquit()
{
	char c = '\0';
	while (c != 'y' && c != 'n')
	{
		printf("\nDo you really want to exit (y/n)?");
		c = getchar();
	}
	if (c == 'n')
		return;
	raise(SIGKILL);
}

void handle_sigint()
{
	printf("\n");
	for (int i = 0; i < commands_stored; i++)
	{
		printf("%s\n", last_ten_commands[i]);
	}
	raise(SIGKILL);
}

/*
 * read till EOF or \n
 * allocate buffer space for storing command
 * reallocate if exceed buffer
 * add command to last_ten_commands 
*/
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
	if (strlen(user_command))
		add_command(user_command);
	return user_command;
}

// add commands to last ten commands
void add_command(char *command)
{
	if (commands_stored < 10)
	{
		last_ten_commands[commands_stored] = command;
		commands_stored++;
	}
	else
	{
		free(last_ten_commands[0]);
		//shift commands list to left
		for (int i = 0; i < 9; i++)
		{
			last_ten_commands[i] = last_ten_commands[i + 1];
		}
		last_ten_commands[9] = command;
	}
}

/*
 * parse single line command into possibly multiple line bash equivalents
 * execute each one
*/
void parse_and_execute(char *user_command)
{
	char commands[10][3][BUFFER_SIZE];
	for (size_t i = 0; i < 10; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			memset(commands[i][j], '\0', BUFFER_SIZE);
		}
	}

	int num_commands = 0;
	char ch, *itr = user_command;
	while (*itr != '\0' && *itr != '\n')
	{
		int num_pipes = 0;
		if (num_commands != 0)
			while (*itr == '|')
			{
				num_pipes++;
				itr++;
			}
		else
			num_pipes = 1;

		for (int p = 0; p < num_pipes; p++)
		{
			// remove leading white-spaces
			while (*itr == ' ')
				itr++;

			int k = 0;
			while (*itr != '|' && *itr != '\0' && *itr != '\n')
			{
				if (*itr == ',')
				{
					k++;
					itr++;
					break;
				}
				commands[num_commands][p][k++] = *itr++;
			}

			// remove trailing white-spaces
			while (commands[num_commands][p][--k] == ' ')
			{
				commands[num_commands][p][k] = '\0';
			}
		}
		num_commands++;
	}

	// for (size_t i = 0; i < num_commands; i++)
	// {
	// 	for (size_t j = 0; j < 3; j++)
	// 	{
	// 		printf("%ld %ld\t%s\t\t", i, j, commands[i][j]);
	// 	}
	// 	printf("\n");
	// }

	size_t bytes_to_read = 0;
	char *input = (char *)malloc(sizeof(char) * PIPE_SIZE), *output = NULL;
	if (file_mode == 0)
	{
		fscanf(file, "%s", input);
	}
	for (int i = 0; i < num_commands; i++)
	{
		FILE *fptr = fopen("t", "w+");
		if (output != NULL)
		{
			strcpy(input, output);
			fprintf(fptr, "%s", output);
		}
		fclose(fptr);
		//Argument extraction
		for (int j = 0; j < 3; j++)
		{
			if (strcmp(commands[i][j], "") == 0)
				break;

			char curr_command[BUFFER_SIZE], *arg_extractor;
			strcpy(curr_command, commands[i][j]);

			int k = 0;
			char **argv;
			arg_extractor = strtok(curr_command, " ");
			while (arg_extractor != NULL)
			{
				argv = (char **)realloc(argv, sizeof(char *) * (k + 1));
				argv[k] = (char *)malloc(sizeof(char) * BUFFER_SIZE);

				stpcpy(argv[k], arg_extractor);
				arg_extractor = strtok(NULL, " ");
				k++;
			}
			argv = (char **)realloc(argv, sizeof(char *) * (k + 2));
			if (i != 0)
			{
				argv[k++] = "t";
			}
			argv[k] = NULL;


			// Pipes established for child process to communicate
			int from_child[2];
			int to_child[2];
			pipe(from_child);
			pipe(to_child);

			free(output);
			output = (char *)malloc(sizeof(char) * PIPE_SIZE);

			write(to_child[1], input, bytes_to_read);
			pid_t p = fork();
			switch (p)
			{
			case -1:
				perror("fork\n");
				exit(1);
				break;

			case 0:
				dup2(from_child[1], STDOUT_FILENO); //stdout to string to parent process
				dup2(to_child[0], STDIN_FILENO);		//stdin of string from parent process
				close(from_child[0]);
				close(from_child[1]);
				close(to_child[0]);
				close(to_child[1]);
				// fputs(input, stdin);
				// fscanf(stdin, "%s", input);
				// printf("%s\n", input);
				char path_to_executable[50] = {'\0'};
				strcat(path_to_executable, "/bin/");
				strcat(path_to_executable, argv[0]);
				execvp(argv[0], argv);
				perror("execv failed\n");
				exit(1);
				break;

			default:
				close(to_child[0]);
				close(from_child[1]);
				printf("Process id:\t%d\n", p);

				bytes_to_read = read(from_child[0], output, PIPE_SIZE);
				printf("%s\n", output);
				break;
			}

			for (int index = 0; index < k - 1; index++)
			{
				free(argv[index]);
			}
			free(argv);

			fflush(stdout);
			if (i == num_commands)
			{
				switch (file_mode)
				{
				case -1:
					printf("\n%s\n", output);
					fflush(stdout);
					break;

				case 1:
					fprintf(file, "%s", output);
					break;

				default:
					break;
				}
			}
		}
		fclose(fptr);
	}

	free(input);
	free(output);
	if (file != NULL)
	{
		fclose(file);
		file = NULL;
	}
	file_mode = -1;
	printf("command execution ended\n\n");
}

// void print_details(pid_t p, int fd[])
// {
// 	printf("Process id: %d\tPipe fds: %d %d\n", p, fd[0], fd[1]);
// }