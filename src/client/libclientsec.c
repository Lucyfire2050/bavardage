/* OpenSSL headers */
#include <string.h>

#include "libclientsec.h"
#include "libclient.h"



#define CAFILE "root.pem"
#define CADIR NULL

#define SALT_SIZE 16
#define HMAC_SIZE 20

char *private_key_filename;
char *certif_request_filename;
char *certif_filename;
char *root_certif_filename;
SocketTCP *secure_socket;

int is_connected = 0;
SSL_CTX *ctx;
SSL *ssl;
BIO *sbio;
message *msg;
int debug = 0;
int is_room_create = 0;

extern char *login, **tab_string;


//Récupération du nom du certificat pour le stocker dans une variable
int set_certif_filename (const char *certif_f) {
    certif_filename = strdup (certif_f);
}

//Récupération du nom du fichier de la requête pour le stocker dans une variable
int set_certif_request_filename (const char *certif_req_f) {
    certif_request_filename = strdup (certif_req_f);
}

//Récupération du nom de la clé pour le stocker dans une variable
int set_private_key_filename (const char *private_key_f) {
    private_key_filename = strdup (private_key_f);
}

//Fonction appelée lorsque la clé est chiffrée
char *pass = "";
int password_cb(char *buf,int num, int rwflag,void *userdata) {
    if(num<strlen(pass)+1)
        return(0);

    strcpy(buf,pass);
    return(strlen(pass));
}

//Récupération du mot de passe de la clé privé
void set_private_key_password (char *password) {
    pass = strdup (password);
}

//Récupération du nom du certificat racine pour le stocker dans une variable
void set_root_certif_filename (const char *filename) {
    root_certif_filename = strdup (filename);
}

//génération d'un sel pseudo aléatoire
unsigned char *gen_salt () {
    unsigned char *buf;
    buf = (unsigned char *) malloc (SALT_SIZE);
    RAND_add ("/dev/urandom", 10, 1.0);
    if (!RAND_bytes (buf, SALT_SIZE)) {
        fprintf (stderr, "The PRNG is not seeded!\n");
        abort ();
    }
    return buf;
}

//Génération d'une clé et d'un IV pour le chiffrement et déchiffrement des messages
key_iv gen_keyiv (unsigned char *master_key, unsigned char *salt) {
    int nrounds = 5;
    key_iv keyiv = malloc (sizeof (struct KEY_IV));
    EVP_BytesToKey (EVP_aes_256_cbc (), EVP_sha1 (), salt, master_key, 32, nrounds, keyiv->key, keyiv->iv);
    return keyiv;
}

//Initialisation du contexte SSL pour les communications sécurisées
SSL_CTX *setup_client_ctx (void) {
    //SSL_CTX *ctx;
    ctx = SSL_CTX_new (SSLv23_method ());
    SSL_CTX_set_default_passwd_cb (ctx, password_cb);
    if (SSL_CTX_load_verify_locations (ctx, root_certif_filename, CADIR) != 1)
        fprintf (stderr, "Error loading CA file and/or directory\n");
    if (SSL_CTX_use_certificate_chain_file (ctx, certif_filename) != 1)
        fprintf (stderr, "Error loading certificate from file\n");
    if (SSL_CTX_use_PrivateKey_file (ctx, private_key_filename, SSL_FILETYPE_PEM) != 1)
        fprintf (stderr, "Error loading private key from file\n");
    SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
    SSL_CTX_set_verify_depth (ctx, 4);

    
    return ctx;
}


//Création du socket sécurisé
int connect_secure_socket (const char *addr, const int port) {
    int co;
    if ((secure_socket = creerSocketTCP ()) == NULL) {
        fprintf (stderr, "Failed to create SocketTCP\n");
        return -1;
    }
    printf ("Please wait for connecting the server...\n");
    if ((co = connectSocketTCP (secure_socket, addr, port)) == -1) {
        fprintf (stderr, "Failed to connect the SocketTCP to %s:%d\n", addr, port);
        return -1;
    }

    //ctx = initialize_ctx (certif_filename, private_key_filename, password);
    ctx = setup_client_ctx ();
    ssl = SSL_new (ctx);
    sbio = BIO_new_socket (secure_socket->socket, BIO_NOCLOSE);
    SSL_set_bio (ssl, sbio, sbio);

    SSL_check_private_key (ssl);

    if (SSL_connect (ssl) <= 0)
        berr_exit ("SSL connect error");

    printf ("You can now send commands and messages\n");
    return 0;
}

//Gestion de la connexion avec authentification
int connect_with_authentication (char *chatservaddr, int chatservport,
                                 char *secservaddr, int secservport) {
    
    connect_socket (chatservaddr, chatservport);
    connect_secure_socket (secservaddr, secservport);
    
}

//Déconnexion du client des serveurs
int disconnect_servers () {
    disconnect ();
    disconnect_sec ();
}

//Initialisation d'une instance de l'implémentation par défaut de HMAC
int hmac_init (unsigned char *key, HMAC_CTX *hm_ctx, size_t keylen) {
    HMAC_Init (hm_ctx, key, keylen, EVP_sha1 ());
}

//Calcul du haché d'un message
unsigned char *compute_hmac (HMAC_CTX *hm_ctx, char *text, unsigned int *len) {
    // Compute the text hmac
    unsigned char out[HMAC_SIZE];
    
    HMAC_Update (hm_ctx, text, strlen (text));
    HMAC_Final (hm_ctx, out, len);

    return out;
}

//Hachage d'un texte et comparaison avec le haché mis en paramètre
int check_hmac (unsigned char *hash_key, char *text, unsigned char *hmac) {

    // Init the hmac context
    HMAC_CTX hm_ctx;
    hmac_init (hash_key, &hm_ctx, 16);

    // Compute the text hmac
    unsigned int len;
    unsigned char *out = compute_hmac (&hm_ctx, text, &len);
    // Check the 2 hmacs
    int same_hmac = 1;
    int i;
    for (i = 0; i < (int) len; i++) {
        same_hmac = same_hmac && (out[i] == hmac[i]);
    }

    // Clean up the context
    HMAC_cleanup (&hm_ctx);

    return same_hmac;
}

//Initialisation du contexte de chiffrement avec AES
int aes_init (unsigned char *key, unsigned char *iv, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx) {

    EVP_CIPHER_CTX_init (e_ctx);
    EVP_EncryptInit_ex (e_ctx, EVP_aes_256_cbc (), NULL, key, iv);
    EVP_CIPHER_CTX_init (d_ctx);
    EVP_DecryptInit_ex (d_ctx, EVP_aes_256_cbc (), NULL, key, iv);

    return 0;
}

//Fonction de chiffrement avec AES
char *aes_encrypt (keys k, char *plaintext, int *len) {
    char finaltext[MAX_MESS_SIZE];
    memset (finaltext, 0, MAX_MESS_SIZE);

    // Init the hmac context
    HMAC_CTX hm_ctx;
    hmac_init (k->hash_key, &hm_ctx, 16);
    unsigned int len_hm = HMAC_SIZE;
    memcpy (finaltext, compute_hmac (&hm_ctx, plaintext, &len_hm), HMAC_SIZE);

    EVP_CIPHER_CTX e_ctx, d_ctx;

    // Generate a salt
    unsigned char *salt = gen_salt ();

    // Generate the AES key and IV regarding the master key and the salt
    key_iv keyiv = gen_keyiv (k->master_key, salt);

    // Init the context and encrypt
    aes_init (keyiv->key, keyiv->iv, &e_ctx, &d_ctx);
    int c_len = *len + EVP_CIPHER_CTX_block_size(&e_ctx), f_len = 0;
    char *ciphertext = malloc (c_len);

    EVP_EncryptInit_ex (&e_ctx, NULL, NULL, NULL, NULL);
    EVP_EncryptUpdate (&e_ctx, ciphertext, &c_len, plaintext, *len);
    EVP_EncryptFinal_ex (&e_ctx, ciphertext + c_len, &f_len);
    
    *len = c_len + f_len;
    
    // Achieve the final packet
    memcpy (finaltext + HMAC_SIZE, salt, SALT_SIZE);
    memcpy (finaltext + HMAC_SIZE + SALT_SIZE, ciphertext, *len);

    return finaltext;
}

//Fonction de dechiffrement avec AES
char *aes_decrypt (keys k, char *ciphertext, int *len) {
    EVP_CIPHER_CTX e_ctx, d_ctx;

    // Retrieve HMAC from packet
    unsigned char hmac[HMAC_SIZE];
    memset (hmac, 0, HMAC_SIZE);
    memcpy (hmac, ciphertext, HMAC_SIZE);

    // Retrieve salt from packet
    unsigned char salt[SALT_SIZE];
    memcpy (salt, ciphertext + HMAC_SIZE, SALT_SIZE);

    // Generete key and iv regarding the master key and the salt
    key_iv keyiv = gen_keyiv (k->master_key, salt);

    // Init the context and decrypt
    aes_init (keyiv->key, keyiv->iv, &e_ctx, &d_ctx);
    int p_len = *len, f_len = 0;
    char *plaintext = malloc (*len + EVP_CIPHER_CTX_block_size(&d_ctx) + 1);

    EVP_DecryptInit_ex (&d_ctx, NULL, NULL, NULL, NULL);
    EVP_DecryptUpdate (&d_ctx, plaintext, &p_len, ciphertext + HMAC_SIZE + SALT_SIZE, *len);
    EVP_DecryptFinal_ex (&d_ctx, plaintext + p_len, &f_len);

    *len = p_len + f_len;

    // Check the message integrity
    int check = check_hmac (k->hash_key, plaintext, hmac);
    if (!check) {
        return "*** MESSAGE CORROMPU ***";
    }
    
    return plaintext;
}

//Fonction de dechiffrement d'un texte envoyé à une ROOM
char *decrypt (char *room_name, char *ciphered, int ciphered_size) {
    keys k = get_keys_from_room(room_name);
    int lenght = MAX_CIPHERED_SIZE;
    char *res = aes_decrypt(k, ciphered, &lenght);
    return res;
}

//Traitement des messages sécurisés reçus par le client
int receive_message_sec(message *m) {
    int ret = SSL_read (ssl, (char *) m, sizeof(message));
    
    if (ret == 0) {
        return -1;
    } else {
        keys k = NULL;
        int leng = 0;
        char *ciphermess = NULL;
        char text[MAX_MESS_SIZE] = "";
        switch (m->code) {
        case CREATE_ROOM_SEC:
        case JOIN_ROOM_SEC:
        case REFRESH_KEYIV:
        case MP_SEC_OK:
            k = malloc(sizeof (struct KEYS));
            memset (k, 0, sizeof (struct KEYS));
            char *room_name = strtok(strdup(m->content), "|");
            add_room (room_name, NULL);
            
            memcpy (k->master_key, m->content + strlen (room_name) + 1, 32);
            memcpy (k->hash_key, m->content + strlen (room_name) + 34, 16);
            set_keys_in_room(room_name, k);
            break;
        case DELETE_ROOM_SEC:
            remove_room (m->content);
            break;
        case QUIT_ROOM_SEC:
            remove_room (m->content);
            break;
        case DEL_ACCOUNT_SEC:
            send_message_sec ("/DISCONNECT_SEC", NULL);
            break;
        case MP_SEC:
            leng = strlen(m->content) + 1;
            
            k = get_keys_from_room (m->receiver);
            ciphermess = aes_encrypt(k, (char *)m->content, &leng);
            strcpy(text, "/MP ");
            strcat(text, m->receiver);
            strcat(text," ");
            memcpy (text + 5 + strlen (m->receiver), ciphermess, MAX_CIPHERED_SIZE);
            send_message(text, NULL);
            break;
        default:
            break;
        }
        
        return 0;
    }
}

//extraction du code d'une commande sécurisée
int extract_code_sec (const char *str) {
    if (str == NULL) {
        return -1;
    }
    char *command = NULL;
    command = str + 1;

    if (strcmp (command, "CREATE_ROOM_SEC") == 0) {
        return CREATE_ROOM_SEC;
    } else if (strcmp (command, "DELETE_ROOM_SEC") == 0) {
        return DELETE_ROOM_SEC;
    } else if (strcmp (command, "DISCONNECT_SEC") == 0) {
        return DISCONNECT_SEC;
    } else if (strcmp (command, "CONNECT_SEC") == 0) {
        return CONNECT_SEC;
    } else if (strcmp (command, "QUIT_ROOM_SEC") == 0) {
        return QUIT_ROOM_SEC;
    } else if (strcmp (command, "JOIN_ROOM_SEC") == 0) {
        return JOIN_ROOM_SEC;
    } else if (strcmp (command, "DEL_ACCOUNT_SEC") == 0) {
        return DEL_ACCOUNT_SEC;
    } else if (strcmp (command, "MP_SEC") == 0) {
        return MP_SEC;
    } else if (strcmp(command, "CONNECT_KO_SEC_OK") == 0) {
        return CONNECT_KO_SEC_OK;
    } else if (strcmp (command, "MESSAGE") == 0) {
        return MESSAGE;
    } else if (strcmp(command, "CONNECT_OK") == 0) {
        return CONNECT_OK;
    } else if (strcmp(command, "ACCEPT_JOIN_ROOM_SEC") == 0) {
	   return ACCEPT_JOIN_ROOM_SEC;
    } else if (strcmp(command, "REFUSE_JOIN_ROOM_SEC") == 0) {
	   return REFUSE_JOIN_ROOM_SEC;
    } else if (strcmp(command, "EJECT_FROM_ROOM_SEC") == 0) {
        return EJECT_FROM_ROOM_SEC;
    } else if (strcmp(command, "HELP") == 0) {
        return HELP;
    } 
    return -1;
}

//Envoi des commandes sécurisés au serveur
int send_command_sec () {
 
    if (secure_socket == NULL) {
        return -1;
    }
    if (msg == NULL) {
        msg = (message*) malloc (sizeof(message));
    }
    if (login != NULL)
        strcpy (msg->sender, login);
    else
        return -1;
 
    if (SSL_write (ssl, (char *) msg, sizeof(message)) < 0) {
        return (1);
    } 

    return 0;
}

//Envoi un message au serveur sécurisé
int send_message_sec (const char *mess, char **error_mess) {
    int code;
    char buffer[20 + MAX_NAME_SIZE + MAX_MESS_SIZE] = "";
    char *ciphermess;
    EVP_CIPHER_CTX en;
    EVP_CIPHER_CTX de;
    int lenght;
    keys k = NULL;
    unsigned char key[32], iv[32];
    unsigned char *keydata="test";
    keys kk = NULL;

    char text[MAX_MESS_SIZE] = "";
    strcpy(buffer, mess);
    buffer[strlen (buffer)] = '\0';

    msg = (message*) malloc (sizeof(message));
 
    if (mess[0] == '/') {
        code = extract_code_sec (strtok (strdup (buffer), " "));
        if (code == -1) {
            return -2;
        }
        msg->code = code;

        char *tmp, *pass, buff[MAX_MESS_SIZE] = "", conn[MAX_MESS_SIZE] = "";
        uint8_t *challenge;
        int i;
      
        switch (code) {
        case CONNECT_OK:
        
            msg->code = CONNECT_OK;
            send_command_sec();
            break;
        case CONNECT_KO_SEC_OK:
            disconnect_sec();
            break;
        case CONNECT_SEC:   // Cas d'une demande de connexion
			is_connected = 1;        
            tmp = strtok (NULL, " ");
            if (tmp != NULL) {
                login = strdup (tmp);
            } else {
                *error_mess = strdup ("Vous devez spécifier un pseudo");
                return -3;
            }

            msg->code = CONNECT_SEC;        
            if (login == NULL) {
                printf ("login null\n");
                return -1;
            } else {            
                strcpy (msg->sender, login);
                return send_command_sec ();
            }
            break;

        case DEL_ACCOUNT_SEC:        
            send_command_sec ();
            disconnect_sec ();
            break;

        case DISCONNECT_SEC:        // Cas d'une demande de déconnexion
            if (is_connected == 0)
				printf("you're not connected yet\n");
			else {
	            strcpy (msg->sender, login);
	            strcpy(conn, "/DISCONNECT ");
	            strcat(conn, login);
	            send_message (conn, &error_mess);
	            msg->code = DISCONNECT_SEC;
	            disconnect_sec ();          
			}
            break;

        case CREATE_ROOM_SEC:       // Cas d'une demande de création de Salon
			
            tmp = strtok (NULL, " ");
             if (tmp != NULL) {
                strcpy (msg->content, tmp);
            } else {
                *error_mess = strdup ("CREATE_ROOM a besoin d'un paramètre\n");
                return -3;
            }
			if (is_room_used (msg->content)) {
				*error_mess = strdup ("This room already exists\n");
				return -3;
			}
           
            add_room (msg->content, NULL);
            strcpy(conn, "/CREATE_ROOM ");
            strcat(conn, msg->content);
            send_message (conn, &error_mess);
			
            msg->code = CREATE_ROOM_SEC;
            strcpy (msg->content, tmp);
            int ret = send_command_sec ();
            return ret;

            break;

        case DELETE_ROOM_SEC:
            tmp = strtok (NULL, " ");
            if (tmp != NULL) {
                strcpy (msg->content, tmp);
            } else {
                *error_mess = strdup ("DELETE_ROOM a besoin d'un paramètre\n");
                return -3;
            }
            strcpy(text, "/DELETE_ROOM ");
            strcat(text, msg->content);     
            send_message(text, &error_mess);
            msg->code = DELETE_ROOM_SEC;
            return send_command_sec ();
            break;

        case QUIT_ROOM_SEC:         // Cas d'une demande pour quitter une room
            tmp = strtok (NULL, " ");
            if (tmp != NULL) {
                strcpy (msg->content, tmp);
            } else {
                *error_mess = strdup ("QUIT_ROOM a besoin d'un paramètre\n");
                return -3;
            }

            strcpy (text, "/QUIT_ROOM ");
            strcat (text, msg->content);
            send_message (text, error_mess);
            
            strcpy (msg->sender, login);
            msg->code = QUIT_ROOM_SEC;
            return send_command_sec ();

            break;

	case ACCEPT_JOIN_ROOM_SEC:
	    tab_string = create_table_param(buffer);
            if (len (tab_string) < 3) {
                *error_mess = strdup ("ACCEPT_JOIN_ROOM_SEC doit avoir 2 paramètres : /ACCEPT_JOIN_ROOM_SEC salon utilisateur\n");
                return -3;
            }
	    strcpy (msg->content, tab_string[1]);
	    strcpy (msg->receiver, tab_string[2]);
	    msg->code = ACCEPT_JOIN_ROOM_SEC;
            return send_command_sec ();
	    break;

    case EJECT_FROM_ROOM_SEC:
        tab_string = create_table_param(buffer);
        if (len (tab_string) < 3) {
            *error_mess = strdup ("EJECT_FROM_ROOM_SEC doit avoir 2 paramètres : /EJECT_FROM_ROOM_SEC salon utilisateur\n");
            return -3;
        }
        strcpy (msg->content, tab_string[1]);
        strcpy (msg->receiver, tab_string[2]);
        msg->code = EJECT_FROM_ROOM_SEC;
        return send_command_sec ();
        break;
	case REFUSE_JOIN_ROOM_SEC:
	    tab_string = create_table_param(buffer);
            if (len (tab_string) < 3) {
                *error_mess = strdup ("REFUSE_JOIN_ROOM_SEC doit avoir 2 paramètres : /MESSAGE salon utilisateur\n");
                return -3;
            }
	    strcpy (msg->content, tab_string[1]);
	    strcpy (msg->receiver, tab_string[2]);
	    msg->code = REFUSE_JOIN_ROOM_SEC;
            return send_command_sec ();
	    break;

        case JOIN_ROOM_SEC:
            tmp = strtok (NULL, " ");
            if (tmp != NULL) {
                strcpy (msg->content, tmp);
            } else {
                *error_mess = strdup ("JOIN_ROOM a besoin d'un paramètre\n");
                return -3;
            }
            return send_command_sec ();
            break;

        case MESSAGE:  // Cas d'envoi de message
            tab_string = create_table_param(buffer);
            if (len (tab_string) < 3) {
                *error_mess = strdup ("MESSAGE doit avoir 2 paramètres : /MESSAGE salon mon super message\n");
                return -3;
            }
            strcpy(msg->receiver, tab_string[1]);
            memcpy (buff, buffer + 10 + strlen (msg->receiver), MAX_MESS_SIZE);
            if(is_room_used(msg->receiver) == 0) {
                strcpy (msg->content, buff);
            } else {
                lenght = MAX_CIPHERED_SIZE;
                k = get_keys_from_room(msg->receiver);
                ciphermess = aes_encrypt(k, (char *)buff, &lenght);
                memcpy (msg->content, ciphermess, lenght);
            }
            free(tab_string);
            return send_command();
            break;

        case MP_SEC:  // Cas d'envoi de message prive
            tab_string = create_table_param(buffer);
            if (len (tab_string) < 3) {
                *error_mess = strdup ("MP_SEC doit avoir 2 paramètres : /MP toto mon super message privé\n");
                return -3;
            }
            strcpy(msg->receiver, tab_string[1]);
            strcpy (buff, "");
            for (i = 2; i < len(tab_string); i++) {
                strcat(buff, tab_string[i]);
                strcat(buff, " ");
            }

            if (lenght > MAX_MESS_SIZE) {
                *error_mess = strdup ("ce message est trop long pour être envoyé chiffré\n");
                return -3;
            }
            if (is_room_used(msg->receiver)==1) {
				lenght = strlen(buff) + 1;
                k = get_keys_from_room (msg->receiver);
				ciphermess = aes_encrypt(k, (char *)buff, &lenght);
				strcpy(msg->content,ciphermess);
				msg->code=MP;
				return send_command();			
			} else {
				msg->code=MP_SEC;
				strcpy(msg->content,buff);
				return send_command_sec();
			}
			free(tab_string);
            break;
            
       case HELP:
			printf("\n\n----------------------------------------------------------------------------------------------------\n");
			printf("------------------------------ %sthe manual utilisation for secure user %s------------------------------\n", KRED, KWHT);
			printf("----------------------------------------------------------------------------------------------------\n\n");
			
			printf("--------------------\nCLASSIC USER can do:\n--------------------\n");			
			printf("%s/CREATE_ROOM %sroom_name               %s- create a room named <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/DELETE_ROOM %sroom_name               %s- delete a room named <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/QUIT_ROOM %sroom_name                 %s- quit a room named <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/JOIN_ROOM %sroom_name                 %s- join a room named <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/DISCONNECT                          %s- end the connection\n", KBLU, KWHT);
			printf("%s/CONNECT %suser                        %s- connect the user <user>\n", KBLU, KCYN, KWHT);
			printf("%s/MP %suser message                     %s- send a message <message> to user <user>\n\n", KBLU, KCYN, KWHT);
			printf("%s/MESSAGE %sroom message                     %s- send the message <message> to the room <room>\n", KBLU, KCYN, KWHT);
            
			printf("--------------------\nSECURED USER can do:\n--------------------\n");
			printf("%s/CONNECT_SEC %suser user_certif user_key    %s- connect a secure user named <user_name>\n", KBLU, KCYN, KWHT);
			printf("                                            with the certicate <user_certif> and the key <user_key>\n");
			printf("%s/CREATE_ROOM_SEC %sroom_name                %s- create a new secure room with the name <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/DELETE_ROOM_SEC %sroom_name                %s- delete a secure romm with the name <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/DISCONNECT_SEC                             %s- disconnect a secure user if is connected\n", KBLU, KWHT);
			printf("%s/QUIT_ROOM_SEC %sroom_name                  %s- disconnect a connected user from the room <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/JOIN_ROOM_SEC %sroom_name                  %s- to join a secure room with the name <room_name>\n", KBLU, KCYN, KWHT);
			printf("%s/DEL_ACCOUNT_SEC                            %s- to delete your secure account\n", KBLU, KWHT);
			printf("%s/MP_SEC %suser message                      %s- send a private message <message> to the user <user>\n", KBLU, KCYN, KWHT);
			printf("%s/MESSAGE %sroom message                     %s- send the message <message> to the room <room>\n", KBLU, KCYN, KWHT);
			printf("%s/ACCEPT_JOIN_ROOM_SEC %sroom user           %s- authorize the user <user> to join the room <room>\n", KBLU, KCYN, KWHT);
			printf("%s/REFUSE_JOIN_ROOM_SEC %sroom user           %s- refuse the user <user> to join the room <room>\n", KBLU, KCYN, KWHT);
			printf("%s/EJECT_FROM_ROOM_SEC %sroom user            %s- revoque the user <user> from the room <room>\n\n", KBLU, KCYN, KWHT);
			
			printf("----------------------------------------------------------------------------------------------------\n");
			printf("--------------- %sNB: all commands in classic user can be executed by a secure user %s------------------\n", KRED, KWHT);
			printf("----------------------------------------------------------------------------------------------------\n\n");
			printf("", KRED, KWHT);
			break;
        }
    }
    return 0;
}

//Déconnexion du client sécurisé
int disconnect_sec () {
    if (secure_socket != NULL && msg != NULL) {
        msg->code = DISCONNECT_SEC;
        send_command_sec ();
    }
    return 0;
}

