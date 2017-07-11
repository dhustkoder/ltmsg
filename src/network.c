#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "io.h"
#include "network.h"
#include "upnp.h"


static inline bool host(void);
static inline bool client(void);


static struct ConnectionInfo cinfo;


const struct ConnectionInfo* initializeConnection(const enum ConnectionMode mode)
{
	if (mode == CONMODE_HOST) {
		cinfo.local_uname = cinfo.host_uname;
		cinfo.remote_uname = cinfo.client_uname;
	} else if (mode == CONMODE_CLIENT) {
		cinfo.local_uname = cinfo.client_uname;
		cinfo.remote_uname = cinfo.host_uname;
	} else {
		fprintf(stderr, "Unknown ConnectionMode value specified.\n");
		return NULL;
	}

	cinfo.mode = mode;
	askUserFor("Enter your username: ", cinfo.local_uname, UNAME_SIZE);
	askUserFor("Enter the connection port: ", cinfo.port, PORT_STR_SIZE);
	
	if (mode == CONMODE_HOST) {
		if (!host())
			return NULL;
		write(cinfo.remote_fd, cinfo.host_uname, UNAME_SIZE);
		read(cinfo.remote_fd, cinfo.client_uname, UNAME_SIZE);
		write(cinfo.remote_fd, cinfo.client_ip, IP_STR_SIZE);
		read(cinfo.remote_fd, cinfo.host_ip, IP_STR_SIZE);
	} else {
		if (!client())
			return NULL;
		read(cinfo.remote_fd, cinfo.host_uname, UNAME_SIZE);
		write(cinfo.remote_fd, cinfo.client_uname, UNAME_SIZE);
		read(cinfo.remote_fd, cinfo.client_ip, IP_STR_SIZE);
		write(cinfo.remote_fd, cinfo.host_ip, IP_STR_SIZE);
	}
	
	return &cinfo;
}


void terminateConnection(const struct ConnectionInfo* const cinfo)
{
	if (cinfo->mode == CONMODE_HOST) {
		close(cinfo->remote_fd);
		close(cinfo->local_fd);
		terminate_upnp();
	} else if (cinfo->mode == CONMODE_CLIENT) {
		close(cinfo->remote_fd);
	}
}


static inline bool host(void)
{
	if (!initialize_upnp(cinfo.port))
		return false;

	/* socket(), creates an endpoint for communication and returns a
	 * file descriptor that refers to that endpoint                */
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		goto Lterminate_upnp;
	}

	/* The setsockopt() function shall set the option specified by the
	 * option_name argument, at the protocol level specified by the level
	 * argument, t the value pointed to by the option_value argument
	 * for the socket associated with the file descriptor specified by the socket
	 * argument
	 * */
	const int optionval = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optionval, sizeof(int)) == -1) {
		perror("Couldn't set socket opt");
		goto Lclose_fd;
	}


	/* the bind() function shall assign a local socket address to
	 * a socket indentified by descriptor socket that has no local 
	 * socket address assigned. Sockets created with socket()
	 * are initially unnamed; they are identified only by their 
	 * address family
	 * */
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;          // IPv4 protocol
	servaddr.sin_addr.s_addr = INADDR_ANY;  // bind to any interface
	servaddr.sin_port = htons(strtoll(cinfo.port, NULL, 0));
	                                        /* htons() converts the unsigned short 
	                                         * int from hostbyte order to
						 * network byte order
						 * */
	if (bind(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
		perror("Couldn't bind");
		goto Lclose_fd;
	}

	/* the listen() function shall mark a connection-mode socket,
	 * specified by the socket argument, as accepting connections
	 * */
	if (listen(fd, 1) == -1) {
		perror("Couldn't set listen");
		goto Lclose_fd;
	}

	puts("Waiting for client...");

	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);

	/* the accept() system call is used with connection-based socket types
	 * (SOCK_STREAM, SOCK_SEQPACKET). It extracts the first connection 
	 * request on the queue of pending connections for the listening socket,
	 * sockfd, creates a new connected socket, and returns a new file
	 * descriptor referring to that socket. The newly created socket is not
	 * listening state. The original socket sockfd is unaffected by this call
	 * */
	const int clifd = accept(fd, (struct sockaddr*)&cliaddr, &clilen);

	if (clifd == -1) {
		perror("Couldn't accept socket");
		goto Lclose_fd;
	}

	if (inet_ntop(AF_INET, &cliaddr.sin_addr, cinfo.client_ip, IP_STR_SIZE) == NULL) {
		perror("Couldn't get client ip");
		goto Lclose_clifd;
	}

	cinfo.local_fd = fd;
	cinfo.remote_fd = clifd;
	return true;

Lclose_clifd:
	close(clifd);
Lclose_fd:
	close(fd);
Lterminate_upnp:
	terminate_upnp();
	return false;
}


static inline bool client(void)
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("Couldn't open socket");
		return false;
	}
	
	/* The gethostbyname() function returns a struct of type hostent
	 * for the given host name. Here name is either a hostname or an
	 * IPv4 address.
	 * */
	askUserFor("Enter the host IP: ", cinfo.host_ip, IP_STR_SIZE);
	struct hostent *hostent = gethostbyname(cinfo.host_ip);
	if (hostent == NULL) {
		perror("Couldn't get host by name");
		goto Lclose_fd;
	}

	/* The connect() function shall attempt to make a connection on a 
	 * mode socket or to set or reset the peer address of a connectionless
	 * mode socket.
	 * */
	struct sockaddr_in hostaddr;
	memset(&hostaddr, 0, sizeof(hostaddr));
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = htons(strtol(cinfo.port, NULL, 0));
	memcpy(&hostaddr.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
	if (connect(fd, (struct sockaddr*)&hostaddr, sizeof(hostaddr)) == -1) {
		perror("Couldn't connect");
		goto Lclose_fd;
	}

	cinfo.remote_fd = fd;
	return true;

Lclose_fd:
	close(fd);
	return false;
}

