namespace Bavardage {
    [CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "../common/common.h")]
    namespace Common {
        public const int MAX_NAME_SIZE;
        public const int MAX_MESS_SIZE;
        public const int MAX_ROOM_NAME_SIZE;
        public const int CREATE_ROOM;
        public const int DELETE_ROOM;
        public const int QUIT_ROOM;
        public const int JOIN_ROOM;
        public const int DISCONNECT;
        public const int CONNECT;
        public const int MESSAGE;
        public const int OK;
        public const int KO;
        public const int LOGIN_IN_USE;
        public const int NOT_CONNECTED;
        public const int CONNECTED;
        public const int USER_LIST_CHUNK;
        public const int USER_LIST_END;
        public const int MP;
        public const int ADD_USER;
        public const int RM_USER;
        public const int NEW_USER;
    }

    
    [CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "libclient.h")]
    namespace ClientCore {
        public int send_message (string m);
        public int send_command ();
        public int connect_socket (string addr, int port);
        public int disconnect ();
        public int get_last_request_code ();
        public bool is_connected ();
        public int receive_message (out Message m);
        public string get_login ();
    }
}
