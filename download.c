#include "download.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Input format: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        return -1;
    }

    struct FTP_URL url;
    bzero(&url, sizeof(url));

    if (parse_ftp_url(argv[1], &url) == -1) return -1;
    extract_file_name(url.resource, url.file);
    if (resolve_host_ip(url.host, url.ip) == -1) return -1;

    printf("Password: %s\n", url.password);
    printf("User: %s\n", url.user);
    printf("Host: %s\n", url.host);
    printf("Resource: %s\n", url.resource);
    printf("File: %s\n", url.file);
    printf("IP Address: %s\n", url.ip);

    char response[LEN];

    int sockfd = createTCPClientSocket(url.ip, FTP_PORT);
    if (sockfd == -1) return -1;

    if (read_response(sockfd, response) != CODE_READY_FOR_NEW_USER) {
        fprintf(stderr, "FTP server not ready.\n");
        close(sockfd);
        return -1;
    } else {
        printf("FTP server ready.\n");
    }

    if (login_to_ftp(sockfd, &url) == -1) {
        close(sockfd);
        return -1;
    }

    int newport;
    if (enter_passive_mode(sockfd, &newport) != CODE_PASSIVE_MODE) {
        fprintf(stderr, "Error entering passive mode.\n");
        close(sockfd);
        return -1;
    } else {
        printf("Passive mode OK\n");
    }

    int newsockfd = createTCPClientSocket(url.ip, newport);
    if (newsockfd == -1) {
        close(sockfd);
        return -1;
    }

    int res = send_retr(sockfd, url.resource);
    if (res != CODE_FILE_STATUS_OK && res != CODE_DATA_CONNECTION_OPEN) {
        fprintf(stderr, "Error retrieving file.\n");
        close(newsockfd);
        close(sockfd);
        return -1;
    } else {
        printf("File retrieved.\n");
    }

    printf("Download started\n");
    if (download_file(newsockfd, url.file) == -1) {
        close(newsockfd);
        close(sockfd);
        return -1;
    }
    printf("Download ended\n");
    close(newsockfd);

    res = read_response(sockfd, response);
    if (res != CODE_TRANSFER_COMPLETE) {
        fprintf(stderr, "Transfer not complete :(\n");
        close(sockfd);
        return -1;
    } else {
        printf("Transfer complete :)\n");
    }

    quit_ftp(sockfd);
    close(sockfd);

    return 0;
}

int parse_ftp_url(char *url_str, struct FTP_URL *url) {
    if (sscanf(url_str, FTP_URL_WITH_CREDENTIALS, url->user, url->password, url->host, url->resource) == 4) {
        return 0;
    } else if (sscanf(url_str, FTP_URL_WITHOUT_CREDENTIALS, url->host, url->resource) == 2) {
        strcpy(url->user, "anonymous");
        strcpy(url->password, "");
        return 0;
    } else {
        fprintf(stderr, "Invalid FTP URL format.\n");
        return -1;
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

int resolve_host_ip(char *host, char *ip) {
    struct hostent *h;
    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        return -1;
    }
    strncpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)), LEN - 1);
    ip[LEN - 1] = '\0';
    return 0;
}

int login_to_ftp(int sockfd, struct FTP_URL *url) {
    char response[LEN];

    char user[LEN];
    snprintf(user, LEN, "USER %.1007s\r\n", url->user);
    if (write(sockfd, user, strlen(user)) < 0) {
        perror("write() failed");
        return -1;
    }
    if (read_response(sockfd, response) != CODE_USER_OK) {
        fprintf(stderr, "Invalid user.\n");
        return -1;
    } else {
        printf("User OK\n");
    }

    char pass[LEN];
    snprintf(pass, LEN, "PASS %.1007s\r\n", url->password);
    if (write(sockfd, pass, strlen(pass)) < 0) {
        perror("write() failed");
        return -1;
    }
    if (read_response(sockfd, response) != CODE_USER_LOGGED_IN) {
        fprintf(stderr, "Invalid password.\n");
        return -1;
    } else {
        printf("Password OK\n");
    }
    return 0;
}
void quit_ftp(int sockfd) {
    char response[LEN];
    if (write(sockfd, "QUIT\r\n", 6) < 0) {
        perror("write() failed");
        return;
    }
    if (read_response(sockfd, response) != CODE_QUIT) {
        fprintf(stderr, "Could not close :(\n");
    }
}

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
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

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

    if (write(sockfd, pasv, strlen(pasv)) < 0) {
        perror("write() failed");
        return -1;
    }

    char response[LEN];
    int responseCode = read_response(sockfd, response);
    printf("Response Code: %d\n", responseCode);

    if (responseCode != CODE_PASSIVE_MODE) {
        fprintf(stderr, "Error entering passive mode.\n");
        return -1;
    }

    char *start = strchr(response, '(');
    if (!start) {
        fprintf(stderr, "Error: No '(' found in response.\n");
        return -1;
    }

    char *end = strchr(response, ')');
    if (!end) {
        fprintf(stderr, "Error: No ')' found in response.\n");
        return -1;
    }

    int p1, p2;
    if (sscanf(start, PASSIVE_MODE_RESPONSE, &p1, &p2) != 2) {
        fprintf(stderr, "Error: Failed to parse the IP and port from the response.\n");
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

    if (write(sockfd, retr, strlen(retr)) < 0) {
        perror("write() failed");
        return -1;
    }

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

    if (bytesRead < 0) {
        perror("read() failed");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}
