#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h> 
#define LEN 3000

struct FTP_URL {
    char host[LEN];       // Hostname
    char resource[LEN];   // Resource path
    char file[LEN];       // File name
    char user[LEN];       // Username
    char password[LEN];   // Password
    char ip[LEN];         // Resolved IP address
};



int createTCPClientSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Clear structure
    bzero((char *) &server_addr, sizeof(server_addr));

    // Set up address family, IP, and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);  // Ensure port is in network byte order

    // Convert IP address
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() failed");
        exit(-1);
    }

    // Create socket
    printf("Connecting to %s:%d\n", ip, port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() failed");
        exit(-1);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sockfd); // Close the socket before exiting
        exit(-1);
    }

    return sockfd;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEN 1024
#define STATUS_CODE_REGEX "^[0-9]{3}"

int read_response(int sockfd, char *response) {
    printf("Reading response...\n");

    // Initialize response buffer to zero
    memset(response, 0, LEN);

    // Read up to LEN-1 bytes from the socket into the response buffer
    sleep(1);
    ssize_t bytesRead = read(sockfd, response, LEN - 1);

    if (bytesRead < 0) {
        // Handle read error
        perror("Error reading from socket");
        return -1;
    } else if (bytesRead == 0) {
        // Handle end-of-file (connection closed)
        fprintf(stderr, "Connection closed by peer\n");
        return -1;
    }

    // Null-terminate the string just in case read() didn't fill up the buffer
    response[bytesRead] = '\0';

    // Print the full response for debugging
    printf("FTP Response: %s\n", response);

    // Declare the status code variable
    int ftpStatusCode;

    // Extract the FTP status code (e.g., 220, 230, etc.)
    if (sscanf(response, "%3d", &ftpStatusCode) != 1) {
        // Handle error extracting the status code
        fprintf(stderr, "Error: Unable to extract the status code.\n");
        return -1;
    }


    // Print the extracted status code for debugging
    printf("FTP Status Code: %d\n", ftpStatusCode);

    return ftpStatusCode;
}


int enter_passive_mode(int sockfd, int *port) {
    char pasv[LEN];
    sprintf(pasv, "PASV\r\n");
    printf("PASV command: %s\n", pasv);
    
    write(sockfd, pasv, strlen(pasv));
    
    char response[LEN];
    int responseCode = read_response(sockfd, response);
    printf("Response Code: %d\n", responseCode);

    if (responseCode != 227) {
        printf("Error entering passive mode.\n");
        return -1;
    }

    char *start = strchr(response, '(');
    if (!start) {
        printf("Error: No '(' found in response.\n");
        return -1;
    }
    
    char *end = strchr(response, ')');
    if (!end) {
        printf("Error: No ')' found in response.\n");
        return -1;
    }
    
    int p1, p2;
    if (sscanf(start, "(%*d,%*d,%*d,%*d,%d,%d)", &p1, &p2) != 2) {
        printf("Error: Failed to parse the IP and port from the response.\n");
        return -1;
    }

    *port = p1 * 256 + p2;
    printf("Passive mode port: %d\n", *port);
    
    return responseCode;
}

int send_retr(int sockfd, char *file) {
    char retr[LEN];
    sprintf(retr, "retr %s\r\n", file);
    printf("RETR command: %s\n", retr);
    
    write(sockfd, retr, strlen(retr));
    
    char response[LEN];
    int responseCode = read_response(sockfd, response);
    printf("Response Code: %d\n", responseCode);

    return responseCode;
}

int download_file(int sockfd, char *file) {
    FILE *fp = fopen(file, "w");
    if (fp == NULL) {
        perror("fopen()");
        return -1;
    }

    char buffer[LEN];
    ssize_t bytesRead;
    while ((bytesRead = read(sockfd, buffer, 1)) > 0) {
        fwrite(buffer, 1, bytesRead, fp);
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct FTP_URL url = {0};

    if (sscanf(argv[1], "ftp://%[^:]:%[^@]@%[^/]/%s", url.user, url.password, url.host, url.resource) == 4) {
    } else if (sscanf(argv[1], "ftp://%[^/]/%s", url.host, url.resource) == 2) {
        strcpy(url.user, "anonymous");
        strcpy(url.password, "");
    } else {
        fprintf(stderr, "Invalid FTP URL format.\n");
        exit(EXIT_FAILURE);
    }

    char *last_slash = strrchr(url.resource, '/');
    if (last_slash != NULL) {
        strcpy(url.file, last_slash + 1);
    } else {
        strcpy(url.file, url.resource);
    }

    struct hostent *h;
    if ((h = gethostbyname(url.host)) == NULL) {
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    strncpy(url.ip, inet_ntoa(*((struct in_addr *)h->h_addr)), LEN - 1);
    url.ip[LEN - 1] = '\0';

    printf("Password: %s\n", url.password);
    printf("User: %s\n", url.user);
    printf("Host: %s\n", url.host);
    printf("Resource: %s\n", url.resource);
    printf("File: %s\n", url.file);
    printf("IP Address: %s\n", url.ip);
    char response[LEN];

    int sockfd = createTCPClientSocket(url.ip, 21);
    if (read_response(sockfd, response) != 220) {
        printf("FTP server not ready.\n");
        return -1;
    } else {
        printf("FTP server ready.\n");
    }

    printf("User: %s\n", url.user);
    char user[LEN];
    sprintf(user, "USER %s\r\n", url.user);
    write(sockfd, user, strlen(user));
    if (read_response(sockfd, response) != 331) {
        printf("Invalid user.\n");
        return -1; 
    } else {
        printf("User OK\n");
    }
    char pass[LEN];
    sprintf(pass, "PASS %s\r\n", url.password);
    write(sockfd, pass, strlen(pass));
    if (read_response(sockfd, response) != 230) {
        printf("Invalid password.\n");
        return -1;
    } else {
        printf("Password OK\n");
    }

    int newport;
    if (enter_passive_mode(sockfd, &newport) != 227) {
        printf("Error entering passive mode.\n");
        return -1;
    } else {
        printf("Passive mode OK\n");
    }

    int newsockfd = createTCPClientSocket(url.ip, newport);
    int res= send_retr(sockfd, url.resource);
    if (res!= 150 && res != 125) {
        printf("Error retrieving file.\n");
        return -1;
    } else {
        printf("File retrieved.\n");
    }
    printf("download started\n");
    download_file(newsockfd, url.file);
    printf("download ended\n");
    res =read_response(sockfd,response) ;
    if(res!=226){
        printf("transfer not complete :(");
        return -1;
    }else{
        printf("transfer complete :)");
    }
    write(sockfd, "quit\n",5);
    if(read_response(sockfd,response) != 221){
        printf("could not close :(\n");
    }
    
    close(newsockfd);
    close(sockfd);


    return 0;
}
