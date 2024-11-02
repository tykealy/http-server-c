#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

char *extract_path(char *buffer)
{
	char *start = strchr(buffer, ' ') + 1;
	if (!start)
		return NULL;
	char *end = strchr(start, ' ');
	if (!end)
		return NULL;
	int len = end - start;
	char *path = (char *)malloc(len + 1);
	strncpy(path, start, len);
	path[len] = '\0';
	return path;
};

int main()
{
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;
	char buffer[1024] = {0};

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int id = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	printf("Client connected\n");

	int bytes_received = recv(id, buffer, sizeof(buffer) - 1, 0);

	if (bytes_received < 0)
	{
		printf("Receive failed: %s \n", strerror(errno));
		return 1;
	}

	char *known_path[3] = {"/", "/echo", "/user-agent"};
	char *path = extract_path(buffer);

	if (strstr(path, known_path[1]) != NULL)
	{
		char *body = path + strlen(known_path[1]) + 1;
		char response[1024];
		snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s", strlen(body), body);
		send(id, response, sizeof(response), 0);
		send(id, path, strlen(path), 0);
	}
	else if (strstr(path, known_path[2]) != NULL)
	{
		char *headers = strstr(buffer, "\r\n") + 2;
		char *headers_end = strstr(headers, "\r\n\r\n");

		if (headers_end != NULL)
		{
			*headers_end = '\0'; // change from '/r/n/r/n' to '/0'(string end)
		}
		char *line = strtok(headers, "\r\n");
		while (line != NULL)
		{
			if (strstr(line, "User-Agent: ") != NULL)
			{
				char response[2048];
				char *agent = line + strlen("User-Agent: ");
				// printf("User-Agent: %s\n", line + strlen("User-Agent: "));
				snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s", strlen(agent), agent);
				send(id, response, sizeof(response), 0);
				break;
			}
			line = strtok(NULL, "\r\n");
		}
		char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
		send(id, response, sizeof(response), 0);
	}
	else if (!path || strcmp(path, known_path[0]) != 0)
	{
		char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
		send(id, response, sizeof(response), 0);
	}
	else
	{
		char response[] = "HTTP/1.1 200 OK\r\n\r\n";
		send(id, response, sizeof(response), 0);
	}

	close(server_fd);

	return 0;
}
