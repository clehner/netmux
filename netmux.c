/*
 * netmux.c -- an interactive socket server

 * with help from Beej's Guide to Network Programming
 * http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// based on example from: https://stackoverflow.com/a/3884402
pid_t pcreate(int fds[2], char **command) {
	/* Spawn a process from pfunc, returning it's pid. The fds array passed will
	 * be filled with two descriptors: fds[0] will read from the child process,
	 * and fds[1] will write to it.
	 * Similarly, the child process will receive a reading/writing fd set (in
	 * that same order) as arguments.
	*/
	pid_t pid;
	int pipes[4];

	// Parent read/child write pipe
	if (pipe(&pipes[0]) < -1) {
		perror("pipe");
		exit(1);
	}
	// Child read/parent write pipe
	if (pipe(&pipes[2]) < -1) {
		perror("pipe");
		exit(1);
	}

	// Start the child process
	if ((pid = fork()) == -1) {
		perror("fork");
		exit(1);		
	}

	if (pid) {
		// Parent process
		fds[0] = pipes[0];
		fds[1] = pipes[3];

		close(pipes[1]);
		close(pipes[2]);

		return pid;

	} else {
		// Child process
		close(pipes[0]);
		close(pipes[3]);

		// Replace stdout with write end of pipe
		dup2(pipes[1],1);

		// Replace stdin with read end of pipe
		dup2(pipes[2],0);

		return execvp(command[0], command);
	}
}

int main(int argc, char *argv[])
{
	fd_set master;	// master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select()
	int fdmax;		// maximum file descriptor number
	char *port;	   // port we're listening on

	int num_clients;  // number of clients connected
	int child_pid;	// pid of child process running command
	int c_pipe[2];	// pipe for communicating with child

	int listener;	 // listening socket descriptor
	int newfd;		// newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	char buf[256];	// buffer for clienta and server data
	int bytes;		// number of bytes read into the buffer
	int active_client = 0;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes=1;		// for setsockopt() SO_REUSEADDR, below
	int i, j, rv;

	struct addrinfo hints, *ai, *p;

	if (argc <= 2) {
		fprintf(stderr, "Usage: %s port [command...]\n", argv[0]);
		return 1;
	}
	port = argv[1];
	pcreate(c_pipe, &argv[2]);

	FD_ZERO(&master);	// clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
		fprintf(stderr, "%s: %s\n", argv[0], gai_strerror(rv));
		exit(1);
	}
	
	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) { 
			continue;
		}
		
		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "%s: failed to bind\n", argv[0]);
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// listen to stdin
	FD_SET(0, &master);

	// listen to pipe
	FD_SET(c_pipe[0], &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	if (c_pipe[0] > fdmax) {	// keep track of the max
		fdmax = c_pipe[0];
	}

	// keep track of how many clients are connected
	num_clients = 0;

	// main loop
	for(;;) {
		read_fds = master; // copy it

		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) < 0) {
			perror("select");
			exit(4);
		}

		if (FD_ISSET(0, &read_fds)) {
			// handle message from stdin
			if (fgets(buf, sizeof buf, stdin) == NULL) {
				if (ferror(stdin)) {
					perror("fgets");
					exit(1);
				}
				if (feof(stdin)) {
					// need to clean up sockets?
					exit(0);
				}
			} else {
				// we got some data from stdin
			}
		}

		if (FD_ISSET(listener, &read_fds)) {
			// handle new connections
			addrlen = sizeof remoteaddr;
			newfd = accept(listener,
				(struct sockaddr *)&remoteaddr,
				&addrlen);

			if (newfd == -1) {
				perror("accept");
			} else {
				FD_SET(newfd, &master); // add to master set
				if (newfd > fdmax) {	// keep track of the max
					fdmax = newfd;
				}
				// send initial data
				num_clients++;
				printf("client %d connected: %s\n", num_clients,
					inet_ntop(remoteaddr.ss_family,
						get_in_addr((struct sockaddr*)&remoteaddr),
						remoteIP, INET6_ADDRSTRLEN));
			}
		}

		if (FD_ISSET(c_pipe[0], &read_fds)) {
			bytes = read(c_pipe[0], buf, sizeof buf);
			if (bytes < 0) {
				perror("read");
			} else if (bytes == 0) {
				// Pipe closed
				fprintf(stderr, "Pipe closed\n");
				exit(0);
			} else {
				// Send the data the last active client
				if (send(active_client, buf, bytes, 0) == -1) {
					perror("send");
				}
			}
		}

		// run through the existing connections looking for data to read
		for(i = 1; i <= fdmax; i++) {
			if (i != listener && i != c_pipe[0] && i != c_pipe[1] &&
					FD_ISSET(i, &read_fds)) {
				// handle data from a client
				bytes = recv(i, buf, sizeof buf, 0);
				if (bytes <= 0) {
					// got error or connection closed by client
					close(i); // bye!
					FD_CLR(i, &master); // remove from master set

					num_clients--;
					printf("client %d disconnected\n", num_clients+1);
				} else {
					// Send data to the pipe
					if (write(0, buf, bytes) < 0) {
						perror("write");
					}

					if (write(c_pipe[1], buf, bytes) < 0) {
						perror("write");
					}
					// This is now the active client
					active_client = i;
				}
			}
		}
	}
	
	return 0;
}
