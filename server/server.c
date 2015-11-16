#include<stdio.h>	 
#include<stdlib.h>	  
#include<string.h>	
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>	
#include<sys/socket.h>	
#include<netinet/in.h>	
#include<netdb.h>	
#include<openssl/md5.h>
#include<dirent.h>
#define MAX_PENDING 1
#define MAX_LINE 10000
#define	MAX_FILE_SIZE 1024

void sendError(void);
void recvError(void);
void listDirectory( int s );
void deleteFile( int s );

struct recvInfo{
	short int filename_len;
	char filename[MAX_LINE];
};

int main(int argc, char*argv[]){	
	// Allocates appropriate memory.
	struct sockaddr_in sin;	
	struct recvInfo info;
	char buf[MAX_LINE];	
	unsigned char fileData[MAX_FILE_SIZE];
	int len;	
	int s, new_s, portNum;
	int opt = 1;

	//check for correct number of arguments
	if (argc!=2)	{
		fprintf(stderr, "error: incorrect number of arguments\n");
	}	else	{
		//set port number to first argument.
		portNum=atoi(argv[1]);
	}

	/*build  address  data  structure*/
	bzero((char *)&sin, sizeof(sin));	
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr =	INADDR_ANY;

	/* Set Server port from command line. */
	sin.sin_port = htons(portNum);	

	/*setup passive open*/	
	if((s	= socket(PF_INET, SOCK_STREAM, 0)) < 0){	
		perror("simplex-­.talk:	socket");	
		exit(1);	
	}	

	/*set socket options*/
	if((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)& opt, sizeof(int)))<0){	
		perror("simplex-­.talk:setscokt");	
		exit(1);	
	}	
	if((bind(s, (struct sockaddr*)&sin, sizeof(sin))) < 0){	
		perror("simplex-­.talk:	bind");	
		exit(1);	
	}	
	if((listen(s, MAX_PENDING))<0){	
		perror("simplex-­.talk: listen");	
		exit(1);	
	}	

	/*wait for connection, then receive and print text*/	
	while(1){
		//wait for connection state

		if((new_s = accept(s, (struct sockaddr*)&sin, (socklen_t *)&len)) < 0){	
			perror("simplex-­.talk:	accept");	
			exit(1);
		}
		//wait for operation state

		//REQ
		//determine operation
		if((len = recv(new_s, buf, sizeof(buf),0)) == -1){
			perror("Server Received Error!");
			exit(1);
		}
		if (strcmp(buf, "REQ")==0)  {
			//case "REQ":
			printf("recieved: %s\n", buf);
			memset(buf, 0, strlen(buf));

			//Receiving size of file nam
			if((len = recv(new_s, buf, sizeof(buf),0)) == -1){
				perror("Server Received Error!");
				exit(1);
			}

			//store filename length into struct and clear receiving buffer when done
			info.filename_len = htons((short) atoi(buf));
			memset(buf,0,strlen(buf));

			//Receiving file name
			if((len = recv(new_s, buf, sizeof(buf),0)) == -1){
				perror("Server Received Error!");
				exit(1);
			}

			//copy contents of buffer into filename and clear buffer once more
			strcpy(info.filename, buf);
			memset(buf,0,strlen(buf));

			//initialize file pointer to open file 
			FILE *fp;
			struct stat st;

			//set up file path to afs directory
			char path[200];
			bzero(path, 200);
			strcat(path, "/afs/nd.edu/coursefa.15/cse/cse30264.01/files/program4/");
			strcat(path, info.filename);

			/* Check if file exists */		
			int file_size=0;
			if(stat(path, &st) != 0){
				printf("File does not exist.\n");
				file_size=-1;
				if (send(new_s, &file_size, sizeof(file_size), 0) ==-1)	{
					perror("Error sending server's error message\n");
					exit(1);
				}
				exit(1);
			}


			/* Open File */
			fp=fopen(path, "rt");
			if(fp==NULL){
				printf("Error opening file.\n");
				exit(1);
			}

			/*send filesize back to client. */
			fseek(fp, 0L, SEEK_END);
			file_size = ftell(fp);
			if(send(new_s, &file_size, sizeof(file_size), 0) == -1)	{
				perror("server send error!");
				exit(1);
			}
			rewind(fp);	//rewind file pointer to compute hash
			/* Determines MD5 hash value of the file. */
			MD5_CTX mdContext;
			int bytes;
			unsigned char data[1024];
			unsigned char c[MD5_DIGEST_LENGTH];

			MD5_Init (&mdContext);
			while ((bytes = fread (data, 1, 1024, fp)) != 0)
				MD5_Update (&mdContext, data, bytes);
			MD5_Final (c,&mdContext);

			/* Send MD5 hash value of the file back to the client. */
			if (send(new_s, c, sizeof(c), 0) == -1)	{
				perror("Hash not sent successfully...\n");
				exit(1);
			}	

			/* Send file contents to client. */	
			//rewind file pointer to send contents of file
			rewind(fp);
			int n;
			//initially clear buffer that file data will be read into
			bzero(fileData, MAX_FILE_SIZE);
			//while there is still data being read...
			//		//send the packet and clear buffer to keep sending packets
			while ((n=fread(fileData, sizeof(char), MAX_FILE_SIZE, fp)) > 0) {
				if (send(new_s, fileData, n, 0) == -1)	{
					perror("File data not sent successfully...\n");
					exit(1);
				}	
				bzero(fileData, MAX_FILE_SIZE);
			}
			/* Close file */	
			fclose(fp);

			/* Cleanup */
			bzero(fileData, MAX_FILE_SIZE);
		} else if (strcmp(buf, "UPL")==0)	{
			//UPL case
			printf("recieved: %s\n", buf);
			memset(buf, 0, strlen(buf));

			//Receiving size of file name
			if((len = recv(new_s, buf, sizeof(buf),0)) == -1){
				perror("Server Received Error!");
				exit(1);
			}

			//store filename length into struct and clear receiving buffer when done
			info.filename_len = htons((short) atoi(buf));
			memset(buf,0,strlen(buf));

			//Receiving file name
			if((len = recv(new_s, buf, sizeof(buf),0)) == -1){
				perror("Server Received Error!");
				exit(1);
			}

			//copy contents of buffer into filename and clear buffer once more
			memset(buf,0,strlen(buf));
			strcpy(info.filename, buf);
			FILE *file;
			file=fopen(info.filename, "w+");
			if (file == NULL){
      	printf("Could not open file\n");
			}
			
			int downloaded=0;       //keep track of total characters downloaded
			int buffer_len=0;       //keep track of buffer length so last packet is correct size
			int file_size;
			//client should send size of file to server
      while (downloaded < file_size){
        
        //while client has not received the entire file size...
        if ((file_size - downloaded) >= MAX_LINE){
        	buffer_len=MAX_LINE;
        }else{
        	buffer_len=(file_size-downloaded);
        }

		    //receive packet from server and read into buf buffer

		    if ((recv(s, buf, buffer_len, 0)) == -1){
          perror("error receiving file from server\n");
          exit(1);
		    }
		    //write contents of buffer directly into file and clear buffer
		    fwrite(buf, sizeof(char), buffer_len, file);
		    bzero(buf, buffer_len);
		    downloaded+=buffer_len; //increment size of downloaded file

				memset(buf,0,strlen(buf));
			}
			//initialize file pointer to open file 

		} else if (strcmp(buf, "LIS")==0) {
			//LIS case
			listDirectory(new_s);
		} else if (strcmp(buf, "DEL")==0) {
			//DEL case
			deleteFile(new_s);
		} else if (strcmp(buf, "XIT")==0) {
			//XIT case
			close(new_s);
		}
		return 0;
	}
};

// Function to send contents of directory to client one-by-one
void listDirectory( int s ){
	int i;
	char buffer[100];
	
	//Two loops: 1. file size, 2. each item in directory
	for(i=0; i<2; i++){
	
		DIR *d;
		struct dirent *dir;
		int dirSize = 0;
		d = opendir(".");
		
		//If there is no errror opening directory
		if (d){
		 
		 	//Loop and perform actions while filename is not ".", ".."
		  while ( (dir = readdir(d)) != NULL){
		  	if( strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name, "..") != 0 ){
				 
				  if( i == 0 ){ //Compute directory size
				  	dirSize++;
				  }else{ //Send directory contents
				  	//sprintf(buffer, "%s", dir->d_name);
				  	if( send(s, dir->d_name, strlen(dir->d_name), 0) == -1 ){
							sendError();
						}
	  				//Receive a confirmation to see if client got filename
						if( recv(s, buffer, sizeof(buffer), 0) == -1 ){
							printf("error communicating directory\n");
							return;
						}
						printf("RECV: %s\n", buffer);
						//Clear the buffer
	  				memset(buffer,0,sizeof(buffer));
				  }
				  
		    }
		  }
		 
		 	closedir(d);
		 
		 	//Send number of files during first pass
		  if(i == 0){
		  	if( send(s, &dirSize, sizeof(dirSize), 0) == -1 ){
		  		sendError();
		  	}
		  }
		}
  }
}

void deleteFile( int s ){
	struct stat sts;
	char buffer[100];
	char fileName[100];
	int recvLength, nameLength, confirmation;
	
	//Clear the buffer
	memset(buffer,0,strlen(buffer));
	
	//Listen for file name length
	if( ( recvLength = recv( s, buffer, sizeof(buffer), 0) ) == -1 ){
		recvError();
	}
	nameLength = atoi( buffer );

	//Clear the buffer
	memset(buffer,0,strlen(buffer));
	
	//Listen for the name of file
	if( ( recvLength = recv( s, buffer, sizeof(buffer), 0) ) == -1 ){
		recvError();
	}	
	strcpy(fileName, buffer);

	//Clear the buffer
	memset(buffer,0,sizeof(buffer));

	//If string information doesn't match, send negative (-1)
	if( strlen(fileName) != nameLength ){
		confirmation = -1;
	}else{
		confirmation = 1;
	}
	
	//Check if file exists
	if( stat(fileName, &sts) == -1 ){
		confirmation = -1;
	}
	
	//Send confirmation to client
	if( send(s, &confirmation, sizeof(confirmation), 0) == -1){
		sendError();
	}
	
	//Return if confirmation was negative, aka something above went wrong
	if( confirmation == -1 ){ return; }

	//Clear the buffer
	memset(buffer,0,strlen(buffer));

	//Receive confirmation of delete
	if( recv(s, buffer, sizeof(buffer), 0) == -1){
		recvError();
	}

	//If compare is yes, delete file
	if( strcmp(buffer, "yes") == 0 || strcmp(buffer, "Yes") == 0 ){
		
		//Attempt to remove file
		if( remove(fileName) == -1 ){
			confirmation = -1;
		}else{
			confirmation = 1;
		}
		
		//Send confirmation to client that file was deleted
		if( send(s, &confirmation, sizeof(confirmation), 0) == -1){
			sendError();
		}
		
	}

	return;
}

void sendError(void){
	perror("Data not sent successfully...\n");
	exit(1);
}
void recvError(void){
	perror("Data not received successfully...\n");
	exit(1);
}
