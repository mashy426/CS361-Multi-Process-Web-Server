/* CS361 Homework 5: A Home-Brew Multi-Process Web Server
 * Name:   Shyam Patel
 * NetID:  spate54
 * Date:   Nov 14, 2018
 */

#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BACKLOG (10)  // # of pending connections queue will hold


// 404 error page HTML source
char *error_page =
    "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
    "<meta charset=\"utf-8\">\n<title>Page Not Found</title>\n"
    "<link href=\"https://fonts.googleapis.com/css?family="
    "Montserrat:400,700,900\" rel=\"stylesheet\">\n<style>\n"
    "* { -webkit-box-sizing: border-box; box-sizing: border-box; }\n"
    "body { padding: 0; margin: 0; }\n"
    "#nf { position: relative; height: 100vh; }\n"
    "#nf .nf { position: absolute; left: 50%; top: 50%; "
        "-webkit-transform: translate(-50%, -50%); "
        "transform: translate(-50%, -50%); }\n"
    ".nf { max-width: 410px; width: 100%; text-align: center; }\n"
    ".nf .nf-404 { height: 280px; position: relative; z-index: -1; }\n"
    ".nf .nf-404 h1 { font-family: 'Montserrat', sans-serif; font-size: 230px; "
        "margin: 0px; font-weight: 900; position: absolute; left: 50%; "
        "-webkit-transform: translateX(-50%); transform: translateX(-50%); "
        "background: -webkit-linear-gradient(135deg, #21219e, #8045a8); "
        "background: -linear-gradient(135deg, #21219e, #8045a8); "
        "-webkit-background-clip: text; -webkit-text-fill-color: transparent; "
        "background-size: cover; background-position: center; }\n"
    ".nf h2 { font-family: 'Montserrat', sans-serif; color: #000; "
        "font-size: 24px; font-weight: 700; "
        "text-transform: uppercase; margin-top: 0; }\n"
    ".nf p { font-family: 'Montserrat', sans-serif; color: #000; "
        "font-size: 14px; font-weight: 400; "
        "margin-bottom: 20px; margin-top: 0px; }\n"
    ".nf a { font-family: 'Montserrat', sans-serif; font-size: 14px; "
        "text-decoration: none; text-transform: uppercase; "
        "background: #0046d5; display: inline-block; padding: 15px 30px; "
        "border-radius: 40px; color: #fff; font-weight: 700; "
        "-webkit-box-shadow: 0px 4px 15px -5px #0046d5; "
        "box-shadow: 0px 4px 15px -5px #0046d5; }\n"
    "@media only screen and (max-width: 767px) { "
        ".nf .nf-404 { height: 142px; } "
        ".nf .nf-404 h1 { font-size: 112px; } }\n"
    "</style>\n</head>\n<body>\n<div id=\"nf\">\n"
    "<div class=\"nf\">\n<div class=\"nf-404\">\n"
    "<h1>Oops!</h1>\n</div>\n<h2>404 - Page not found</h2>\n"
    "<p>The page you are looking for might have been removed, "
    "had its name changed or is temporarily unavailable.</p>\n"
    "<a href=\"../\">Go Back</a> &nbsp; "
    "<a href=\"mailto:spate54@uic.edu\">Contact Admin</a>\n"
    "</div>\n</div>\n</body>\n</html>";


// directory listing page HTML source
char *directory_page =
    "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
    "<meta charset=\"utf-8\">\n<title>Directory Listing</title>\n"
    "<link href=\"https://fonts.googleapis.com/css?family="
    "Montserrat:400,700,900\" rel=\"stylesheet\">\n<style>\n"
    "* { -webkit-box-sizing: border-box; box-sizing: border-box; }\n"
    "body { padding: 0; margin: 0; }\n"
    "#dl { position: relative; height: 100vh; }\n"
    "#dl .dl { position: absolute; left: 50%; top: 50%; "
        "-webkit-transform: translate(-50%, -50%); "
        "transform: translate(-50%, -50%); }\n"
    ".dl { width: 100%; text-align: center; }\n"
    ".dl .dl1 { height: 80px; position: relative; z-index: -1; }\n"
    ".dl .dl1 h1 { font-family: 'Montserrat', sans-serif; font-size: 60px; "
        "margin: 0px; font-weight: 900; position: absolute; left: 50%; "
        "background: -webkit-linear-gradient(135deg, #21219e, #8045a8); "
        "background: -linear-gradient(135deg, #21219e, #8045a8); "
        "-webkit-background-clip: text; -webkit-text-fill-color: transparent; "
        "-webkit-transform: translateX(-50%); transform: translateX(-50%); }\n"
    ".dl h2 { font-family: 'Montserrat', sans-serif; color: #000; "
        "font-size: 18px; font-weight: 700; "
        "text-transform: uppercase; margin-top: 0; }\n"
    ".dl a { font-family: 'Montserrat', sans-serif; font-size: 14px; "
        "text-decoration: none; text-transform: uppercase; "
        "background: #0046d5; display: inline-block; padding: 6px 18px; "
        "border-radius: 40px; color: #fff; font-weight: 700; "
        "-webkit-box-shadow: 0px 4px 15px -5px #0046d5; "
        "box-shadow: 0px 4px 15px -5px #0046d5; }\n"
    "</style>\n</head>\n<body>\n<div id=\"dl\">\n"
    "<div class=\"dl\">\n<div class=\"dl1\">\n"
    "<h1>Directory Listing</h1>\n</div>\n";


// return requested resource (0 if invalid HTTP request)
char *parseRequest(char *request) {
    // assume pathnames are < 257 bytes
    char *buffer = malloc(sizeof(char) * 257);
    memset(buffer, 0, 257);

    if (fnmatch("GET * HTTP/1.*", request, 0)) return 0;

    sscanf(request, "GET %s HTTP/1.", buffer);

    return buffer;                   // return str
}//end parseRequest()


// return if pathname is regular
int isRegular(char *pathname) {
    struct stat statbuf;
    stat(pathname, &statbuf);
    return S_ISREG(statbuf.st_mode);
}//end isRegular()


// return if pathname is directory
int isDirectory(char *pathname) {
    struct stat statbuf;
    stat(pathname, &statbuf);
    return S_ISDIR(statbuf.st_mode);
}//end isDirectory()


// return MIME type of pathname extension
char *getMIME(char *pathname) {
    char *extn = strstr(pathname, ".");
    if      (strcmp(extn, ".gif" ) == 0) return "image/gif"      ;
    else if (strcmp(extn, ".jpeg") == 0 ||
             strcmp(extn, ".jpg" ) == 0) return "image/jpeg"     ;
    else if (strcmp(extn, ".png" ) == 0) return "image/png"      ;
    else if (strcmp(extn, ".pdf" ) == 0) return "application/pdf";
    else                                 return "text/html"      ;
}//end getMIME()


// return content header string with appropriate MIME type
char *request_str(char *pathname) {
    // assume str is < 257 bytes
    char *buffer = malloc(sizeof(char) * 257);

    sprintf(buffer, "HTTP/1.0 200 OK\r\n"
            "Content-type: %s; charset=UTF-8\r\n\r\n",
            getMIME(pathname));

    return buffer;                   // return str
}//end request_str()


// return 404 error string
char *error_str() {
    // assume str is < 4096 bytes
    char *buffer = malloc(sizeof(char) * 4096);

    sprintf(buffer, "HTTP/1.0 404 Not Found\r\n"
            "Content-type: text/html; charset=UTF-8\r\n\r\n%s",
            error_page);

    return buffer;                   // return str
}//end error_str()


// return directory listing string
char *directory_listing(char *dir) {
    // assume str is < 16384 bytes
    char *buffer = malloc(sizeof(char) * 16384);
    sprintf(buffer, "HTTP/1.0 200 OK\r\n"
            "Content-type: text/html; charset=UTF-8\r\n\r\n%s"
            "<h2>For %s</h2>\n", directory_page, dir);

    DIR *dirp;                       // dir pointer
    struct dirent *ds;               // dir entry str
    char   pathname[257], addr[257];

    if ((dirp = opendir(dir)) != NULL) {
        while ((ds = readdir(dirp)) != NULL) {
            sprintf(pathname, "%s%s", dir, ds->d_name);
            sprintf(addr,     "%s"  ,      ds->d_name);

            if (isRegular(pathname))
                sprintf(buffer + strlen(buffer),
                        "<p><a href=\"%s\">&#x1f4c4 %s</a></p>"  , addr, addr);

            else if (isDirectory(pathname))
                sprintf(buffer + strlen(buffer),
                        "<p><a href=\"%s/\">&#x1f4c1 %s/</a></p>", addr, addr);
        }//end loop
    }//end if...

    closedir (dirp);                 // close dir
    sprintf(buffer + strlen(buffer), "</div>\n</div>\n</body>\n</html>");

    return buffer;                   // return str
}//end directory_listing()


// serve request
void serve_request(int client_fd) {
    int  read_fd, read_b, path_offset = 0;
    char client_buf[4096], send_buf[4096], pathname[4096];
    char *requested_path;
    memset(client_buf, 0, 4096);
    memset(pathname,   0, 4096);

    for (;;) {
        path_offset += recv(client_fd, &client_buf[path_offset], 4096, 0);
 
        if (strstr(client_buf, "\r\n\r\n")) break;
    }//end loop

    requested_path = parseRequest(client_buf);

    // take requested_path, add '.' to beginning + open it
    pathname[0] = '.';
    strncpy(&pathname[1], requested_path, 4095);

    // pathname exists : successful open
    if ((read_fd = open(pathname, 0, 0)) != -1) {
        // pathname is regular
        if (isRegular(pathname)) {
            send(client_fd, request_str(requested_path),
                 strlen(request_str(requested_path)), 0);

            for (;;) {
                if ((read_b = read(read_fd, send_buf, 4096)) == 0) break;

                send(client_fd, send_buf, read_b, 0);
            }//end loop
        }//end if...

        // pathname is directory
        else if (isDirectory(pathname)) {
            // append index page to directory
            char path[257];
            sprintf(path, "%s%s", pathname, "index.html");

            // index page exists
            if ((read_fd = open(path + 2, 0, 0)) != -1) {
                send(client_fd, request_str(path + 2),
                     strlen(request_str(path + 2)), 0);

                for (;;) {
                    if ((read_b = read(read_fd, send_buf, 4096)) == 0) break;

                    send(client_fd, send_buf, read_b, 0);
                }//end loop
            }//end if...

            // index page doesn't exist : directory listing
            else send(client_fd, directory_listing(pathname),
                      strlen(directory_listing(pathname)), 0);
        }//end else if...
    }//end if...

    // pathname doesn't exist : 404 error
    else send(client_fd, error_str(), strlen(error_str()), 0);

    close(read_fd);                  // close fd
    close(client_fd);                // close fd
}//end serve_request()


// thread routine
void *thread(void *sockp) {
    int sock = *((int *)sockp);
    free(sockp);                     // deallocate sock pointer

    serve_request(sock);             // serve client request
    close(sock);                     // close client connection

    return NULL;                     // return NULL
}//end thread()


// main takes 2 args :
// (1) port # on which to bind + listen for connections
// (2) directory out of which to serve
int main(int argc, char **argv) {
    int server_sock;                 // server socket
    chdir(argv[2]);                  // change directory to 2nd command line arg
    int port = atoi(argv[1]);        // read port # from 1st command line arg

    // create server socket that will connect to clients
    if ((server_sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("Creating socket failed");
        exit(EXIT_FAILURE);          // exit failure
    }//end if...

    // set SO_REUSEADDR sock opt : immediately re-bind to same port
    int reuse_true = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                   sizeof(reuse_true)) < 0) {
        perror("Setting socket option failed");
        exit(EXIT_FAILURE);          // exit failure
    }//end if...

    // create addr str
    struct sockaddr_in6 addr;        // internet socket address data str
    addr.sin6_family = AF_INET6;
    addr.sin6_port   = htons(port);  // byte order significant
    addr.sin6_addr   = in6addr_any;  // listen to all interfaces

    // bind socket to addr + port
    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error binding to port");
        exit(EXIT_FAILURE);          // exit failure
    }//end if...

    // listen for client connections
    if (listen(server_sock, BACKLOG) < 0) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);          // exit failure
    }//end if...

    for (;;) {
        int       *sock = malloc(sizeof(int));        // client socket
        pthread_t *tid  = malloc(sizeof(pthread_t));  // thread id

        // addr str to identify remote connection
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr);

        // accept 1st waiting connection from server socket
        if ((*sock = accept(server_sock, (struct sockaddr *)&remote_addr,
                            &socklen)) < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);      // exit failure
        }//end if...

        // create new thread
        if (pthread_create(tid, NULL, thread, sock)) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);      // exit failure
        }//end if...

        free(tid);                   // deallocate thread id
    }//end loop

    close(server_sock);              // close server connection
    return EXIT_SUCCESS;             // exit success
}//end main()

