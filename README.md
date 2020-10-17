This is a file storage system implemented using IPC mechanisms such as message queues, semaphores, shared memories and signals.

In this file storage system, there is a **metadata server (M)**, **data
servers(D0, D1, ..)** and a **client(C)**.

General information about the implementation:

 - There are three message queues : ​client_mq​, ​m_server_mq and
d_server_mq​, which respectively are used by the corresponding
processes for reading messages while it can be written on by any
of the other two remaining types of servers.
 - There are three semaphores : ​s_client​, ​s_m_server and
s_d_server​. The semaphores ensure synchronicity in any action
for interprocess back and forth communication amongst servers.
During transmission, the semaphore of the sending server is
blocked and that of the receiving process is unlocked.
 - There is a shared memory ​d_servers_pids that is shared by the
starting process start.c, client.c and m_server.c. It stores the
d_server_ids against corresponding pids.
 - To ensure only d_server accesses the d_server_mq at a time,
each d_server is by default indefinitely paused, and have signal
handlers for SIGQUIT, SIGUSR1 and SIGUSR2. It is used by any
other server to communicate with that specific d_server, as pid of
that d_server is determined by d_server_pids and it is used to
send signal to that d_server to wake it so that it becomes ready to
receive message in d_server_mq. Hence, only the required
d_server receives the message.
 - Each d_server creates a new directory where it stores chunks as
files with chunk_id as filenames.

Specific information about the implementation:
 - MAX ALLOWED CHUNK SIZE = 30 bytes
 - MAX NUMBER OF D SERVERS  = 50
 - MAX LENGTH OF DIRECTORY/FILE NAME = 20 bytes
 - MAX NUMBER OF IMMEDIATE FILES IN A DIRECTORY = 10

 Commands:
 - **addf(*local_path*, *chunk_size*, *m_server_path*)**
 - **rm(*m_server_path*)**
 - **cp(*m_server_path_src*, *m_server_path_dest*)**
 - **mv(*m_server_path_src*, *m_server_dest*)**
 - ***d_server_num*** **bash_command**

 Start m_server and d_server:

    gcc start.c
    ./a.out

For every client: 

    ./client num_of_d_server