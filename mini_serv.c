#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>

int server;
int g_id = 0;

typedef struct client_s
{
	int id;
	char *buf;
}				client_t;

client_t g_clients[FD_SETSIZE];

fd_set readfds, writefds, fds;

char g_buf[1024];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int putError(char *msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	return (1);
}

void fatalError()
{
	for(int i = 0; i < FD_SETSIZE; ++i)
		if (FD_ISSET(i, &fds))
		{
			free(g_clients[i].buf);
			close(i);
		}
	close(server);
	exit (putError("Fatal error\n"));
}

void sendAll(int fd, char *msg)
{
	for (int i = 0; i < FD_SETSIZE; ++i)
		if (FD_ISSET(i, &writefds) && fd != i)
			send(i, msg, strlen(msg), 0);
}

int main(int argc, char *argv[]) {
	int connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli; 

	if (argc < 2)
		return(putError("Wrong number of arguments\n"));

	FD_ZERO(&fds);

	// socket create and verification 
	server = socket(AF_INET, SOCK_STREAM, 0); 
	if (server == -1)
		exit(putError("Fatal error\n")); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(server, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatalError();
	if (listen(server, 10) != 0)
		fatalError();

	FD_SET(server, &fds);

	while (1)
	{
		readfds = writefds = fds;
		select(FD_SETSIZE, &readfds, &writefds, NULL, NULL);

		for (int fd = 0; fd < FD_SETSIZE; ++fd)
		{
			if (FD_ISSET(fd, &readfds))
			{
				if (fd == server)
				{
					len = sizeof(cli);
					connfd = accept(server, (struct sockaddr *)&cli, &len);
					if (connfd < 0)
						fatalError();
					FD_SET(connfd, &fds);
					g_clients[connfd].id = g_id++;
					g_clients[connfd].buf = 0;
					sprintf(g_buf, "server: client %d just arrived\n", g_clients[connfd].id);
					sendAll(connfd, g_buf);
				}
				else
				{
					int ret = recv(fd, g_buf, 1024, 0);
					g_buf[ret] = '\0';

					if (ret <= 0)
					{
						free(g_clients[fd].buf);
						FD_CLR(fd, &fds);
						close(fd);
						sprintf(g_buf, "server: client %d just left\n", g_clients[fd].id);
						sendAll(fd, g_buf);
					}
					else
					{
						g_clients[fd].buf = str_join(g_clients[fd].buf, g_buf);
						char *msg;
						while ((ret = extract_message(&g_clients[fd].buf, &msg)))
						{
							if (ret == -1)
								fatalError();
								
							sprintf(g_buf, "client %d: ", g_clients[fd].id);
							sendAll(fd, g_buf);
							sendAll(fd, msg);
							free(msg);
						}
					}
				}
			}
		}
	}

}