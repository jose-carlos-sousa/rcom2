#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h> 
#define LEN 1024
#define CODE_FILE_STATUS_OK 150
#define CODE_DATA_CONNECTION_OPEN 125
#define CODE_PASSIVE_MODE 227
#define CODE_USER_LOGGED_IN 230
#define CODE_USER_OK 331
#define CODE_QUIT 221
#define CODE_TRANSFER_COMPLETE 226
#define CODE_READY_FOR_NEW_USER 220
#define FTP_PORT 21
#define SLEEP_DURATION 1
#define FTP_URL_WITH_CREDENTIALS "ftp://%[^:]:%[^@]@%[^/]/%s"
#define FTP_URL_WITHOUT_CREDENTIALS "ftp://%[^/]/%s"
#define PASSIVE_MODE_RESPONSE "(%*d,%*d,%*d,%*d,%d,%d)"
#define NEW_PORT(p1, p2) ((p1) * 256 + (p2))


int createTCPClientSocket(char *ip, int port);
int read_response(int sockfd, char *response);
int enter_passive_mode(int sockfd, int *port);
int send_retr(int sockfd, char *file);
int download_file(int sockfd, char *file);
void parse_ftp_url(char *url_str, struct FTP_URL *url);
void extract_file_name(char *resource, char *file);
void resolve_host_ip(char *host, char *ip);
void login_to_ftp(int sockfd, struct FTP_URL *url);
void quit_ftp(int sockfd);

struct FTP_URL {
    char host[LEN];       
    char resource[LEN];
    char file[LEN];      
    char user[LEN];     
    char password[LEN];   
    char ip[LEN];       
};