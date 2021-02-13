#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

struct sockaddr_in pasv;
int pasvstatus = 0;
char initialDir[1024];
bool isLoggedIn = false;
int port1;
int port2;
char* passiveip[1024];
bool passivemode = false;
fd_set readfds;
struct timeval tv;


bool Passivemode(){
    if(pasvstatus == 0)
    {
            pasvstatus = socket(AF_INET , SOCK_STREAM , 0); 
            FD_SET(pasvstatus,&readfds);
        if (pasvstatus < 0) {
            perror("421 Failed to set the socket option");
            return false;
            }
        int value = 1;   
    if (setsockopt(pasvstatus, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
    {
        perror("421 Failed to set the socket option");
        return false;
    }
      
    //bind the socket to a port
    struct sockaddr_in address;
    bzero(&address, sizeof(struct sockaddr_in));
    struct ifaddrs * ADDR=NULL;
    //get a IP address to bind with
    getifaddrs(&ADDR);
    struct ifaddrs * temp=NULL;
    for(temp = ADDR; temp != NULL; temp = temp->ifa_next){
        //check 127.0.0.1 and ipv6
        if(temp->ifa_addr->sa_family == AF_INET6||strcasecmp(temp->ifa_name, "lo") == 0)
        { //printf("skip:%s\n",temp->ifa_name); 
            continue;}
        //check ipv4
        if(temp->ifa_addr->sa_family == AF_INET)
        { 
          //get ip from interface
          struct in_addr new;
          new=((struct sockaddr_in *)temp->ifa_addr)->sin_addr;
          address.sin_addr.s_addr = new.s_addr;
          break;
        }
    }
    address.sin_family = AF_INET;
    address.sin_port = 0;
    if (bind(pasvstatus, (const struct sockaddr*) &address, sizeof(struct sockaddr_in)) != 0)
    {
      perror("421 Failed to bind the PASV socket");
      return false;
    }
    //get port number
    socklen_t size = sizeof(struct sockaddr);
    getsockname(pasvstatus, (struct sockaddr *) &address, &size);
    if (listen(pasvstatus, 10) != 0)
    {
      perror("421 Failed to listen for PASV connections");
      return false;
    }
    printf("pasdes : %d\n",pasvstatus);
    char ip[INET_ADDRSTRLEN];
    struct in_addr newip;
    newip=address.sin_addr;
    inet_ntop(AF_INET, &newip, ip, INET_ADDRSTRLEN);
    printf("port:%d\n",ntohs(address.sin_port));
    port1 = ntohs(address.sin_port)/256;
    port2 = ntohs(address.sin_port)%256; 
    int i = 0;
    char *tokenip = strtok (ip, ".");
    while (tokenip != NULL)
    {
      passiveip[i] = tokenip;
      //printf("check:%s\n",passiveip[i]);
      i++;
      tokenip = strtok (NULL, ".");
    }

    return true;
        
    }
    return false;
   
}

void trim_n(char* a){
    char *tmp;
if ((tmp = strrchr(a, '\n')) != NULL) {
   *tmp = 0;
}
    char *tmp2;
if ((tmp = strrchr(a, '\r')) != NULL) {
   *tmp = 0;
}

}

void* interact(void* args)
{
    int clientd = *(int*) args;
    
    // Interact with the client
    char buffer[1024];
    char *welcomemessage = "220 Welcome\n"; 
    char *goodbyemessage = "byebye\n";
    if (send(clientd, welcomemessage, strlen(welcomemessage), 0) != strlen(welcomemessage))
        {
            perror("Failed to send to the socket1");
            
        }
    getcwd(initialDir, 1024);

    while (true)
    {
        bzero(buffer, 1024);
        
        // Receive the client message
        ssize_t length = recv(clientd, buffer, 1024, 0);
        char tbuffer[1024];
        strcpy(tbuffer,buffer);
        trim_n(tbuffer);
        printf("received: %s\n",tbuffer);

        if (length < 0)
        {
            perror("Failed to read from the socket");
            
            break;
        }
        
        if (length == 0)
        {
            printf("EOF\n");
            
            break;
        }
        
        // Do something to the client message
        // Get Command
        char *arg[1024];
        int i = 0;
        char * split = strtok(tbuffer," ");
        while (split != NULL){
          arg[i]=split;
          printf("checkarg:%s\n",arg[i]);
          i++;
          split = strtok(NULL, " ");        
        }


        if (strcasecmp(arg[0], "quit") == 0) {
          if(i == 1) {
              send(clientd , goodbyemessage, strlen(goodbyemessage), 0);
              isLoggedIn = false;
              break;
          } else {
            length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
          }
        } else if(strcasecmp(arg[0],"user")==0) {
            printf("usercheck:%s\n",arg[1]);
            if(i != 2){
                length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
            } else {
                if(strcasecmp(arg[1], "cs317") == 0) {
                    length = snprintf(buffer, 1024, "%s\r\n", "230 - Login successful.\n");
                    isLoggedIn = true;
                } else {
                    length= snprintf(buffer,1024, "%s\r\n", "530 - Not logged in.\n");
                }
            }
        } else {
            if (!isLoggedIn) {
                length= snprintf(buffer,1024, "%s\r\n", "503 - Login with USER first.");
            } else {
                if(strcasecmp(arg[0],"cwd")==0) {
                    if (i != 2) {
                        length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
                    } else {
                        char *dir = arg[1];
                        if (dir == NULL) {
                            length= snprintf(buffer,1024, "%s\r\n", "550 - Failed to change directory.");
                        } else {
                            char dircpy[1024];
                            strcpy(dircpy, dir);
                            char *temp = strtok(dir, "/\r\n");
                            if (strcmp(temp, "..") == 0 || strcmp(temp, ".") == 0) {
                                length= snprintf(buffer,1024, "%s\r\n", "550 - Directory cannot start with ../ or ./");
                            } else {
                                bool failed = false;
                                temp = strtok(NULL, "/\r\n");
                                while(temp != NULL) {
                                    if (strcmp(temp, "..") == 0) {
                                        length= snprintf(buffer,1024, "%s\r\n", "550 - Directory cannot contain ../");
                                        failed = true;
                                        break;
                                    }
                                    temp = strtok(NULL, "/\n");

                                }
                                if (!failed) {
                                    if (chdir(dircpy) == 0) {
                                        length= snprintf(buffer,1024, "%s\r\n", "250 - Directory successfully changed.");
                                    } else {
                                        length= snprintf(buffer,1024, "%s\r\n", "550 - Directory failed to changed.");
                                    }
                                }
                            }
                        }
                    }
                } else if (strcasecmp(arg[0],"cdup")==0) {
                    if(i != 1){
                        length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
                    } else {
                        char currentDir[1024];
                        getcwd(currentDir, 1024);
                        if (strcmp(currentDir, initialDir) == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "550 - Cannot access parent directory of root directory");
                        } else {
                            if (chdir("..") == 0){
                                length= snprintf(buffer,1024, "%s\r\n", "250 - Directory successfully changed.");
                            } else {
                                length= snprintf(buffer,1024, "%s\r\n", "550 - Directory failed to changed.");
                            }
                        }
                    }
                } else if (strcasecmp(arg[0], "type") == 0) {
                    if(i != 2){
                        length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
                    } else {
                        if (strcasecmp(arg[1], "I") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "200 - Switching to Image Mode");
                        } else if (strcasecmp(arg[1], "A") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "200 - Switching to ASCII mode");
                        } else if (strcasecmp(arg[1], "E") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - EBCDIC type not implemented");
                        } else if (strcasecmp(arg[1], "L") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Local byte Byte size type not implemented");
                        } else if (strcasecmp(arg[1], "N") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Non-print type not implemented");
                        } else if (strcasecmp(arg[1], "T") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Telnet format effectors type not implemented");
                        } else if (strcasecmp(arg[1], "C") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Carriage Control (ASA) type not implemented");
                        } else {
                            length= snprintf(buffer,1024, "%s\r\n", "501 - TYPE not supported");
                        }
                    }
                } else if (strcasecmp(arg[0], "mode") == 0) {
                    if(i != 2){
                        length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
                    } else {
                        if (strcasecmp(arg[1], "S") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "200 - Mode set to stream");
                        } else if (strcasecmp(arg[1], "B") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Block Mode not implemented");
                        } else if (strcasecmp(arg[1], "C") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Compressed Mode not implemented");
                        } else {
                            length= snprintf(buffer,1024, "%s\r\n", "501 - MODE not supported");
                        }
                    }
                } else if (strcasecmp(arg[0], "stru") == 0) {
                    if(i != 2){
                        length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");
                    } else {
                        if (strcasecmp(arg[1], "F") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "200 - File Structure set to No record structure");
                        } else if (strcasecmp(arg[1], "R") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Record structure not implemented");
                        } else if (strcasecmp(arg[1], "P") == 0) {
                            length= snprintf(buffer,1024, "%s\r\n", "504 - Page structure not implemented");
                        } else {
                            length= snprintf(buffer,1024, "%s\r\n", "501 - MODE not supported");
                        }
                    }
                } else if (strcasecmp(arg[0], "pasv") == 0){
                    bool connectionSuccessful = Passivemode() ;
                    if(connectionSuccessful == false){
                    char* fail = "421 Service not available, closing control connection. This may be a reply to any command if the service knows it must shut down.\r\n";
                    send(clientd,fail, strlen(fail) , 0 );
                    continue;}
                    else{
                    passivemode = true;
                    length = snprintf(buffer, 1024, "%s%s,%s,%s,%s,%d,%d%s\r\n", "227 Entering Extended Passive Mode (", passiveip[0], passiveip[1], passiveip[2], passiveip[3], port1, port2, ").");
                    }
                }else if (strcasecmp(arg[0], "nlst") == 0){
                    /*int n = pasvstatus + 1;
                    struct timeval tv;
                    tv.tv_sec = 40;
                    tv.tv_usec = 0;
                    int rv = select(n, &readfds, NULL, NULL, &tv);
                    if (rv == -1) {
                    perror("select"); // error occurred in select()
                    } else if (rv == 0) {
                     printf("Timeout occurred! No data after 10.5 seconds.\n");
                    } else {
                        printf("successfully accepted");}*/
                    if(i!=1)
                    {length = snprintf(buffer, 1024, "%s\r\n", "501 - invalid number of arguments.");}
                    else{
                        if(!passivemode){
                            char* needpassive = "425 Use pasv first.\n";
                            send(clientd , needpassive, strlen(needpassive),0);
                            continue;
                        }
                    //set socket accept to data transfer

                        
                        
                    struct sockaddr_in pasvaddress;
                    socklen_t sizes = sizeof(struct sockaddr_in);
                    int fd = accept(pasvstatus, (struct sockaddr*) &pasvaddress, &sizes);
                    //accept fail
                    if (fd < 0)
                    {   char* fail1 = "425\n";
                        send(clientd , fail1, strlen(fail1) , 0 );
                        continue;
                        }
                    char* success = "150 File status okay; about to open data connection.\n";
                    send(clientd , success, strlen(success) , 0 );
                    getcwd(initialDir,1024);
                    int filesnumber = listFiles(fd,initialDir);
                    if(filesnumber == -1){
                    char* fail2 = "451 Requested action aborted. Local error in processing.\n";
                    send(clientd , fail2, strlen(fail2) , 0 );
                    } else if(filesnumber == -2){
                    char* fail3 = "452 Requested action not taken. Insufficient storage space in system.\n";
                    send(clientd , fail3, strlen(fail3) , 0 );
                    } else{
                    char* sendok = "226 Closing data connection.Requested file action successful\n";
                    send(clientd , sendok, strlen(sendok) , 0 );}
                    bzero(buffer, 1024);
                    close(fd);
                    }


                }
            }

        }
        // Send the server response
        printf("length: %zd\n",length);
        printf("sendback: %s\n",buffer);
        if (send(clientd, buffer, length, 0) != length)
        {
            perror("Failed to send to the socket");
            
            break;
        }
    }
    
    close(clientd);
    
    return NULL;
}

int main(int argc, char **argv) {

    int port = (int) strtol(argv[1],NULL,10);
    int i;
    
    // Check the command line arguments
    if (argc != 2) {
      usage(argv[0]);
      return -1;
    }

    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc
    
    // Create a TCP socket
    FD_ZERO(&readfds);
    int socketd = socket(PF_INET, SOCK_STREAM, 0);
    FD_SET(socketd,&readfds);
    if (socketd < 0)
    {
        perror("Failed to create the socket.");
        
        exit(-1);
    }
    
    // Reuse the address
    int value = 1;
    
    if (setsockopt(socketd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
    {
        perror("Failed to set the socket option");
        
        exit(-1);
    }
    
    // Bind the socket to a port
    struct sockaddr_in address;
    
    bzero(&address, sizeof(struct sockaddr_in));
    
    address.sin_family = AF_INET;
    
    address.sin_port = htons(port);
    
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    if (bind(socketd, (const struct sockaddr*) &address, sizeof(struct sockaddr_in)) != 0)
    {
        perror("Failed to bind the socket");
        
        exit(-1);
    }
    
    // Set the socket to listen for connections
    if (listen(socketd, 4) != 0)
    {
        perror("Failed to listen for connections");
        
        exit(-1);
    }
    
    while (true)
    {
        // Accept the connection
        struct sockaddr_in clientAddress;
        
        socklen_t clientAddressLength = sizeof(struct sockaddr_in);
        
        printf("Waiting for incomming connections...\n");
        
        int clientd = accept(socketd, (struct sockaddr*) &clientAddress, &clientAddressLength);
        
        if (clientd < 0)
        {
            perror("Failed to accept the client connection");
            
            continue;
        }
        
        printf("Accepted the client connection from %s:%d.\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
        
        // Create a separate thread to interact with the client
        pthread_t thread;
        
        if (pthread_create(&thread, NULL, interact, &clientd) != 0)
        {
            perror("Failed to create the thread");
            
            continue;
        }
        
        // The main thread just waits until the interaction is done
        pthread_join(thread, NULL);
        
        printf("Interaction thread has finished.\n");
    }
    
    return 0;




    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection
    


}
