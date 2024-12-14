#include "download.h"


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Input format: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct FTP_URL url;
    bzero(&url, sizeof(url));

    parse_ftp_url(argv[1], &url);
    extract_file_name(url.resource, url.file);
    resolve_host_ip(url.host, url.ip);

    printf("Password: %s\n", url.password);
    printf("User: %s\n", url.user);
    printf("Host: %s\n", url.host);
    printf("Resource: %s\n", url.resource);
    printf("File: %s\n", url.file);
    printf("IP Address: %s\n", url.ip);

    char response[LEN];

    int sockfd = createTCPClientSocket(url.ip, FTP_PORT);
    if (read_response(sockfd, response) != CODE_READY_FOR_NEW_USER) {
        printf("FTP server not ready.\n");
        return -1;
    } else {
        printf("FTP server ready.\n");
    }

    login_to_ftp(sockfd, &url);

    int newport;
    if (enter_passive_mode(sockfd, &newport) != CODE_PASSIVE_MODE) {
        printf("Error entering passive mode.\n");
        return -1;
    } else {
        printf("Passive mode OK\n");
    }

    int newsockfd = createTCPClientSocket(url.ip, newport);
    int res = send_retr(sockfd, url.resource);
    if (res != CODE_FILE_STATUS_OK && res != CODE_DATA_CONNECTION_OPEN) {
        printf("Error retrieving file.\n");
        return -1;
    } else {
        printf("File retrieved.\n");
    }

    printf("Download started\n");
    download_file(newsockfd, url.file);
    printf("Download ended\n");
    close(newsockfd);

    res = read_response(sockfd, response);
    if (res != CODE_TRANSFER_COMPLETE) {
        printf("Transfer not complete :(\n");
        return -1;
    } else {
        printf("Transfer complete :)\n");
    }

    quit_ftp(sockfd);
    close(sockfd);

    return 0;
}

void parse_ftp_url(char *url_str, struct FTP_URL *url) {
    if (sscanf(url_str, FTP_URL_WITH_CREDENTIALS, url->user, url->password, url->host, url->resource) == 4) {
    } else if (sscanf(url_str, FTP_URL_WITHOUT_CREDENTIALS, url->host, url->resource) == 2) {
        strcpy(url->user, "anonymous");
        strcpy(url->password, "");
    } else {
        fprintf(stderr, "Invalid FTP URL format.\n");
        exit(EXIT_FAILURE);
    }
}

void extract_file_name(char *resource, char *file) {
    char *last_slash = strrchr(resource, '/');
    if (last_slash != NULL) {
        strcpy(file, last_slash + 1);
    } else {
        strcpy(file, resource);
    }
}

void resolve_host_ip(char *host, char *ip) {
    struct hostent *h;
    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    strncpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)), LEN - 1);
    ip[LEN - 1] = '\0';
}

void login_to_ftp(int sockfd, struct FTP_URL *url) {
    char response[LEN];

    char user[LEN];
    sprintf(user, "USER %s\r\n", url->user);
    write(sockfd, user, strlen(user));
    if (read_response(sockfd, response) != CODE_USER_OK) {
        printf("Invalid user.\n");
        exit(EXIT_FAILURE);
    } else {
        printf("User OK\n");
    }

    char pass[LEN];
    sprintf(pass, "PASS %s\r\n", url->password);
    write(sockfd, pass, strlen(pass));
    if (read_response(sockfd, response) != CODE_USER_LOGGED_IN) {
        printf("Invalid password.\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Password OK\n");
    }
}

void quit_ftp(int sockfd) {
    char response[LEN];
    write(sockfd, "QUIT\r\n", 6);
    if (read_response(sockfd, response) != CODE_QUIT) {
        printf("Could not close :(\n");
    }
}

// Function to create a TCP client socket
int createTCPClientSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("Connecting to %s:%d\n", ip, port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() failed");
        exit(-1);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sockfd);
        exit(-1);
    }

    return sockfd;
}

// Function to read a response from the FTP server
int read_response(int sockfd, char *response) {
    printf("Reading response...\n");
    memset(response, 0, LEN);
    sleep(SLEEP_DURATION);
    ssize_t bytesRead = read(sockfd, response, LEN - 1);
    if (bytesRead < 0) {
        perror("Error reading from socket");
        return -1;
    } else if (bytesRead == 0) {
        fprintf(stderr, "Connection closed by peer\n");
        return -1;
    }

    response[bytesRead] = '\0';
    printf("FTP Response: %s\n", response);
    int ftpStatusCode;
    if (sscanf(response, "%3d", &ftpStatusCode) != 1) {
        fprintf(stderr, "Error: Unable to extract the status code.\n");
        return -1;
    }
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

    if (responseCode != CODE_PASSIVE_MODE) {
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
    if (sscanf(start, PASSIVE_MODE_RESPONSE, &p1, &p2) != 2) {
        printf("Error: Failed to parse the IP and port from the response.\n");
        return -1;
    }



    *port = NEW_PORT(p1, p2);
    printf("Passive mode port: %d\n", *port);

    return responseCode;
}

int send_retr(int sockfd, char *file) {
    char retr[LEN];
    sprintf(retr, "RETR %s\r\n", file);
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
