#ifndef _CSFTPH__

#define _CSFTPH__

 #include <regex.h>

void* getClientCommand(void* args);
void CWDcommand(int client_fd, char pathName[]);
void CDUPcommand(int client_fd, char cwd[]);
void TYPEcommand(int client_fd, char type[]);
void MODEcommand(int client_fd, char mode[]);
void STRUcommand(int client_fd, char structure[]);


// changes current working directory to pathName
void CWDcommand(int client_fd, char pathName[]){

    //get path length
    int path_length = strlen(pathName);

    //neccessary for proper string passing
    char* pathNameTrunc = strtok(pathName, " \t\r\n");
    pathName[path_length-1] = '\0';

    // check for illegal directory changes
    if(path_length >= 2){

        // if pathName begins with ./, reject command
        if((pathName[0] == 46 && pathName[1] == 47))
            {
                dprintf(client_fd, "550 Failed to change directory.\r\n");
                return;
            }

         // if pathName contains ../, reject comamnd   
        if(path_length >= 3){
            for(int i = 0; i < path_length-3;i++){
                 if(pathName[i] == 46 && pathName[i+1] == 46 && pathName[i+2] == 47){
                     dprintf(client_fd, "550 Failed to change directory.\r\n");
                     return;
                 }
            }
        }
    }

    // if pathName does not exist, reject change, else change directory
    if(chdir(pathName) < 0){

         dprintf(client_fd, "550 Failed to change directory.\r\n");

    }
    else {

        dprintf(client_fd, "250 Directory successfully changed.\r\n");
    }

}

// go into parent directory of current directory
void CDUPcommand(int client_fd, char cwd[]){

    //printf("parent directory is: %s\n", cwd);
    
    char cwd_Buffer[1024];
    char* currentWorkingDirectory = getcwd(cwd_Buffer, sizeof(cwd_Buffer));

    //printf("currentWorkingDirectory  is: %s\n", currentWorkingDirectory);

    // don't let user go to any higher if already in parent directory
    if(strcmp(cwd, currentWorkingDirectory) == 0){
        dprintf(client_fd, "550 Failed to change directory.\r\n");
        return;
    }

    if (chdir("..") < 0) {

            dprintf(client_fd, "550 Failed to change directory.\r\n");
 
    } else {

            dprintf(client_fd, "250 Directory successfully changed.\r\n");
    }

}

//change the data type handled by FTP
void TYPEcommand(int client_fd, char type[]){

    char response[256];
    ssize_t sendBytes;

    if(strncasecmp(type, "A", 1) == 0){

        // sprintf(response,"200 Switching to ASCII mode.\r\n");
        // sendBytes = write(client_fd, response, strlen(response));
        dprintf(client_fd, "200 Switching to ASCII mode.\r\n");
        
    }  
    else if(strncasecmp(type, "I", 1) == 0){
        dprintf(client_fd, "200 Switching to Binary mode.\r\n");
        
    }
    else{
        dprintf(client_fd, "500 Unrecognised TYPE command.\r\n");
    
    }
}

//changes mode of data transfer of FTP
void MODEcommand(int client_fd, char mode[]){

    if(strncasecmp(mode, "S", 1) == 0){
        dprintf(client_fd, "200 Mode set to S.\r\n");
    }  
    else{

        dprintf(client_fd, "504 Bad MODE command.\r\n");

    }
}

//sets file system data structure to File Structure
void STRUcommand(int client_fd, char structure[]){

    if(strncasecmp(structure, "F", 1) == 0){
        dprintf(client_fd, "200 Structure set to F.\r\n");
    }  
    else{
        dprintf(client_fd, "504 Bad STRU command.\r\n");

    }
}




#endif


