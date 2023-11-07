#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sqlite3.h"


using namespace std;

#define SERVER_PORT  9955
#define MAX_PENDING  5
#define MAX_LINE     10000

struct sockaddr_in srv;
char buf[MAX_LINE];
socklen_t buf_len, addr_len;
int nRet;
int nClient[10] = { 0, };
int nSocket;


sqlite3* db;
char* zErrMsg = 0;
const char* sql;
int rc;
string result;
string* ptr = &result;


typedef struct
{
    int socket;
    int id;
    string user;
    string password;
}userInfo;

typedef struct
{
    string ip;
    string user;
    int socket;
    pthread_t myThread;
}loggedUser;

void* temp = malloc(sizeof(userInfo));
userInfo u;

fd_set fr;
fd_set fw;
fd_set fe;
int nMaxFd;
pthread_t thread_handles;
long thread;

//Array to hold input from client when dealing with commands
string clientData[6];

//List of users
vector<loggedUser> list;
void NewClientConnection();
void DataFromClient();

string buildCommand(char*);

void* databaseCommands(void*);
static int callback(void*, int, char**, char**);
string getData(char*, string);
bool getData(char*, string*, string);


// Parses command from buffer sent from client 
string buildCommand(char line[]) {
    string command = "";
    size_t len = strlen(line);
    for (size_t i = 0; i < len; i++) {
        if (line[i] == '\n')
            continue;
        if (line[i] == ' ')
            break;
        command += line[i];
    }
    return command;
}

int callback_get_balance(void* user_balance, int argc, char** argv, char** azColName) {
    if (argc == 1) {
// Assuming that the result set has only one column (usd_balance)
        *((double*)user_balance) = atof(argv[0]);}
// Continue processing other rows if any
    return 0; }

int callback_get_quantity(void* seller_quantity, int argc, char** argv, char** azColName) {
    if (argc == 1) {
// Assuming that the result set has only one column (quantity)
        *((int*)seller_quantity) = atoi(argv[0]);}
// Continue processing other rows if any
    return 0; }



string clientPassword(char line[], int n) {
    int spaceLocation = n + 2;
    int i = spaceLocation;
    string info = "";
    while (line[i] != '\n') {
        if (line[i] == 0)
            return "";
        if (line[i] == ' ')
            return info;
        info += line[i];
        i++;
    }
    return info;
}
//FOR LOOKUP FUNCTION
int getMatchedRecordsCount(const string& records) {
                // Count the number of matched records in the formatted string.
                int count = 0;
                size_t pos = 0;
                // Search for line breaks in the string and count the occurrences
                while ((pos = records.find('\n', pos)) != string::npos) {
                    count++;
                    pos++;
                }
                return count;
}

int main(int argc, char* argv[]) {
#pragma region Database Setup
// Open Database and Connect to Database
    rc = sqlite3_open("cis427PokeCard2023.sqlite", &db);
// Check that Database was opened successfully
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    }
    else {
        fprintf(stderr, "Opened database successfully\n");
    }
// Create sql Users table creation command
    sql = "create table if not exists Users\
    (\
        ID INTEGER PRIMARY KEY AUTOINCREMENT,\
        first_name TEXT,\
        last_name TEXT,\
        user_name TEXT NOT NULL,\
        password TEXT,\
        email TEXT NOT NULL,\
        usd_balance DOUBLE NOT NULL\
    );";

// Execute Users table creation
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

// Create sql Pokemon_cards table creation command
    sql = "create table if not exists Pokemon_cards (\
        ID INTEGER PRIMARY KEY AUTOINCREMENT,\
        Card_name varchar(10) NOT NULL,\
        Card_type TEXT,\
        rarity TEXT,\
        quantity INTEGER,\
        Card_price DOUBLE,\
        owner_id TEXT,\
        FOREIGN KEY(owner_id) REFERENCES Users(ID)\
    );";

// Create Users Table Values
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
// First Check that there is no root user, then add.
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='root'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";



      
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }


    else if (result == "USER_NOT_PRESENT") {
// Create the root user:
        fprintf(stdout, "Root user is not present. Attempting to add the user.\n");
// Add a root user
        sql = "INSERT INTO Users VALUES (1, 'Root', 'User', 'root', 'root01', 'cis427PokemonCards@abc.com', 100);";
      
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }


        else {
            fprintf(stdout, "\tA root user was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tThe root user already exists in the Users table.\n";
    }
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }
// Check that john exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='j_doe'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "john is not present. Attempting to add the user.\n");
// Add john to Pokemon Database
        sql = "INSERT INTO Users VALUES (2, 'john', 'doe', 'j_doe', 'Passwrd4', 'j.doe@abc.com', 100);";
       // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (john) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tjohn already exists in the Users table.\n";
    }

//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that jane exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='j_smith'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "jane is not present. Attempting to add the user.\n");
// Add jane
        sql = "INSERT INTO Users VALUES (3, 'jane', 'smith', 'j_smith', 'pass456', 'j.smith@abc.com', 90);";
       
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (jane) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tjane already exists in the Users table.\n";
    }
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }   
// Check that charlie exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='c_brown'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "charlie is not present. Attempting to add the user.\n");
// Add charlie to Pokemon Database
        sql = "INSERT INTO Users VALUES (4, 'charlie', 'brown', 'c_brown', 'Snoopy', 'c.brown@abc.com', 90);";
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (charlie) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tcharlie already exists in the Users table.\n";
    }
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }
// Check that lucy exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='l_van'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "lucy is not present. Attempting to add the user.\n");
// Add lucy to Pokemon Database
        sql = "INSERT INTO Users VALUES (5, 'lucy', 'van', 'l_van', 'Football', 'l.van@abc.com', 70);";
        
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (lucy) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tlucy already exists in the Users table.\n";
    }

//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that linus exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='l_blanket'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "linus is not present. Attempting to add the user.\n");
// Add linus to Pokemon Database
        sql = "INSERT INTO Users VALUES (6, 'linus', 'blanket', 'l_blanket', 'security23', 'l.blanket@abc.com', 90);";
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (linus) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tlinus already exists in the Users table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }




//Adding new users based on P2 User Info Table Requirements
//NEW MARY
// Check that mary exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='mary'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "mary is not present. Attempting to add the user.\n");
// Add mary to Pokemon Database
        sql = "INSERT INTO Users VALUES (7, 'mary', 'ann', 'mary', 'mary01', 'mary@abc.com', 100);";
       // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (mary) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tmary already exists in the Users table.\n";
    }

//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

//NEW JOHN USER 
    // Check that john exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='john'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "john is not present. Attempting to add the user.\n");
// Add john to Pokemon Database
        sql = "INSERT INTO Users VALUES (8, 'john', 'doe', 'john', 'john01', 'john@abc.com', 100);";
       // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (john) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tjohn already exists in the Users table.\n";
    }

//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }



//NEW MOE USER
    // Check that moe exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE  Users.user_name='moe'), 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "USER_NOT_PRESENT") {
        fprintf(stdout, "moe is not present. Attempting to add the user.\n");
// Add moe to Pokemon Database
        sql = "INSERT INTO Users VALUES (9, 'moe', 'bob', 'moe', 'moe01', 'moe@abc.com', 100);";
       // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new user (moe) was added successfully.\n");
        }
    }
    else if (result == "USER_PRESENT") {
        cout << "\tmoe already exists in the Users table.\n";
    }

//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

    
// Check that Pikachu exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Pikachu'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Pikachu is not present. Attempting to add the card.\n");
// Add Pikachu to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (1, 'Pikachu', 'Electric', 'Common', 2, 19.99, 2);";
// sql = "INSERT INTO Pokemon_cards VALUES (1, 'Pikachu', 19.99, 'Electric', 'Common', 2);";
       
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new card (Pikachu) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tcard already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Charizard exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Charizard'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Charizard is not present. Attempting to add the card.\n");
// Add Charizard to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (2, 'Charizard', 'Fire', 'Rare', 1, 15.49, 2);";
       // rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new card (Charizard) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tCharizard already exists in the Pokemon_cards table.\n";
    }
     
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Bulbasaur exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Bulbasaur'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Bulbasaur is not present. Attempting to add the card.\n");
// Add Bulbasaur to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (3, 'Bulbasaur',  'Grass', 'Common', 50, 11.9, 4);";
          
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new card (Bulbasaur) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tBulbasaur already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Squirtle exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Squirtle'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Squirtle is not present. Attempting to add the card.\n");
// Add Squirtle to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (4, 'Squirtle', 'Water', 'Uncommon', 30, 18.99, 5);";
         
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        else {
            fprintf(stdout, "\tA new card (Squirtle) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tSquirtle already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Jigglypuff exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Jigglypuff'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Jigglypuff is not present. Attempting to add the card.\n");
// Add Jigglypuff to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (5, 'Jigglypuff', 'Normal', 'Common', 3, 24.99, 6);";

// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
  
        else {
            fprintf(stdout, "\tA new card (Jigglypuff) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tJigglypuff already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

    //Added more pokemon for new users in P2!
// Check that Keldeo exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Keldeo'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Keldeo is not present. Attempting to add the card.\n");
// Add Keldeo to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (6, 'Keldeo', 'Water', 'Rare', 1, 29.99, 7);";

// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
  
        else {
            fprintf(stdout, "\tA new card (Keldeo) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tKeldeo already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Torchic exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Torchic'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Torchic is not present. Attempting to add the card.\n");
// Add Torchic to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (7, 'Torchic', 'Fire', 'Common', 5, 2.99, 7);";

// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
  
        else {
            fprintf(stdout, "\tA new card (Torchic) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tTorchic already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Mudkip exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Mudkip'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Mudkip is not present. Attempting to add the card.\n");
// Add Mudkip to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (8, 'Mudkip', 'Water', 'Common', 10, 7.99, 8);";
  
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
  
        else {
            fprintf(stdout, "\tA new card (Mudkip) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tMudkip already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Mewtwo exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Mewtwo'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Mewtwo is not present. Attempting to add the card.\n");
// Add Mewtwo to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (9, 'Mewtwo', 'Psychic', 'Uncommon', 2, 32.49, 8);";
  
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
   
        else {
            fprintf(stdout, "\tA new card (Mewtwo) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tMewtwo already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Zygarde exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Zygarde'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Zygarde is not present. Attempting to add the card.\n");
// Add Zygarde to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (10, 'Zygarde', 'Dragon', 'Rare', 1, 35.00, 8);";

  // SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
  
        else {
            fprintf(stdout, "\tA new card (Zygarde) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tZygarde already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Lycanroc exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Lycanroc'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Lycanroc is not present. Attempting to add the card.\n");
// Add Lycanroc to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (11, 'Lycanroc', 'Rock', 'Rare', 3, 24.99, 9);";
  
// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
   
        else {
            fprintf(stdout, "\tA new card (Lycanroc) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tLycanroc already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }

// Check that Treecko exists. No? Create entry
    sql = "SELECT IIF(EXISTS(SELECT 1 FROM Pokemon_cards WHERE  Pokemon_cards.Card_name='Treecko'), 'POKEMON_CARD_PRESENT', 'POKEMON_CARD_NOT_PRESENT') result;";
    rc = sqlite3_exec(db, sql, callback, ptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else if (result == "POKEMON_CARD_NOT_PRESENT") {
        fprintf(stdout, "Treecko is not present. Attempting to add the card.\n");
// Add Treecko to Pokemon Database
        sql = "INSERT INTO Pokemon_cards VALUES (12, 'Treecko', 'Grass', 'Common', 15, 5.99, 9);";

// SQL Execute and Error Handling from Using Database
//Specific to INSERT, Initializing Database with Values
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
   
        else {
            fprintf(stdout, "\tA new card (Treecko) was added successfully.\n");
        }
    }
    else if (result == "POKEMON_CARD_PRESENT") {
        cout << "\tTreecko already exists in the Pokemon_cards table.\n";
    }
    
//ERROR INSERTING into Database
    else {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cout << "Error returned Result = " << result << endl;
    }




#pragma endregion

// Setup passive open & Initialize the socket
    nSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (nSocket < 0) {
        cout << "Socket not Opened\n";
        sqlite3_close(db);
        cout << "Closed DB" << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "Socket Opened: " << nSocket << endl;
    }
// Set Socket Options
    int nOptVal = 1;
    int nOptLen = sizeof(nOptVal);
    nRet = setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&nOptVal, nOptLen);
    if (!nRet) {
        cout << "The setsockopt call successful\n";
    }
    else {
        cout << "Failed setsockopt call\n";
        sqlite3_close(db);
        cout << "Closed Database" << endl;
        close(nSocket);
        //Print to screen which socket was closed
        cout << "Closed socket: " << nSocket << endl;
        exit(EXIT_FAILURE);
    }
// Build address data structure
    srv.sin_family = AF_INET;
    srv.sin_port = htons(SERVER_PORT);
    srv.sin_addr.s_addr = INADDR_ANY;
    memset(&(srv.sin_zero), 0, 8);
//Bind the socket to the local port
    nRet = (bind(nSocket, (struct sockaddr*)&srv, sizeof(srv)));
    if (nRet < 0) {
        cout << "Failed to bind to local port\n";
        sqlite3_close(db);
        cout << "Closed Database " << endl;
        close(nSocket);
        cout << "Closed socket: " << nSocket << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "Successfully bound to local port\n";
    }
// Listen for Client Requests
    nRet = listen(nSocket, MAX_PENDING);
    if (nRet < 0) {
        cout << "Failed to start listen to local port\n";
        sqlite3_close(db);
        cout << "Closed Database " << endl;
        close(nSocket);
        cout << "Closed socket: " << nSocket << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "Started listening to local port\n";
    }
    struct timeval tv;
    cout << "\nWaiting for connections ...\n";
    while (1)
    {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
// Set the FD_SET
        FD_ZERO(&fr);
        FD_SET(nSocket, &fr);
        nMaxFd = nSocket;
        for (int nIndex = 0; nIndex < 5; nIndex++)
        {
            if (nClient[nIndex] > 0)
            {
                FD_SET(nClient[nIndex], &fr);
            }
            if (nClient[nIndex] > nMaxFd)
                nMaxFd = nClient[nIndex];
        }
        nRet = select(nMaxFd + 1, &fr, NULL, NULL, &tv);
// Reset Bits
        if (nRet < 0)
        {
            cout << endl << "select api call failed. Will exit";
            return (EXIT_FAILURE);
        }
        else
        {
// There is some client waiting either to connect or some new data came from existing client.
            if (FD_ISSET(nSocket, &fr))
            {
// Handle New connection
                NewClientConnection();
            }
            else
            {
// Check what existing client got the new data
                DataFromClient();
            }
        }
    }



    for (int l = 0; l < 10; l++) {
        close(nClient[l]);
    }


    sqlite3_close(db);
    cout << "Closed DB" << endl;
    close(nSocket);
    //Display to screen socket that closed
    cout << "Closed socket: " << nSocket << endl;
    exit(EXIT_SUCCESS);
}

//Get information from the User and place into clientData array to be extracted in commands
string getData(char line[], string command) {
    int l = command.length();
    int spaceLocation = l + 1;
    int i = spaceLocation;
    string info = "";
//For correct output and lines
    while (line[i] != '\n') {
        if (line[i] == 0)
            return "";
        if (line[i] == ' ')
            return info;
        info += line[i];
        i++;
    }
    return info;
}



void* databaseCommands(void* userData) {
    cout << "Username: " << ((userInfo*)userData)->user << endl;;
    int clientIndex = ((userInfo*)userData)->socket;
    int clientID = nClient[((userInfo*)userData)->socket];
    nClient[clientIndex] = -1;
    cout << clientID << endl;
    int buf_len;
    string u = ((userInfo*)userData)->user;
    int idINT = ((userInfo*)userData)->id;
    string id = to_string(idINT);
    string command;
    bool rootUsr;
//No root? Error out
    if (idINT == 1) {
        rootUsr = true;
    }
    else {
        rootUsr = false;}
    while (1){
        char Buff[10000] = { 0, };
        while ((buf_len = (recv(clientID, Buff, sizeof(Buff), 0)))) {
// Print out received message per assignment
            cout << "SERVER: Received: " << Buff << endl;
// Parse message for initial command
            command = buildCommand(Buff);
//Already logged in error handling
            if (command == "LOGIN") {
                send(clientID, "You are already logged in!", 27, 0);}

     
//Function had to be completely redone. Had been unstable and inconsistent. This version is consitent. 
 else if (command == "BUY") {
    char * commandCount = (char*)malloc(sizeof(Buff));

strcpy(commandCount, Buff);
printf("Received: \"%s\"\n", commandCount);
    char card_name[50];
    char card_type[50];
    char rarity[50];
    double card_price;
    int quantity;
    int owner_id;
    char response[MAX_LINE];
    char successResponse[10];
    char sql_query[256];
    char sell_name[50];
    char* zErrMsg = 0;
    //Check the client did the right input
    if (sscanf(commandCount, "BUY %s %s %s %lf %d %d", card_name, card_type, rarity, &card_price, &quantity, &owner_id) != 6) {
        sprintf(response, "403 message format error\nInvalid BUY request format\n");
        send(clientID, response, strlen(response), 0);}


//Adding here for database upload fix
double new_price = card_price;
//adding quantities for updating buyer and seller calculations
int sell_q = quantity;
int update_sell_q = quantity;


// Query the database to get user balance
    sprintf(sql_query, "SELECT usd_balance FROM Users WHERE ID=%d;", clientID);
// Declare the variable to store the user's balance
    double user_balance; 
    int rc = sqlite3_exec(db, sql_query, callback_get_balance, &user_balance, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


// Check if user has enough balance, If not exit out of buying card
    if (user_balance < card_price * quantity) {
        sprintf(response, "Not enough balance.");
        printf("%s\n", response); 
        send(clientID, response, strlen(response), 0); }

//Else user has enough money so finish the purchase
else{
      sprintf(sql_query, "SELECT quantity FROM Pokemon_cards WHERE quantity=%d AND ID=%d;", sell_q, owner_id);

// Declare the variable to store the user's seller's quantity
        int seller_quantity;
    int rc_quantity = sqlite3_exec(db, sql_query, callback_get_quantity, &seller_quantity, &zErrMsg);
    if (rc_quantity != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


//Check if there are enough cards to sell/User has the cards, If dont own card quantity = 0!!!!
if(seller_quantity < quantity){
            sprintf(response, "Invalid Purchase Attempt");
        printf("%s\n", response); 
        send(clientID, response, strlen(response), 0);
}


else{
// Deduct the price changed to buy_id
    user_balance -= card_price * quantity;
    sprintf(sql_query, "UPDATE Users SET usd_balance=%.2lf WHERE ID=%d;", user_balance, clientID);
    rc = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

//Client doesn't own card, insert into database
else{
    sprintf(sql_query, "INSERT INTO Pokemon_Cards (card_name, card_type, rarity, quantity, card_price, owner_id) VALUES ('%s', '%s', '%s', %d, %f, %d);",
        card_name, card_type, rarity, quantity, new_price, clientID);

    rc = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


// Query the database to get seller balance
    sprintf(sql_query, "SELECT usd_balance FROM Users WHERE ID=%d;", owner_id);
// Declare the variable to store the user's balance
double seller_balance;
    int new_rc = sqlite3_exec(db, sql_query, callback_get_balance, &seller_balance, &zErrMsg);
    if (new_rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


// Add the price changed to seller's account
    seller_balance += card_price * quantity;
    sprintf(sql_query, "UPDATE Users SET usd_balance=%.2lf WHERE ID=%d;", seller_balance, owner_id);
    new_rc = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (new_rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


       
//Adding here to reduce seller's quantity
    sprintf(sql_query, "SELECT quantity FROM Pokemon_cards WHERE owner_id=%d;", owner_id);
int sell_quantity;
    int new_rc_update = sqlite3_exec(db, sql_query, callback_get_quantity, &sell_quantity, &zErrMsg);
    if (new_rc_update != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


sell_quantity = sell_quantity - update_sell_q;
    sprintf(sql_query, "UPDATE Pokemon_cards SET quantity=%d WHERE owner_id=%d;", sell_quantity, owner_id);
    new_rc_update = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (new_rc_update != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}




// Success response
    sprintf(response, "200 OK\nBOUGHT: New balance: %d %s. User USD balance $%.2lf\n", quantity, card_name, user_balance);
    send(clientID, response, strlen(response), 0);

//updating so only this sentence prints out instead of extra buffer information/list info, doesn't matter still shows
    //sprintf(successResponse, "200 OK\nBOUGHT: New balance: %d %s. User USD balance $%.2lf\n", quantity, card_name, user_balance);
    //send(clientID, successResponse, strlen(successResponse), 0);

}}}
}


//This function had been unstable and was not consistantly compiling so this also had to be completely redone
    else if (command == "SELL") {
        char * commandCount = (char*)malloc(sizeof(Buff));

strcpy(commandCount, Buff);
printf("Received: \"%s\"\n", commandCount);
    char card_name[50];
    char card_type[50];
    char rarity[50];
    int quantity;
    double card_price;
    int owner_id;
    char response[MAX_LINE];
    char sql_query[256];
    char* zErrMsg = 0;

// Parse the SELL command request
    if (sscanf(commandCount, "SELL %s %d %lf %d", card_name, &quantity, &card_price, &owner_id) != 4) {
        sprintf(response, "403 message format error\nInvalid SELL request format\n");
        send(clientID, response, strlen(response), 0);   }

// Calculate the total card price to be deposited to the user's balance
    double total_price = card_price * quantity;
//another quantity to do calculations in updating buyer and seller's quantities
int sell_q = quantity;
int update_sell_q = quantity;
int update_buy_q = quantity;


// Query the database to get buyer balance
    sprintf(sql_query, "SELECT usd_balance FROM Users WHERE ID=%d;", owner_id);
// Declare the variable to store the user's balance
double buyer_balance;
    int new_rc_bb = sqlite3_exec(db, sql_query, callback_get_balance, &buyer_balance, &zErrMsg);
    if (new_rc_bb != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

// Check if buyer has enough balance, If not exit out of selling card
    if (buyer_balance < card_price * quantity) {
        sprintf(response, "Not enough balance.");
        printf("%s\n", response); 
        send(clientID, response, strlen(response), 0); }

//Buyer has enough to sell the card to
else{
// Add the price changed to seller's account
    buyer_balance -= card_price * quantity;
    sprintf(sql_query, "UPDATE Users SET usd_balance=%.2lf WHERE ID=%d;", buyer_balance, owner_id);
    new_rc_bb = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (new_rc_bb != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


// Query the database to get user balance
    sprintf(sql_query, "SELECT usd_balance FROM Users WHERE ID=%d;", clientID);
// Declare the variable to store the user's balance
    double user_balance; 
    int rc = sqlite3_exec(db, sql_query, callback_get_balance, &user_balance, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

// Add the price changed to seller's account, no check required here since its just crediting money to current user
    user_balance += card_price * quantity;
    sprintf(sql_query, "UPDATE Users SET usd_balance=%.2lf WHERE ID=%d;", user_balance, clientID);
    rc = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

//insert card into database for buyer
string myCardSql = "SELECT Card_name as card_name, Card_type as card_type, rarity as rarity FROM Pokemon_cards WHERE Pokemon_cards.owner_id=" + owner_id;
        int insert_rc = sqlite3_exec(db, myCardSql.c_str(), callback, ptr, &zErrMsg);
        if (insert_rc != SQLITE_OK) {
            fprintf(stderr, "SQL error in trying the insert: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);}

string insertCard = "INSERT INTO Pokemon_cards (card_name, card_type, rarity, quantity, card_price, owner_id) VALUES (card_name, card_type, rarity, quantity, card_price,  owner_id);";
    insert_rc = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (insert_rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


 //Adding here to increase buyer's quantity
    sprintf(sql_query, "SELECT quantity FROM Pokemon_cards WHERE owner_id=%d;", owner_id);
int buy_quantity;
    int new_rc_buy = sqlite3_exec(db, sql_query, callback_get_quantity, &buy_quantity, &zErrMsg);
    if (new_rc_buy != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

buy_quantity = buy_quantity + update_buy_q;
//added card name!
    sprintf(sql_query, "UPDATE Pokemon_cards SET quantity=%d WHERE owner_id=%d;", buy_quantity, owner_id);
    new_rc_buy = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (new_rc_buy != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


//Adding here to reduce seller's quantity
    sprintf(sql_query, "SELECT quantity FROM Pokemon_cards WHERE owner_id=%d;", clientID);
int sell_quantity;
    int new_rc_update = sqlite3_exec(db, sql_query, callback_get_quantity, &sell_quantity, &zErrMsg);
    if (new_rc_update != SQLITE_OK) {
        //Error happening here
        fprintf(stderr, "SQL error Happening Here?: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}

sell_quantity = sell_quantity - update_sell_q;
//added card name!
    sprintf(sql_query, "UPDATE Pokemon_cards SET quantity=%d WHERE owner_id=%d;", sell_quantity, clientID);
    new_rc_update = sqlite3_exec(db, sql_query, 0, 0, &zErrMsg);
    if (new_rc_update != SQLITE_OK) {
        fprintf(stderr, "SQL error?: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);}


// Send a success response
    sprintf(response, "200 OK\nSOLD: New balance: %d %s. User USD balance $%.2lf\n", quantity, card_name, user_balance);
    send(clientID, response, strlen(response), 0);





}
    }

else if (command == "LIST") {
    string sendStr;
    result = ""; // Clear the result

    // Check if the user is a root user or a regular user
    if (id == "1") {
            result = ""; // Clear the result
        // Root user can list all pokemon card records and the owner's first name from Users
        string sql = "SELECT P.ID, P.Card_name, P.Card_type, P.rarity, P.quantity, P.Card_price, U.first_name "
                     "FROM Pokemon_cards P "
                     "JOIN Users U ON P.owner_id = U.ID;";
        rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
        sendStr = "The list of records in the cards database:\nID Card Name Type Rarity Count OwnerID\n" + result;

    } else {
        // Non-root user
        // Clear the result for new data
        result = "";

        // Retrieve the user's first name
        string userSql = "SELECT first_name FROM Users WHERE Users.ID=" + id;
        rc = sqlite3_exec(db, userSql.c_str(), callback, ptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        string userName = result;  // Store the user's first_name

        // Retrieve Pokemon cards for the current user
        result = ""; // Clear the result for new data
        string cardSql = "SELECT P.ID, P.Card_name, P.Card_type, P.rarity, P.quantity, P.Card_price, U.first_name "
                 "FROM Pokemon_cards P "
                 "JOIN Users U ON P.owner_id = U.ID "
                 "WHERE P.owner_id = " + id;
        rc = sqlite3_exec(db, cardSql.c_str(), callback, ptr, &zErrMsg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }

        if (result.empty()) {
            sendStr += "No records in the Pokémon cards table for current user, " + userName + ".";
        } else {
            sendStr += "The list of records in the Pokémon cards table for current user, " + userName + ":\nID Card Name Type Rarity Quantity Price OwnerID\n" + result + "\n";
        }
    }

    send(clientID, sendStr.c_str(), sizeof(Buff), 0);
}

            else if (command == "BALANCE") {
// cout << "Received: BALANCE" << endl;
                string sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE Users.ID=" + id + "), 'PRESENT', 'NOT_PRESENT') result;";
// SQL Execute
                rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                if (rc != SQLITE_OK) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                }
                else if (result == "PRESENT") {
//Balance for user with that ID
                    sql = "SELECT usd_balance FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    string usd_balance = result;
//Client Name
                    sql = "SELECT first_name FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    string user_name = result;
                    sql = "SELECT last_name FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    user_name += " " + result;
                    string tempStr = "200 OK\n   Balance for user " + user_name + ": $" + usd_balance;
                    send(clientID, tempStr.c_str(), sizeof(Buff), 0);
                }
                else {
                    cout << "SERVER: User does not exist. Aborting Balance.\n";
                    send(clientID, "User does not exist.", sizeof(Buff), 0);
                }
            }
            else if (command == "QUIT") {
                send(clientID, "200 OK", 27, 0);
                for (int i = 0; i < list.size(); i++) {
                    if (list.at(i).user == u)
                        list.erase(list.begin() + i);
                }
                nClient[clientIndex] = 0;
                close(clientID);
                pthread_exit(userData);
                return userData;
            }

            else if (command == "SHUTDOWN") {
    if (rootUsr) {
        send(clientID, "200 OK", 7, 0);
        sqlite3_close(db);
        cout << "Closed Database" << endl;
        close(clientID);
        cout << "Client Terminated Connection:  " << clientID << endl;
        for (int i = 0; i < list.size(); i++) {
            if (list.at(i).user == u) {
                list.erase(list.begin() + i);
                i--; // Decrement to recheck the same index
            }
        }
        for (int i = 0; i < list.size(); i++) {
            close(nClient[list.at(i).socket]);
            pthread_cancel((list.at(i)).myThread);
        }
        close(nSocket);
        cout << "Socket Closed: " << nSocket << endl;
        pthread_exit(userData);
        exit(EXIT_SUCCESS);
    } else {
        // For non-root users, send a 401 status code and an error message.
        send(clientID, "401 Unauthorized: Only root user can execute a SHUTDOWN. ", 57, 0);
    }
}


            else if (command == "LOGOUT") {
                send(clientID, "200 OK", 7, 0);
                for (int i = 0; i < list.size(); i++) {
                    if (list.at(i).user == u)
                        list.erase(list.begin() + i);
                }
                nClient[clientIndex] = clientID;
                pthread_exit(userData);
                return userData;
            }
            else if (command == "DEPOSIT") {
                string sql = "SELECT IIF(EXISTS(SELECT 1 FROM Users WHERE Users.ID=" + id + "), 'PRESENT', 'NOT_PRESENT') result;";
// SQL Execute
                rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                if (rc != SQLITE_OK) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                }
                else if (result == "PRESENT") {
// Output Balance For This User
                    string deposit = "";
                    for (int i = (command.length() + 1); i < strlen(Buff); i++) {
                        if (Buff[i] == '\n')
                            break;
                        deposit += Buff[i];
                    }
                    sql = "UPDATE Users SET usd_balance= usd_balance +" + deposit + " WHERE Users.ID='" + id + "';";
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    sql = "SELECT usd_balance FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    string usd_balance = result;
// Get User Name
                    sql = "SELECT first_name FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    string user_name = result;
                    sql = "SELECT last_name FROM Users WHERE Users.ID=" + id;
                    rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                    user_name += " " + result;
                    string tempStr = "200 OK\n   Deposit successfully. New User balance for " + user_name + ": $" + usd_balance;
                    send(clientID, tempStr.c_str(), sizeof(Buff), 0);
                }
                else {
                    cout << "SERVER> User does not exist. Aborting Balance.\n";
                    send(clientID, "User does not exist.", sizeof(Buff), 0);
                }
            }
            else if (command == "WHO" && rootUsr) {
                string result = "200 OK\nThe list of the active Users:\n";
                for (int i = 0; i < list.size(); i++) {
                    result += (list.at(i).user + " " + list.at(i).ip + "\n");
                }
                send(clientID, result.c_str(), sizeof(Buff), 0);
            }
//CHECK to determine if client is in database when have multiple clients
            else if (command == "CHECK") {
                string searchTerm = "";
                string sendStr;
                result = "";
                for (int i = (command.length() + 1); i < strlen(Buff); i++) {
                    if (Buff[i] == '\n')
                        break;
                    searchTerm += Buff[i];
                }
                string sql = "SELECT COUNT(POKEMON_CARD_name) FROM (SELECT * FROM Pokemon_cards WHERE owner_id = " + id + ") WHERE POKEMON_CARD_name LIKE '%" + searchTerm + "%';";
                rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                string count = result;
                result = "";
                sql = "SELECT POKEMON_CARD_name, POKEMON_CARD_price FROM (SELECT * FROM Pokemon_cards WHERE owner_id = " + id + ") WHERE POKEMON_CARD_name LIKE '%" + searchTerm + "%'";
                rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);
                if (result == "") {
                    sendStr = "404 Your search did not match any records";
                }
                else {
                    sendStr = "200 OK\n   Found:" + count + "\nPOKEMON_CARD_name POKEMON_CARD_Amount\n   " + result;
                }
                send(clientID, sendStr.c_str(), sizeof(Buff), 0);

            }
            else if (command == "LOOKUP") {
                string searchTerm = ""; // Initialize the search term
                string sendStr;
                result = "";

                // Extract the search term from the client's command
                size_t commandLen = command.length();
                size_t i = commandLen + 1;

                while (i < strlen(Buff) && Buff[i] != '\n') {
                    searchTerm += Buff[i];
                    i++;
                }

                // Split the search term into individual words
                vector<string> searchTerms;
                size_t start = 0;
                size_t end;
                while ((end = searchTerm.find(' ', start)) != string::npos) {
                    searchTerms.push_back(searchTerm.substr(start, end - start));
                    start = end + 1;
                }
                searchTerms.push_back(searchTerm.substr(start));

                // Construct the SQL query to look up Pokémon cards based on card_name, card_type, or rarity
                string sql = "SELECT * FROM Pokemon_cards WHERE owner_id = " + id + " AND (";
                for (size_t j = 0; j < searchTerms.size(); j++) {
                    if (j > 0) {
                        sql += " OR ";
                    }
                    sql += "(card_name = '" + searchTerms[j] + "' OR card_type = '" + searchTerms[j] + "' OR rarity = '" + searchTerms[j] + "')";
                }
                sql += ");";

                rc = sqlite3_exec(db, sql.c_str(), callback, ptr, &zErrMsg);

                if (result == "") {
                    sendStr = "404 Your search did not match any records";
                } else {
                    // Format the result into a response string
                    sendStr = "200 OK\nFound " + to_string(getMatchedRecordsCount(result)) + " match" + (getMatchedRecordsCount(result) > 1 ? "es" : "") + "\n" + result;
                }

                send(clientID, sendStr.c_str(), sizeof(Buff), 0);
        }

// All Else Fails, Invalid Command
            else {
                cout << "SERVER: INVALID COMMAND" << endl;
                send(clientID, "400 Invalid Command", 20, 0);
            }
        }

        for (int i = 0; i < list.size(); i++) {
            if (list.at(i).user == u)
                list.erase(list.begin() + i);
        }

        cout << endl << "Error at client socket\n";
        nClient[clientIndex] = 0;
        close(clientID);
        pthread_exit(userData);

        return userData;
    }
}

bool getData(char line[], string info[], string command) {
    int l = command.length();
    int spaceLocation = l + 1;
//changed from 3
    for (int i = 0; i < 8; i++) {
        info[i] = "";

// Parses the information
        for (int j = spaceLocation; j < strlen(line); j++) {

            if (line[j] == 0)
                return false;
            if (line[j] == ' ')
                break;
            if (line[j] == '\n')
                break;
            info[i] += line[j];

//Valid Client Data input for commands
            if (i > 0) {
                if (((int)line[j] > 57 || (int)line[j] < 46) && (int)line[j] != 47)
                    return false;
            }
        }
        //changed from 3
        if (info[i] == "") {
            fill_n(info, 8, 0);
            return false;
        }

        spaceLocation += info[i].length() + 1;

    }

    return true;

}

//Function to deal with message format to screen to client
static int callback(void* ptr, int count, char** data, char** azColName) {

    if (count == 1) {
        result = data[0];
    }
    else if (count > 1) {
        for (int i = 0; i < count; i++) {

            if (result == "") {
                result = data[i];
            }
            else {
                result = result + " " + data[i];
            }
            if (i == 6)
            {
                result += "\n  ";
            }

        }
    }
    return 0;
}

void NewClientConnection(){
//Descriptor file per CIS 427 Lecture
    int nNewClient = accept(nSocket, (struct sockaddr*)&srv, &addr_len);

    //Issue with Connection since client tried to connect without IP
    if (nNewClient < 0) {
        perror("Error during accepting connection");
    }
    else {

        void* temp = &nNewClient;
        cout << nNewClient << endl;

        int nIndex;
        for (nIndex = 0; nIndex < 5; nIndex++)
        {
            if (nClient[nIndex] == 0)
            {
                nClient[nIndex] = nNewClient;
                if (nNewClient > nMaxFd)
                {
                    nMaxFd = nNewClient + 1;
                }
                break;
            }
        }

        if (nIndex == 5)
        {
            cout << endl << "Server busy. Cannot accept anymore connections";
        }

        printf("New connection, socket fd is %d, ip is: %s, port: %d\n", nNewClient, inet_ntoa(srv.sin_addr), ntohs(srv.sin_port));
        send(nClient[nIndex], "You have successfully connected to the server!", 47, 0);
    }

}

void DataFromClient()
{
    string command;
    temp = &u;

    for (int nIndex = 0; nIndex < 5; nIndex++)
    {
        if (nClient[nIndex] > 0)
        {
            if (FD_ISSET(nClient[nIndex], &fr))
            {
// Read the data from client
                char sBuff[10000] = { 0, };
                int nRet = recv(nClient[nIndex], sBuff, 10000, 0);
                if (nRet < 0)
                {
// This happens when client closes connection abruptly
                    cout << endl << "Error at client socket";
                    for (int i = 0; i < list.size(); i++) {
                        if (list.at(i).user == u.user)
                            list.erase(list.begin() + i);
                    }
                    close(nClient[nIndex]);
                    nClient[nIndex] = 0;
                }
                else
                {

                    command = buildCommand(sBuff);
                    cout << command << endl;

                    if (command == "LOGIN") {
                        string info = getData(sBuff, command);
                        loggedUser tempStruct;
                        u.user = info;
                        int passLength = command.length() + info.length();
                        string passInfo = clientPassword(sBuff, passLength);

                        u.password = passInfo;
                        u.socket = nIndex;
                        tempStruct.socket = nIndex;
                        struct sockaddr_in client_addr;
                        socklen_t addrlen;

                        cout << "Assigned user info. Username: " << info << " Socket Index: " << u.socket << endl;

                        string commandSql = "SELECT IIF(EXISTS(SELECT * FROM Users WHERE user_name = '" + info + "' AND password = '" + passInfo + "') , 'USER_PRESENT', 'USER_NOT_PRESENT') result;";
                        sql = commandSql.c_str();
                        sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                        if (result == "USER_PRESENT") {
                            cout << "Logging in... " << endl;
                            send(nClient[nIndex], "200 OK", 7, 0);
                            getpeername(nClient[nIndex], (struct sockaddr*)&client_addr, &addrlen);
                            tempStruct.ip = "";
                            cout << "IP address: " << inet_ntoa(client_addr.sin_addr) << endl;
                            tempStruct.ip = inet_ntoa(client_addr.sin_addr);
                            cout << tempStruct.ip << endl;
                            tempStruct.user = u.user;

                            list.push_back(tempStruct);

                            commandSql = "SELECT ID FROM Users WHERE user_name = '" + info + "' AND password = '" + passInfo + "'";
                            sql = commandSql.c_str();
                            sqlite3_exec(db, sql, callback, 0, &zErrMsg);
                            u.id = stoi(result);

                            pthread_create(&(list.at(list.size() - 1).myThread), NULL, databaseCommands, temp);

}
                        else {
                            cout << "403: Username or Password Invalid!" << endl;
                            //should we close the client if they enter invalid credentials?
                           close(nClient[nIndex]);
                           nClient[nIndex] = 0;
}
                    }
                    else if (command == "QUIT") {
                        cout << "Quit command!" << endl;
                        send(nClient[nIndex], "200 OK", 27, 0);
                        close(nClient[nIndex]);
                        nClient[nIndex] = 0;
                    }
                    else if (command == "BUY" || command == "SELL" || command == "CHECK" || command == "LOOKUP" || command == "DEPOSIT" || command == "SHUTDOWN" || command == "LIST" || command == "WHO" || command == "BALANCE" || command == "LOGOUT" || command == "NULL") {
                        send(nClient[nIndex], "Guest Users can only use the login and quit commands.", 54, 0);
                    }
                    else {
                        cout << endl << "Received data from:" << nClient[nIndex] << "[Message: " << sBuff << " size of array: " << strlen(sBuff) << "] Error 400" << endl;
                        send(nClient[nIndex], "Command does not exist.", 24, 0);
                    }
                    break;
                }
            }
        }
    }
}
