#include "lib_server.h"
#include "../common/common.h"
#include "../common/SocketTCP.h"
#include "../common/mysqlite.c"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>
#include <string.h>
#include <sqlite3.h>


SocketTCP *listen_socket;
pthread_mutex_t mutex;

void my_sigaction(int s) {
    switch (s) {
        case SIGINT:
            closeSocketTCP(listen_socket);
            break;
        default:
            break;
    }
}

int clear_message (message *m) {
	strcpy (m->name, "");
	strcpy (m->mess, "");
	strcpy (m->rooms, "");
	m->code = -1;
	
}

void *handle_connexion(void *param) {
        SocketTCP *s = (SocketTCP *) param;
		int receive;
		message buffer;
        message response;
        int is_connected = 0;
        while (1) {
			clear_message (&buffer);
			clear_message (&response);
			receive = readSocketTCP(s, (char *) &buffer, sizeof (message));
			if (receive > 0) {
        		switch (buffer.code) {
        		       	case CREATE_ROOM:
										break;
               
        		       	case QUIT_ROOM:
										break;

						case JOIN_ROOM:
										break;

        		       	case DISCONNECT:																				
										if (is_connected) {
											printf("Disconnection\n");
											response.code = OK;	
											delete_user (buffer.name);							
										} else {
												printf("Not connected\n");
												response.code = NOT_CONNECTED;
											} 
										writeSocketTCP(s, (char *) &response, sizeof(message));
										closeSocketTCP(s);                        
										pthread_exit(0);
										break;                        
					                                                             
        		       	case CONNECT:
										if (check_user(buffer.name)) {
										   response.code = LOGIN_IN_USE;
										   printf("login name already in use\n");
									     } else {
												  printf ("successful connection : %s\n", buffer.name);										
												  strcpy(response.name, buffer.name);
												  response.code = OK;
												  strcpy(response.mess, "successful connection");
										          is_connected = 1;
										          add_user(buffer.name);
											}
										
										break;

               			case MESSAGE:
										break;

						default:
								break;
           		}
	               writeSocketTCP(s, (char *) &response, sizeof(message));			
			}
			
      		}
		return NULL;
	}


void new_thread(SocketTCP *socket) {
  int ret;

  pthread_attr_t attr;
  if ((ret = pthread_attr_init(&attr)) != 0) {
    fprintf(stderr, "pthread_attr_init: %s\n", strerror(ret));
    exit(EXIT_FAILURE);
  }
  
  // On détache le thread afin de ne pas avoir à faire de join
  if ((ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
    fprintf(stderr, "pthread_attr_setdetachstate: %s\n", strerror(ret));
    exit(EXIT_FAILURE);
  }

  
  pthread_t t;
  if ((ret = pthread_create(&t, NULL, handle_connexion, (void*) socket)) != 0) {
    fprintf(stderr, "pthead_create: %s\n", strerror(ret));
    exit(EXIT_FAILURE);
  }

  if ((ret = pthread_attr_destroy(&attr)) != 0) {
    fprintf(stderr, "pthread_attr_destroy: %s\n", strerror(ret));
    exit(EXIT_FAILURE);
  }
}

int start_listening(const char *addr, int port) {
	SocketTCP *client;
	
        
    if ((listen_socket = creerSocketEcouteTCP(addr, port)) == NULL) {
        perror("creerSocketEcouteTCP");
        return -1;
     }

	(void) signal(SIGINT, my_sigaction);
    pthread_mutex_init(&mutex, NULL);
	while (1) {
        client = acceptSocketTCP(listen_socket); 									
		printf ("New connection\n");  
        new_thread (client);
	 }
	return -1;
}


int main(int argc, char *argv[]) {
	char r[QUERY_SIZE] = "server_database.db";
	connect_server_database(r);
	start_listening(argv[1], atoi(argv[2]));  
	close_server_database();  
	return -1;
}
