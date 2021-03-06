/* *********************** INFO *****************************
 
            "ITERATIVE FILE TRANSFER TCP SERVER"
            Exercise 2.3 Lab 2
            (server)
 
            Developer: Antonio Tavera
 
 ************ BRIEF EXPLANATION OF THE ALGORITHM ************
In this exercise I am developing a TCP sequential server that, after having established a TCP connection with a client, accepts file transfer requests and sends the requested files following its specific protocol.

First, after a check on the command line arguments, the server port number is read from command line and is converted in a network byte order through the htons() function. The socket is then created through the Socket() function (with parameters AF_INET as family, SOCK_STREAM as type and IPPROTO_TCP as protocol). The socket just created is binded to any local IP address by setting s_addr to INADDR_ANY. The Bind() function is used to do this operation. Now the server listen to connection requests from client by the Listen() function. The signal handler for any SIGPIPE signal (e.g. when clients lose connection before the end of the process) is initialized. An infinite loop is created to accept (Accept() function) and handle connection requests (serverServiceFunction() function) from different clients.

The serverServiceFunction() function, that receives as parameter the connected socket, enter an infinite loop where it reads and handles all the requests coming from client. A select structure is initialized to handle possible timeout. The readline_unbuffered() function is used to read client commands. If the number of bytes read are equal to zero the connection is closed by party on socket and the server return to the main function; if the number of bytes is negative something goes wrong, an error is printed and server returns to main function; if what is read is equal to the QUIT_CMD the connection will be closed and server returns to the main function; if what is read is equal to the GET_CMD the serverServiceFunction() checks if the file requested is a valid file (checks if it contains some invalid characters, e.g. if it a directory and not a file name, checks if it is in the current directory). If it is, it proceeds by opening the file and getting its statistics (file size and timestamp) whit the stat() function and a st stat structure. The two statistics information are converted in a network byte order and sent back to the client (an OK_MSG with attached file size and timestamp) through the sendn() function. After that, the bytes of the file, previosly opened, are sent to the client (with the sendn() function ) BUFFERLENGTH per BUFFERLENGTH bytes until the EOF is reached. Every time, the file pointer is switched through the fseek() function. Each time a function fails, there is an error or an invalid command is received, an ERR_MSG is sent to the client, the connection is closed and the serverServiceFunction() returns to the main.

The sigpipeHandler() function, the signal handler for SIGPIPE signal, print an error message.
************************************************************ */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include "./../errlib.h"
#include "./../sockwrap.h"

#define RCVBUFFERLENGTH     4098                    //receive buffer length
#define SNDBUFFERLENGTH     4097                    //send buffer length
#define MAXWAITINGTIME      60                      //waiting time for messages from client

static const char GET_CMD[]     =   "GET ";         //Get message string
static const char QUIT_CMD[]    =   "QUIT\r\n";     //Quit message string
static const char ERR_MSG[]     =   "-ERR\r\n";     //Error message string
static const char OK_MSG[]      =   "+OK\r\n";      //Ok message string

char *prog_name;
void serverServiceFunction(int socketNumber);
static void sigpipeHandler(int);



int main(int argc, char **argv){
    
    //definining variables
    int passive_socket;                 //passive socket
    int s;                              //connected socket
    uint16_t lport_n, lport_h;          //port used by the server
    socklen_t addrlen;                  //address length
    struct sockaddr_in saddr, caddr;    //server and client addresses structure
    int backlog = 2;                    //maximum length of pending request queue
    
    //assigning program name
    prog_name = argv[0];
    printf("\n");
    
    //checking arguments passed by command line
    if(argc!=2){
        printf("Command line error. Requested port number. Try again!\n");
        exit(1);
    }
    
    //reading server port number from command line
    if(sscanf(argv[1], "%" SCNu16, &lport_h)!=1){
        err_sys("Invalid port number");
    }
    lport_n = htons(lport_h);
    
    //creating the socket
    printf("Creating the socket \t\t\t\t");
    s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("-> Done. Socket number: %d\n", s);
    
    //binding the socket to any local IP address
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family        =   AF_INET;
    saddr.sin_port          =   lport_n;
    saddr.sin_addr.s_addr   =   INADDR_ANY;
    showAddr("Binding to address:", &saddr);
    Bind(s, (struct sockaddr *)&saddr, sizeof(saddr));
    printf("-> Done\n");
    
    //listening to connection requests
    printf("Listening at socket: %d with backlog: %d\t\t", s, backlog);
    Listen(s, backlog);
    printf("-> Done\n");
    printf("\n");
    passive_socket = s;
    
    //signal handler for SIGPIPE signal
    //when client close connection before the end 
    Signal(SIGPIPE, sigpipeHandler);
    
    //main server loop
    for( ; ; ){
        addrlen = sizeof(struct sockaddr_in);
        s = Accept(passive_socket, (struct sockaddr *)&caddr, &addrlen);
        showAddr("Accepted connection from", &caddr);
        printf("\n");
        //serving the client
        serverServiceFunction(s);
        printf("\n");
    }
}




void serverServiceFunction(int socketNumber){
   
    int socket = socketNumber;          //socket
    int n;                              //number of bytes received
    char buffer[RCVBUFFERLENGTH];       //receive buffer
    char sndbuffer[SNDBUFFERLENGTH];    //send buffer
    FILE *fp;                           //file pointer
    char *filename;                     //used to store the name of the file
    struct stat st;                     //stat structure
    uint32_t filesize;                  //size of the file, 32 bit unsigned integer
    uint32_t time;                      //timestamp of the last file modification
    uint32_t sentsize;                  //number of bytes already sent
    uint32_t numreadchar;               //number of character read from file
    fd_set cset;                        //set of socket
    struct timeval tval;                //timeval structure
    int m;
    
    
    for( ; ; ){
        
        //setting select structure to handle read timeout
        FD_ZERO(&cset);
        FD_SET(socket, &cset);
        tval.tv_sec = MAXWAITINGTIME;
        tval.tv_usec = 0;
        if((m = Select(FD_SETSIZE, &cset, NULL, NULL, &tval)) > 0) {
            
            //read line unbuffered from client
            n = (int)readline_unbuffered(socket, buffer, RCVBUFFERLENGTH);
            
        }else {
            //timeout. No message received from client in the maximum waiting time
            printf("Timeout. No message received from client. Closing connection\t");
            Close(socket);
            printf("-> Connection closed\n");
            return;
        }
        
        //checks on number of bytes read
        if(n == 0){
            //connection closed by party on socket, close connection and terminate
            printf("Connection closed by party on socket %d\n", socket);
            Close(socket);
            return;
            
        }else if(n < 0){
            //reading error, informs client, close connection and terminate
            printf("Reading error. Closing connection\t");
            if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                printf("\n");
                printf("Sending error message failed!\t\t\t\t");
            }
            Close(socket);
            printf("-> Connection closed\n");
            return;
            
        } else if(strncmp(buffer, QUIT_CMD, sizeof(QUIT_CMD)-1)==0){
            //check if it is QUIT command, if it is close connection and terminate
            printf("-QUIT command received:\n");
            printf("Closing connection\t\t\t\t");
            Close(socket);
            printf("-> Connection closed\n");
            return;
            
        } else if(strncmp(buffer, GET_CMD, sizeof(GET_CMD)-1)==0){
            //check if it is GET command
            
            //remove carriage return and line feed character, replace them with \0
            buffer[strlen(buffer) - 2] = '\0';
            //check if it is a valid file or a directory
            filename = strdup(buffer+(sizeof(GET_CMD)-1));
            if(filename[0] == '.' || filename[0] == '~' || (strchr(filename, '/') != NULL)){
                //invalid file, print error and stop execution
                printf("\n");
                printf("Invalid file error. Closing connection\t\t");
                if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                    printf("\n");
                    printf("Sending error message failed!\t\t\t\t");
                }
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            
            //check if the requested file is in the current directory otherwise inform client and exit
            if((fp=fopen(filename, "r"))==NULL){
                printf("Opening file error. Closing connection\t\t");
                if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                    printf("\n");
                    printf("Sending error message failed!\t\t\t\t");
                }
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            printf("-GET command received:\n");
            
            //getting file statistic
            if(stat(filename, &st)!=0){
                printf("Getting file statistics error. Closing connection\t");
                if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                    printf("\n");
                    printf("Sending error message failed!\t\t\t\t");
                }
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            
            //getting file size in network order byte
            filesize = htonl(st.st_size);
            //getting timestamp in network order byte
            time = htonl(st.st_mtime);
            
            //sending ok reply message to client with attached file size and timestap
            if((sendn(socket, OK_MSG, sizeof(OK_MSG)-1, 0))!=(sizeof(OK_MSG)-1)){
                printf("Sending ok message failed. Closing connection\t");
                if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                    printf("\n");
                    printf("Sending error message failed!\t\t\t");
                }
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            
            //sending filesize to client
            if((sendn(socket, &filesize, sizeof(uint32_t), 0))!=sizeof(uint32_t)){
                printf("Sending filesize failed. Closing connection\t");
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            
            //sending timestamp of the last file modification to client
            if((sendn(socket, &time, sizeof(uint32_t), 0))!=sizeof(uint32_t)){
                printf("Sending timestap failed. Closing connection\t");
                Close(socket);
                printf("-> Connection closed\n");
                return;
            }
            
            //sending bytes of the requested file to client
            printf("Sending file to client\t\t\t\t");
            sentsize = 0;
            while(sentsize<st.st_size){
                
                //switch file pointer to the new position
                if(fseek(fp, sentsize, SEEK_SET)==-1){
                    printf("\n");
                    printf("Error in switching file pointer. Closing Connection\t");
                    Close(socket);
                    fclose(fp);
                    printf("-> Connection closed\n");
                    return;
                }
                //read from file
                numreadchar = (uint32_t)fread(sndbuffer, sizeof(char), SNDBUFFERLENGTH, fp);
                //send data to the client
                if(sendn(socket, sndbuffer, numreadchar, 0) != numreadchar){
                    printf("\n");
                    printf("Sending file failed. Closing connection\t\t");
                    Close(socket);
                    fclose(fp);
                    printf("-> Connection closed\n");
                    return;
                }
                //update number of bytes size
                sentsize += numreadchar;
            }
            printf("-> File sent\n");
            fclose(fp);
        
        } else{
            //other problems, invalid commands, reply with error message, close connection
            printf("Invalid command received. Closing connection\t");
            if((sendn(socket, ERR_MSG, sizeof(ERR_MSG)-1, 0))!=(sizeof(ERR_MSG)-1)){
                printf("\n");
                printf("Sending error message failed. Closing connection\t");
            }
            Close(socket);
            printf("-> Connection closed\n");
            return;
            
        }
    }
    return;
}


//signal handler for SIGPIPE signal 
static void sigpipeHandler(int signo){
    printf("-> Broken pipe!\n");
    return;
}












