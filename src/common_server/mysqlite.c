#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "mysqlite.h"


// variable globale
sqlite3 *database;
sqlite3_stmt *stmt;


/** connection la base de donnees **/
int connect_server_database(const char *fileDb) {

    // creation de la base de donnÃ©e si existe pas sinon ouverture de la base
    int open = sqlite3_open(fileDb, &database);
    if (open != SQLITE_OK) {
        perror("Database connection failed\n");
        sqlite3_close(database);
        return -1;
    }

    // creation de la table users
    char create_table[QUERY_SIZE] = "CREATE TABLE IF NOT EXISTS users (login VARCHAR(20) unique, is_connected INTEGER)";
    int sq = sqlite3_exec(database, create_table, 0, 0, 0);
    if (sq != SQLITE_OK) {
        perror("Can't create table users\n");
        return -1;
    }

    return 1;
}


/** fermer la base de donnÃ©es */
int close_server_database() {

    sqlite3_close(database);
    return 1;
}


/** ajoute un user dans la base **/
int add_user(char *login) {
  printf("BEGIN ADD_USER with login %s\n", login);
    // creation de la requete insertion
    char insert[QUERY_SIZE] = "";
    sprintf (insert, "INSERT INTO users values (\'%s\', 1)", login);

    int sql = sqlite3_exec(database, insert, 0, 0, 0);
    if (sql != SQLITE_OK) {
        perror("Can't insert in server database");
        return -1;
    } else {
        printf("Added user %s succesfully\n", login);
    }
    return 1;
}


/** supprime un user de la base **/
int delete_user (char *login) {

    char delete[QUERY_SIZE] = "";
    sprintf (delete, "DELETE FROM users WHERE login = \'%s\'", login);

    int sql = sqlite3_exec(database, delete, 0, 0, 0);
    if (sql != SQLITE_OK) {
        perror("Can't check in server database\n");
        return -1;
    } else {
        printf("User delete succesfully\n");
    }
  printf("END ADD_USER with login %s\n", login);
    return 1;
}


/** verifie la prÃ©sence d'un user */
int check_user(char *login) {
    char select[QUERY_SIZE] = "";
    sprintf (select, "SELECT * FROM users WHERE login = \'%s\'", login);

    int sql = sqlite3_prepare_v2(database, select, -1, &stmt, 0);
    if (sql) {
        perror("Cant select in server database\n");
        return -1;
    } else {
        int fetch = 1;
        int total = 0;
        while(fetch) {
            switch (sqlite3_step(stmt)) {
            case SQLITE_ROW:
                total++;
                break;
            case SQLITE_DONE:
                fetch = 0;
                break;
            default:
                printf("Some error encountered\n");
                return -1;
            }
        }
        if (total == 0) {
            return 1;
        } else {
            return -1;
        }
    }
}

/* determine si un user est connecte ou non */
int is_connected (char *login) {
  check_user(login);
  
  char is_connect[QUERY_SIZE] = "";
  sprintf (is_connect, "SELECT FROM users values (\'%s\') where is_connected = 1", login);

   int sq = sqlite3_exec(database, is_connect, 0, 0, 0);
   if (sq != SQLITE_OK) {
       perror("Can't verifie the connection status\n");
       return -1;
   }
    return 1;
}

/* change le status d'un utilisateur */
int change_status(char *login) {
  char ch[QUERY_SIZE] = "";
  sprintf (ch, "UPDATE users set is_connected = 0 where login = \'%s\'", login);
  int sq = sqlite3_exec(database, ch, 0, 0, 0);
  if (sq != SQLITE_OK) {
    perror("can't change status\n");
    return -1;
  }
  return 1;
}
