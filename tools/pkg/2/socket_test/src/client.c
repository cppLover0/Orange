#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#include <string.h>

static const char* socket_path = "/home/mysocket";
static const unsigned int s_recv_len = 200;
static const unsigned int s_send_len = 100;

int main()
{
	int sock = 0;
	int data_len = 0;
	struct sockaddr_un remote;
	char recv_msg[s_recv_len];
	char send_msg[s_send_len];

	memset(recv_msg, 0, s_recv_len*sizeof(char));
	memset(send_msg, 0, s_send_len*sizeof(char));

	if( (sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1  )
	{
		printf("Client: Error on socket() call \n");
		return 1;
	}

	remote.sun_family = AF_UNIX;
	strcpy( remote.sun_path, socket_path );
	data_len = strlen(remote.sun_path) + sizeof(remote.sun_family);

	printf("Client: Trying to connect... \n");
	if( connect(sock, (struct sockaddr*)&remote, data_len) == -1 )
	{
		printf("Client: Error on connect call \n");
		return 1;
	}

	printf("Client: Connected \n");

	while( write(STDOUT_FILENO,">",1), fgets(send_msg, s_send_len, stdin), !feof(stdin))
	{
		if( send(sock, send_msg, strlen(send_msg)*sizeof(char), 0 ) == -1 )
		{
			printf("Client: Error on send() call \n");
		}
		memset(send_msg, 0, s_send_len*sizeof(char));
		memset(recv_msg, 0, s_recv_len*sizeof(char));

		if( (data_len = recv(sock, recv_msg, s_recv_len, 0)) > 0 )
		{
			printf("Client: Data received: %s\n", recv_msg);
		}
		else
		{
			if(data_len < 0)
			{
				printf("Client: Error on recv() call \n");
			}
			else
			{
				printf("Client: Server socket closed \n");
				close(sock);
				break;
			}

		}
	}

	printf("Client: bye! \n");

	return 0;
}