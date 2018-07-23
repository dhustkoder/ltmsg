#ifndef LTMSG_NETWORK_H_
#define LTMSG_NETWORK_H_

#define UNAME_SIZE    (24)
#define IP_STR_SIZE   (24)
#define PORT_STR_SIZE (6)


enum ConnectionMode {
	CONMODE_HOST,
	CONMODE_CLIENT
};


struct ConnectionInfo {
	char host_uname[UNAME_SIZE];
	char client_uname[UNAME_SIZE];
	char host_ip[IP_STR_SIZE];
	char client_ip[IP_STR_SIZE];
	char port[PORT_STR_SIZE];
	char* local_uname;
	char* remote_uname;
	int local_fd;
	int remote_fd;
	enum ConnectionMode mode;
};


extern const struct ConnectionInfo* initialize_connection(enum ConnectionMode mode);
extern void terminate_connection(const struct ConnectionInfo* cinfo);



#endif
