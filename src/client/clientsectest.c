#include "libclientsec.h"
#include "../common/commonsec.h"

#define CHATADDR "localhost"
#define CHATPORT 10000

#define SECADDR  "localhost"
#define SECPORT  11000

int main (int argc, char *argv[]) {

	init_OpenSSL ();
	//seed_prng();
	//SSL_CTX *ctx;
	//ctx = setup_client_ctx();

	//set_private_key_filename ("toto.pem");
	//set_certif_filename ("toto_certif.pem");
	connect_with_authentication (CHATADDR, CHATPORT, argv[1], SECADDR, SECPORT);
	char *err;
	send_message_sec ("/CONNECT_SEC tototo", &err);
	return 0;
}
