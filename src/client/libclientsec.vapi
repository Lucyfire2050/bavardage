namespace Bavardage {
    [CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "../common/commonsec.h")]
    namespace CommonSec {
        public const int CREATE_ROOM_SEC;
        public const int DELETE_ROOM_SEC;
        public const int QUIT_ROOM_SEC;
        public const int JOIN_ROOM_SEC;
        public const int DISCONNECT_SEC;
        public const int CONNECT_SEC;

        public void init_OpenSSL ();
    }

    
    [CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "libclientsec.h")]
    namespace ClientSecCore {
        public int connect_with_authentication (string chatservaddr, int chatservport, string secservaddr, int secservport);

        public int disconnect_servers ();

        public void set_certif_filename (string filename);

        public void set_private_key_filename (string filename);

        public int send_message_sec (string message, out string error);
    }
}