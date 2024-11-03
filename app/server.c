#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

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

void *handle_client(void *arg)
{
	int id = (intptr_t)arg;
	char buffer[1024] = {0};
	int bytes_received = recv(id, buffer, sizeof(buffer) - 1, 0);

	if (bytes_received < 0)
	{
		printf("Receive failed: %s \n", strerror(errno));
		return NULL;
	}

	char *known_path[4] = {"/", "/echo", "/user-agent", "files"};
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
	else if (strstr(path, known_path[3]) != NULL)
	{
		char *filename = path + strlen(known_path[3]) + 2;
		char response[1024];
		char full_path[1024];
		snprintf(full_path, sizeof(full_path), "./tmp/%s", filename);
		printf("full_path: %s\n", full_path);
		FILE *file = fopen(full_path, "r");
		if (!file)
		{
			char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
			send(id, response, sizeof(response), 0);
			close(id);
			return NULL;
		}
		char content[1024] = {0};
		fgets(content, sizeof(content), file);
		fclose(file);
		snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n%s", strlen(content), content);
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
	free(path);
	close(id);
	return NULL;
}

int main()
{
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd, id;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;
	char buffer[1024] = {0};
	pthread_t thread_id;

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

	while (1)
	{
		client_addr_len = sizeof(client_addr);
		id = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (id == -1)
		{
			printf("Accept failed: %s \n", strerror(errno));
			continue;
		}
		printf("Client connected\n");

		if (pthread_create(&thread_id, NULL, handle_client, (void *)(intptr_t)id) != 0)
		{
			printf("Thread creation failed: %s \n", strerror(errno));
			close(id);
		}
		else
		{
			pthread_detach(thread_id);
		}
	}

	close(server_fd);
	return 0;
}