#include "lib_client.h"
#include "../common/common.h"
#include "../common/SocketTCP.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

SocketTCP *client_sock;
message *msg;
char *login, **tab_string;
int status = NOT_CONNECTED;
pthread_t thread_send, thread_recv;

//gestion des signaux d'interruption
void my_sigaction (int s) {
	switch (s) {
	case SIGINT:
		disconnect ();
		break;
	default:
		break;
	}
}

//Decoupage d'une chaine de caractère en fonction d'une taille donnée
char *str_sub (const char *s, unsigned int start, unsigned int end) {
	char *new_s = NULL;
	if (s != NULL && start < end) {
		new_s = malloc (sizeof(*new_s) * (end - start + 2));
		if (new_s != NULL) {
			int i;
			for (i = start; i < end; i++) {
				new_s[i - start] = s[i];
			}
			new_s[i - start] = '\0';
		} else {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
	return new_s;
}

//extraction du code d'une commande
int extract_code (const char *str) {
	char *command = NULL;
	command = str_sub (str, 1, strlen (str));

	if (strcmp (command, "CREATE_ROOM") == 0) {
		return CREATE_ROOM;
	} else if (strcmp (command, "DELETE_ROOM") == 0) {
		return DELETE_ROOM;
	} else if (strcmp (command, "DISCONNECT") == 0) {
		return DISCONNECT;
	} else if (strcmp (command, "CONNECT") == 0) {
		return CONNECT;
	} else if (strcmp (command, "QUIT_ROOM") == 0) {
		return QUIT_ROOM;
	} else if (strcmp (command, "JOIN_ROOM") == 0) {
		return JOIN_ROOM;
	} else if (strcmp (command, "MESSAGE") == 0) {
		return MESSAGE;
	} else if (strcmp (command, "MP") == 0) {
		return MP;
	}

	return -1;
}

//Lancement des threads de réception et d'envoi
int start_communication () {
	(void) signal (SIGINT, my_sigaction);
	pthread_create (&thread_recv, NULL, traitement_recv, NULL);
	pthread_create (&thread_send, NULL, traitement_send, NULL);

	pthread_join (thread_recv, NULL);
	pthread_join (thread_send, NULL);

	return 0;
}

//Création du socket
int connect_socket (const char *addr, const int port) {
	int co;
	if ((client_sock = creerSocketTCP ()) == NULL) {
		perror ("creerSocketTCP");
		return -1;
	}
	printf ("Please wait for connecting the server...\n");
	if ((co = connectSocketTCP (client_sock, addr, port)) == -1) {
		perror ("connectSocketTCP");
		return -1;
	}

	printf ("You can now send commands and messages\n");
	start_communication ();

	return 0;
}

//traitement des commandes saisies par l'utilisateur
void *traitement_send (void *param) {
	char mess[MAX_MESS_SIZE] = "";
	while (fgets (mess, MAX_MESS_SIZE, stdin) != NULL) {
		mess[strlen (mess) - 1] = '\0';
		send_message (mess);
	}
	pthread_exit (0);
}

//Traitement des commandes reçues par le client
void *traitement_recv (void *param) {
	message mess;
	while (1) {

		if (readSocketTCP (client_sock, (char*) &mess, sizeof(message)) > 0) {			
		} else {
			perror ("readSocketTCP");
			break;
		}

		if (msg->code == CONNECT && mess.code == OK && status == NOT_CONNECTED) {
			printf (
					"You're now connected to the chat server with the login: %s\n",
					login);
			status = CONNECTED;
		}

		if (msg->code == CONNECT && mess.code == KO && status == NOT_CONNECTED) {
			printf ("Error : %s\n", mess.content);
		}

		if (msg->code == CREATE_ROOM && mess.code == OK && status == CONNECTED) {
			printf ("You've successfully created a room named: %s\n",
					msg->content);
		}

		if (msg->code == CREATE_ROOM && mess.code == KO) {
			printf ("Error : %s\n", mess.content);
		}

		if (msg->code == JOIN_ROOM && mess.code == OK && status == CONNECTED) {
			printf ("You've successfully joined a room named: %s\n",
					msg->content);
		}

		if (msg->code == MESSAGE && mess.code == KO) {
			printf ("Error : %s\n", mess.content);
		}

		if (msg->code == MESSAGE && mess.code == OK && status == CONNECTED) {
			printf ("You've successfully send a public message \"%s\"\n",
					msg->content);
		}

		if (mess.code == MESSAGE && status == CONNECTED) {
			printf ("[%s @ %s] %s\n", mess.sender, mess.receiver, mess.content);
		}

		if (msg->code == MP && mess.code == KO) {
			printf ("Error : %s\n", mess.content);
		}

		if (mess.code == MP && status == CONNECTED) {
			printf ("[%s > %s] %s\n", mess.sender, mess.receiver, mess.content);
		}

		if (msg->code == JOIN_ROOM && mess.code == KO) {
			printf ("The room does not exist\n");
		}
		char * res = NULL;
		if (msg->code == JOIN_ROOM && mess.code == USER_LIST_CHUNK) {
			printf ("You joined the room %s\n", msg->content);
			res = strdup (mess.content);
			while (mess.code == USER_LIST_CHUNK) {
				readSocketTCP (client_sock, (char*) &mess, sizeof(message));
				strcat (res, ", ");
				strcat (res, mess.content);
			}
		}

		if (mess.code == USER_LIST_END) {
			printf ("The user %s joined the room \n", mess.sender);
		}

		if (msg->code == QUIT_ROOM && mess.code == OK && status == CONNECTED) {
			printf ("You've successfully quitted the room %s\n", msg->content);
		}

		if (msg->code == DELETE_ROOM && status == CONNECTED) {
			printf ("Room successfully deleted: %s\n", msg->content);
		}
		if (msg->code == DELETE_ROOM && mess.code == KO) {
			printf ("Error: %s\n", mess.content);
		}
		if (mess.code == DELETE_ROOM) {			
			printf ("The room %s has been deleted\n", mess.content);
		}

		if (msg->code == DISCONNECT && mess.code == OK) {
			closeSocketTCP (client_sock);
			printf ("You're now disconnected from the chat server\n");
			pthread_detach (thread_send);
			exit (0);
		}
	}
	pthread_exit (0);
}

//Calcul de la taille d'un tableau de chaines de caractères
int len (char **tab) {
	int n = 0;
	if (tab == NULL)
		return n;
	else {
		int l = 0;
		while (tab[l] != NULL) {
			n++;
			l += sizeof(char *) / sizeof(tab[l]);
		}
		return n;
	}
}

//Envoi des commandes au serveur
int send_command () {
	if (msg == NULL) {
		msg = (message*) malloc (sizeof(message));
	}
	if (login != NULL)
		strcpy (msg->sender, login);
	writeSocketTCP (client_sock, (char*) msg, sizeof(message));

	return 0;
}

//Créer un tableau a partir d'une chaine
char **create_table_param (const char *string) {
	char **res = (char **) malloc (sizeof(char*) * MAX_MESS_SIZE);
	int i = 0;
	if (string != NULL) {
		char *tok = strtok (strdup (string), " ");
		while (tok != NULL) {
			res[i] = tok;
			tok = strtok (NULL, " ");
			i++;
		}
	}
	return res;
}

//Envoi un message au serveur
int send_message (const char *mess) {
	int code;
	char buffer[strlen (mess)];
	strcpy (buffer, mess);
	msg = (message*) malloc (sizeof(message));

	if (mess[0] == '/') {
		code = extract_code (strtok (strdup (buffer), " "));
		if (code == -1) {
			perror ("extract_code");
			return -1;
		}
		msg->code = code;
		char *tmp, buff[MAX_MESS_SIZE] = "";
		int i;
		switch (code) {
		case CONNECT:	// Cas d'une demande de connexion
			if (status == NOT_CONNECTED) {
				tmp = strtok (NULL, " ");
				if (tmp != NULL) {
					login = strdup (tmp);
				}
				if (login == NULL) {
					printf ("login null\n");
				}
				strcpy (msg->sender, login);
				send_command ();
			}
			break;

		case DISCONNECT:	// Cas d'une demande de déconnexion
			strcpy (msg->sender, login);
			disconnect ();
			break;

		case CREATE_ROOM:	// Cas d'une demande de création de Salon
			tmp = strtok (NULL, " ");
			if (tmp != NULL) {
				strcpy (msg->content, tmp);
			}
			send_command ();
			break;
		case DELETE_ROOM:
			tmp = strtok (NULL, " ");
			if (tmp != NULL) {
				strcpy (msg->content, tmp);
			}
			send_command ();
			break;
		case QUIT_ROOM:		// Cas d'une demande pour quitter une room
			tmp = strtok (NULL, " ");
			if (tmp != NULL) {
				strcpy (msg->content, tmp);
			}
			strcpy (msg->sender, login);
			send_command ();

			break;

		case JOIN_ROOM:
			tmp = strtok (NULL, " ");
			if (tmp != NULL) {
				strcpy (msg->content, tmp);
			}
			send_command ();
			break;

		case MESSAGE:  // Cas d'envoi de message
			tab_string = create_table_param (buffer);
			strcpy (msg->receiver, tab_string[1]);
			for (i = 2; i < len (tab_string); i++) {
				strcat (buff, tab_string[i]);
				strcat (buff, " ");
			}

			strcpy (msg->content, buff);
			send_command ();
			break;

		case MP:  // Cas d'envoi de message privé
			tab_string = create_table_param (buffer);
			strcpy (msg->receiver, tab_string[1]);
			for (i = 2; i < len (tab_string); i++) {
				strcat (buff, tab_string[i]);
				strcat (buff, " ");
			}

			strcpy (msg->content, buff);
			send_command ();
			break;
		}
	}
	return 0;
}

//Déconnexion du client
int disconnect () {
	msg->code = DISCONNECT;
	send_command ();
	return 0;
}

int main (int argc, char *argv[]) {
	if (argc < 3) {
		fprintf (stderr, "Usage: ./client ip_server port_server\n");
		exit (EXIT_FAILURE);
	}
	connect_socket (argv[1], atoi (argv[2]));
	return 0;
}

