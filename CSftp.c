#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include "dir.h"
#include "usage.h"
#include "CSftp.h"
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define BACKLOG 5
#define FILE_CHUNK 512

//global vars for main socket connection
struct sockaddr_in clientAddress;
socklen_t clientAddressLength = sizeof(struct sockaddr_in);

//global vars for passive socket connection
struct sockaddr_in dataAddress;
socklen_t dataAddressLength = sizeof(struct sockaddr_in);
int passive_mode = 0;
int passiveSock_fd = 0;
int pasv_client_fd = 0;

// function declarations
void getCredentials(int client_fd);
void PASVcommand(int client_fd);
void RETRcommand(int client_fd, char* fileName);
void NLSTcommand(int client_fd);
char* getHostName();


// get the correct USER credentials for logging in
void getCredentials(int client_fd){

  ssize_t recvBytes;
  char buffer[1024];
  bzero(buffer, 1024);

  dprintf(client_fd, "220 Welcome!\r\n");
  
  //get user commands
  while(1){

    recvBytes = recv(client_fd, buffer, 1024, 0);
    if (recvBytes < 0)
    {
      perror("Failed to read from the socket");
      break;
    }
    if (recvBytes == 0)
    {
      dprintf(client_fd, "421 Service not available, user interrupt. Connection closed.\n");
      break;
    }

    char *tokens[10];
    int i = 0;
    tokens[i] = strtok(buffer, " ");
    
    while(tokens[i] != NULL)
    {
      tokens[++i] = strtok(NULL, " ");
    }

    // if user types "QUIT"
    if(strncasecmp(tokens[0], "QUIT", 4) == 0){

        dprintf(client_fd, "221 Goodbye.\r\n");
        close(client_fd); 
        break;

    }

    // if user does not log in with USER cs317
    if(i <= 0 || i != 2 || strncasecmp(tokens[0], "USER", 4) != 0 || strncmp(tokens[1], "cs317", 5) != 0)
    {
      dprintf(client_fd, "530 Please login with USER.\r\n");
      continue;
    }
    else{
      dprintf(client_fd, "230 Login successful.\r\n");
    }

    break;

  }

}


int main(int argc, char **argv) {

    // Check the command line arguments
    if (argc != 2) {
      usage(argv[0]);
      return -1;
    }
  
  char* port_number = argv[1];
  int sock_fd, client_fd, one;
  struct addrinfo hints;
  struct addrinfo *server_info, *info;

  // set up for socket connection
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;


  int getaddrinfo_res = getaddrinfo(NULL, port_number, &hints, &server_info);
  if(getaddrinfo_res < 0){
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getaddrinfo_res));
    return -1;
  }
    
  for(info = server_info; info != NULL; info = info->ai_next) {

	  sock_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if(sock_fd < 0){
			perror("Creating sock_fd error");
			return -1;
		}

		int setsockopt_res = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    if(setsockopt_res < 0) {
			perror("Function setsockopt error");
			return -1;
		}

		int bind_res = bind(sock_fd, info->ai_addr, info->ai_addrlen);
    if(bind_res < 0 || info == NULL) {
			close(sock_fd);
			perror("Binding socket error");
			return -1;
		}

		break;
	}

	freeaddrinfo(server_info);

	int listen_res = listen(sock_fd, BACKLOG);
  if(listen_res < 0){
		perror("listen ERROR");
		return -1;
	}
  
  printf("Server is waiting for connections...\n");

  // loop to find waiting connections
  while(1){

      client_fd = accept(sock_fd, (struct sockaddr *)&clientAddress, &clientAddressLength);
      if (client_fd < 0) {
          perror("accept ERROR");
          continue;
        }

      // when accepted, create a new thread and enter getClientComamnd function
      pthread_t thread;
      
      int pthreadCreate_res = pthread_create(&thread, NULL, getClientCommand, &client_fd); 
      if(pthreadCreate_res < 0)
      {
          perror("Failed to create the thread");
          continue;
      }


    }

    return 0;
}

// gets client's commands and executes them until they quit
void* getClientCommand(void* args)
{
    int client_fd = *((int *) args);
    
    // get the user to log in first
    getCredentials(client_fd);

    // set up buffer to receieve client input
    char buffer[1024];
    bzero(buffer, 1024);
    ssize_t sendBytes, recvBytes;

    // save the parent directory for calls to CDUP
    char cwd_Buffer[1024];
    char* cwd = getcwd(cwd_Buffer, sizeof(cwd_Buffer));

    // loop and get client input
    while (1)
    {
        
        recvBytes = recv(client_fd, buffer, 1024, 0);
        if (recvBytes < 0)
        {
            perror("Failed to read from the socket");
            break;
        }
        if (recvBytes == 0)
        {
            perror("EOF or Client has disconnected.");
            dprintf(client_fd, "421 Service not available, user interrupt. Connection closed.\r\n");
            break;
        }

        
      // parsing user input into array of words
      char *tokens[50];
	    char *s;
      int nargs;
	   
      memset(&tokens, 0, sizeof(tokens));

      for (nargs = 0, s = strtok(buffer, " "); s != NULL; s = strtok(NULL, " "), nargs++) {
        if (nargs >= 50) {
          dprintf(client_fd, "500 Too many command line arguments.");
          continue;
        }

        tokens[nargs] = s;
      }
      tokens[nargs] = NULL;

      // if no commands given, go back to waiting for input
      if (nargs == 0){
        continue;
      }

      // nargs buggy; manually set number of args for arguments checking
      // if(tokens[0] != NULL && tokens[1] != NULL && tokens[2] == NULL){
      //    nargs = 2;
      // }


    // based on user input, do command
    if(strncasecmp(tokens[0], "QUIT", 4) == 0){

        dprintf(client_fd, "221 Goodbye.\r\n");
        close(client_fd); 
        break;

    } else if(strncasecmp(tokens[0], "USER", 4) == 0) {

         dprintf(client_fd, "530 Can't change from guest user.\r\n"); 
         continue;
         
    } else if(strncasecmp(tokens[0], "CWD", 3) == 0) {

        if(nargs != 2){
          dprintf(client_fd, "501 Syntax error in parameters or arguments.\r\n"); 
          continue;

        } else {

        CWDcommand(client_fd, tokens[1]);
        continue;

       }  
          
    } else if(strncasecmp(tokens[0], "CDUP", 4) == 0) {

         CDUPcommand(client_fd, cwd);
         continue;
         
    } else if(strncasecmp(tokens[0], "TYPE", 4) == 0) {

       if(nargs != 2){
          dprintf(client_fd, "501 Syntax error in parameters or arguments.\r\n"); 
          continue;

        } else {
          TYPEcommand(client_fd, tokens[1]);
          continue;

        }


    } else if(strncasecmp(tokens[0], "MODE", 4) == 0) {

         if(nargs != 2){
          dprintf(client_fd, "501 Syntax error in parameters or arguments.\r\n"); 
          continue;

        } else {
          MODEcommand(client_fd, tokens[1]);
          continue;
        }

    } else if(strncasecmp(tokens[0], "STRU", 4) == 0) {

          if(nargs != 2){
            dprintf(client_fd, "501 Syntax error in parameters or arguments.\r\n"); 
            continue;

        } else {
          STRUcommand(client_fd, tokens[1]);
          continue;
        }


    } else if(strncasecmp(tokens[0], "PASV", 4) == 0) {

          PASVcommand(client_fd);
          continue;

    } else if(strncasecmp(tokens[0], "NLST", 4) == 0) {

        NLSTcommand(client_fd);
        continue; 

    } else if(strncasecmp(tokens[0], "RETR", 4) == 0) {

        if(nargs != 2){

          dprintf(client_fd, "501 Syntax error in parameters or arguments.\r\n"); 
          continue;

        } else {

           RETRcommand(client_fd, tokens[1]); 
           continue;

        }

    } else {

        dprintf(client_fd, "500 Unknown command.\r\n"); 
        continue;
       
    }

    break;
    
  }

  return NULL;
}

// set up passive socket for NLST and RETR
void PASVcommand(int client_fd) {

    struct sockaddr_in passive_socket;

	  passiveSock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(passiveSock_fd < 0) {
			perror("passive socket creation ERROR");
			return;
		}

    memset(&passive_socket, 0, sizeof(passive_socket));

    // random number generator for port number
    srand(time(0));
    unsigned short random;
    random = (unsigned short) (rand() % (65535 + 1 - 1024)) + 1024;

    // fill in passive_socket info
    passive_socket.sin_family = clientAddress.sin_family;
    char* serverIP = getHostName();
    inet_pton(AF_INET, serverIP, &(passive_socket.sin_addr.s_addr));
		passive_socket.sin_port = htons(random);

    int bind_res = bind(passiveSock_fd, (struct sockaddr *) &passive_socket, sizeof(passive_socket));
    if(bind_res < 0){
			perror("passive socket bind ERROR");
			return;
		}

	 int listen_res = listen(passiveSock_fd, 1); 
   if(listen_res < 0){
			perror("passive socket listen ERROR");
			return;
		}

  // gather IP address and port info to print
   uint32_t addr = passive_socket.sin_addr.s_addr;
   int new_port = ntohs(passive_socket.sin_port);
   int p1 = new_port/256;
   int p2 = new_port%256;

   // for ease of testing
   printf("new_port: %d\n", new_port);

   dprintf(client_fd, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\r\n",
           addr & 0xff, (addr >> 8) & 0xff, (addr >> 16) & 0xff,(addr >> 24) & 0xff, p1, p2);

    passive_mode = 1; 

    return;

}

//print current directory
void NLSTcommand(int client_fd){

  if(passive_mode == 1){


      pasv_client_fd = accept(passiveSock_fd, (struct sockaddr *)&dataAddress, &dataAddressLength);
      if(pasv_client_fd < 0){
			  perror("passive socket accept ERROR");
			  return;
		}

      dprintf(client_fd, "150 Here comes the directory listing.\r\n");
      char cwd_Buffer[1024];
      char* currentWorkingDirectory = getcwd(cwd_Buffer, sizeof(cwd_Buffer));

      // list files in current directory
			listFiles(pasv_client_fd, cwd_Buffer);
      dprintf(client_fd, "226 Directory send okay.\r\n");
						
			close(passiveSock_fd);
			close(pasv_client_fd);
			passive_mode = 0;

  }else{
    
    dprintf(client_fd, "425 Use PASV first.\r\n");
  }

  return;
}

void RETRcommand(int client_fd, char* fileName){

    if(passive_mode == 1){

          pasv_client_fd = accept(passiveSock_fd, (struct sockaddr *)&dataAddress, &dataAddressLength);
          if(pasv_client_fd < 0){
            perror("passive socket accept ERROR");
            return;
          }
      
        // neccessary for passing strings
        char* fileNameTrunc = strtok(fileName, " \t\r\n");
        int pathlength = strlen(fileName);


        dprintf(client_fd, "150 Opening data connection for %s\n", fileName);

        // open file descriptor for file
        int file_fd = open(fileName, O_RDONLY, 0);
        if(file_fd < 0){
          perror("could not open file");
          dprintf(client_fd, "550 Failed to open file.\r\n");
          return;

        }

        // setting up file transfer
        int readBytes, sendBytes;
        char buffer[1024];
        fileName[pathlength] = '\0';


        // send bytes to passive socket
        while ((readBytes = read(file_fd, buffer, sizeof(buffer))) > 0) {
          ssize_t writeBytes = write(pasv_client_fd, buffer, readBytes);
          if (writeBytes != readBytes) {
            fprintf(stderr, "%s\n", strerror(errno));
            break;
          }
        }
        
       (void)close(file_fd);

      // transfer completed
      dprintf(client_fd, "226 Transfer Complete.\n");
			close(pasv_client_fd);
			close(pasv_client_fd);
			passive_mode = 0;

    } else {

      dprintf(client_fd, "425 Use PASV first.\r\n");

    }

  return;
}

//helper function that will get server's IP address
char* getHostName(){

   char host[256];
   char *IP;
   struct hostent *host_entry;
   if ( gethostname(host, sizeof(host)) <0  ) {
    perror("Failed to get host name");
    return NULL;
   }   
   if ( (host_entry = gethostbyname(host)) == NULL ) {
    perror("Failed to get host IP address");
    return NULL;
   }
   IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
   printf("Hostname is %s and IP is %s\n",host,IP);
   return IP;

}