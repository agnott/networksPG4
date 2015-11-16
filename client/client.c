
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
//#include <mhash.h>
#include <openssl/md5.h>
#define MAX_LINE 1024
#define	MAX_FILE_SIZE 1024
#define DEBUG 1

char sendline[MAX_LINE];
char buf[10];
char recline[1024];

int opREQ(int s, char *filename);
int opUPL(int s, char *filename);
int opDEL(int s, char *filename);
int opLIS(int s);

void clearline(char *line);
void dprint(char *msg);

int main(int argc, char * argv[])
{
	// Allocate appropriate memory
	struct hostent *hp;
	struct sockaddr_in sin;
	char *host;
	char filename[20];
	int s, port_number;
	//int len, port_number;
	//struct timeval start, end;

	//check for correct number of arguments
	if (argc==3) {  //name and three command line arguments
		host = argv[1];
		port_number = atoi(argv[2]);
	}
	else {
		fprintf(stderr, "error: too few arguments\n");
		exit(1);
	}

	/* translate host name into peer's IP address */
	hp = gethostbyname(host);
	if (!hp) {
		fprintf(stderr, "error: unknown host: %s\n", host);
		exit(1);
	}

	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(port_number);

	/* active open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("error: socket failure\n");
		exit(1);
	}
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{perror("error: connection error\n");
		close(s);
		exit(1);
	}

	//prompt user for operation state
	while ( strcmp(buf, "XIT") != 0 )	{
	
		//Clear sendline var
		clearline(sendline);
		printf("Pick an operation: REQ, UPL, LIS, DEL, XIT\n");
		scanf("%s", buf);
	
		if(send(s, buf, strlen(buf), 0)==-1){
			perror("client send error!");
			exit(1);
		}

		//REQ
		if (strcmp(buf, "REQ") == 0)	{	
			/* get length of file name and add it to filename string-- send
			 *          * the combination to server*/
			printf("Please enter a file to receive:\n");
			scanf("%s", filename);
			if(DEBUG==1) printf("file being sent: %s\n", filename);
			int res = opREQ(s, filename);
			if(res!=0) printf("REQ Error: %d\n", res);
		//UPL
		} else if (strcmp(buf, "UPL") == 0) {
			if(DEBUG==1) printf("command: %s\n", buf);
			printf("Please enter a file to upload:\n");
			scanf("%s", filename);
			if(DEBUG==1) printf("file being upload: %s\n", filename);
			int res = opUPL(s, filename);
			if(res!=0) printf("REQ Error: %d\n", res);
		//DEL
		} else if (strcmp(buf, "DEL") == 0) {
			if(DEBUG==1) printf("command: %s\n", buf);
			
			printf("Please enter a file to delete: \n");
			scanf("%s", filename);
			if(DEBUG==1) printf("file being deleted: %s\n", filename);
			int res = opDEL(s,filename);
			if(res!=0) printf("DEL Error: %d\n", res);
		//LIS
		} else if (strcmp(buf, "LIS") == 0) {
			if(DEBUG==1) printf("command: %s\n", buf);
			
			int res = opLIS(s);
			if(res!=0) printf("LIS Error: %d\n", res);
		} else {
			break;
		}

	}
	/* after file is received, close connection. */
	close(s);
	return 0;
}

int opREQ(int s, char *filename){
	struct timeval start, end;
	clearline(sendline);
	clearline(recline);

	short int filename_len = strlen(filename);
	//Send length of file name
	sprintf(sendline, "%d", filename_len);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}

	//Clear sendline var
	clearline(sendline);

	//Send the filename
	sprintf(sendline, "%s", filename);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}

	//Clear sendline var
	clearline(sendline);

	//clear the two buffers so they can continue to receive messages
	clearline(sendline);
	clearline(recline);

	/* receive size of requested file from server. Negative value, error */
	int file_size;
	int temp;
	if ((temp=(recv(s, &file_size, sizeof(file_size), 0))) == -1)    {
		perror("client recieved error");
		exit(1);
	}

	//if file does not exist on server--display error message and exit
	if (file_size == -1)    {
		perror("File does not exist on the server\n");
		exit(1);
	}

	/* receive MD5 Hash value from server and store for later use. */
	unsigned char serverHash[MD5_DIGEST_LENGTH];
	if (recv(s, serverHash, sizeof(serverHash), 0)==-1)     {
		perror("error receiving hash");
		exit(1);
	}

	//create file to store contents of file being receivd
	FILE *file;
	file=fopen(filename, "w+");      //create file with write permissions
	if (file == NULL)       {
		printf("Could not open file\n");

	}
	int downloaded=0;       //keep track of total characters downloaded
	int buffer_len=0;       //keep track of buffer length so last packet is correct size

	//find time when filename was first sent to calculate throughput later
	gettimeofday(&start, NULL);
	while (downloaded < file_size)  {
		//while client has not received the entire file size...
		//if there is enough for one full packet, send full packet's worth of data
		if ((file_size - downloaded) >= 1024)       {
			buffer_len = 1024;
		}       else    {
			buffer_len = (file_size-downloaded);
		}

		//receive packet from server and read into recline buffer
		if ((recv(s, recline, buffer_len, 0)) == -1)    {
			perror("error receiving file from server\n");
			exit(1);
		}
		//write contents of buffer directly into file and clear buffer
		fwrite(recline, sizeof(char), buffer_len, file);
		clearline(recline);
		downloaded+=buffer_len; //increment size of downloaded file

	}
	//get time once entire file has been received
	gettimeofday(&end, NULL);
	rewind(file);   //rewind file pointer to compute hash

	/*computes MD5 Hash value based on content compare to original MD5 */
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];
	unsigned char c[MD5_DIGEST_LENGTH];

	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, 1024, file)) != 0)
		MD5_Update (&mdContext, data, bytes);
	MD5_Final (c,&mdContext);

	/*if MD5 values don't match, signal error and exit */
	int l, error = 0;
	for (l=0; l<MD5_DIGEST_LENGTH; l++){
		if (c[l]!=serverHash[l]){
			printf("error: hashes do not match\n");
			error = -1;
		}
	}

	//Send successful or unsuccessful req sum
	if( send( s, &error, sizeof(error), 0) == -1){
		exit(1);
	}
	
	int j;
	for(j = 0; j < MD5_DIGEST_LENGTH; j++) printf("%02x", serverHash[j]);
	printf ("\n");
	
	for(j = 0; j < MD5_DIGEST_LENGTH; j++) printf("%02x", c[j]);
	printf ("\n");
	
	if(error == 0){
		//Say that hash matches, print out information
		printf("Hash Matches\n");
		printf("%i bytes transferred in %5.2f seconds\n", file_size, ((end.tv_sec + end.tv_usec/1000000.)- (start.tv_sec + start.tv_usec/1000000.)));
		printf("Throughput: %5.3f Megabytes/sec\n", (file_size/1000000.)/((end.tv_sec+end.tv_usec/1000000.)- (start.tv_sec + start.tv_usec/1000000.)));
		printf("File MD5sum: ");
	}else{
		//remove(filename);	
	}
	
	return 0;
}

int opUPL(int s, char *filename){
	clearline(sendline);
	clearline(recline);

	unsigned char fileData[MAX_FILE_SIZE];
	
	//initialize file pointer to open file 
	FILE *fp;
	struct stat st;

	//set up file path to afs directory
	char path[200];
	bzero(path, 200);
	//strcat(path, "/afs/nd.edu/coursefa.15/cse/cse30264.01/files/program4/");
	strcat(path, filename);

	// Check if file exists
	int file_size=0;
	if(stat(path, &st) != 0){
		printf("File does not exist.\n");
		exit(1);
	}
	
	short int filename_len = strlen(filename);
	//Send length of file name
	sprintf(sendline, "%d", filename_len);
	printf("len: %s\n", sendline);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}

	//Clear sendline var
	clearline(sendline);
	
	int confirmation;
	if ((recv(s, &confirmation, sizeof(confirmation), 0)) == -1){
		perror("error receiving confirmation from server\n");
		exit(1);
	}
	else{
		printf("%i\n", confirmation);
	}
	
	clearline(recline);

	//Send the filename
	sprintf(sendline, "%s", path);
	printf("name: %s\n", sendline);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}
	
	//Clear sendline var
	clearline(sendline);

	//Confirmation of receive filename
	if ((recv(s, &confirmation, sizeof(confirmation), 0)) == -1){
		perror("error receiving confirmation from server\n");
		exit(1);
	}
	else{
		printf("%i\n", confirmation);
	}

	// Open File
	fp=fopen(path, "rt");
	if(fp==NULL){
		printf("Error opening file.\n");
		exit(1);
	}

	// send filesize back to server.
	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);
	sprintf(sendline, "%d", file_size);
	if(DEBUG==1) printf("filesize: %s\n", sendline);
	//if(DEBUG==1) printf("%i\n", file_size);
	if(send(s, &sendline, sizeof(sendline), 0) == -1)	{
		perror("server send error!");
		exit(1);
	}
	rewind(fp);	//rewind file pointer to compute hash
	clearline(sendline);
	
	// Send file contents to server.
	//rewind file pointer to send contents of file
	rewind(fp);
	int n;
	//initially clear buffer that file data will be read into
	bzero(fileData, MAX_FILE_SIZE);
	//while there is still data being read...
	//send the packet and clear buffer to keep sending packets
	while ((n=fread(fileData, sizeof(char), MAX_FILE_SIZE, fp)) > 0) {
		if(DEBUG==1) printf("%s\n", fileData);
		if (send(s, fileData, n, 0) == -1)	{
			perror("File data not sent successfully...\n");
			exit(1);
		}	
		bzero(fileData, MAX_FILE_SIZE);
	}
	rewind(fp);

	// Determines MD5 hash value of the file.
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];
	unsigned char c[MD5_DIGEST_LENGTH];

	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, 1024, fp)) != 0)
		MD5_Update (&mdContext, data, bytes);
	MD5_Final (c,&mdContext);

	/*if MD5 values don't match, signal error and exit */
	int j;
	for(j = 0; j < MD5_DIGEST_LENGTH; j++) printf("%02x", c[j]);
	printf ("\n");
	
	// Send MD5 hash value of the file back to the server.
	if (send(s, c, sizeof(c), 0) == -1)	{
		perror("Hash not sent successfully...\n");
		exit(1);
	}
	clearline(sendline);
	clearline(recline);
	
	float throughput;
	printf("Size: %i\n", (int)sizeof(throughput));
	if ((recv(s, &throughput, sizeof(throughput), 0)) == -1){
		perror("error receiving confirmation from server\n");
		exit(1);
	}
	printf("%f\n", throughput);
	
	if(throughput<0){
		printf("Transfer unsuccessful.\n");	
	}
	else{
		printf("Throughput: %f Megabytes/sec\n", throughput);	
	}

	/* Close file */
	fclose(fp);

	/* Cleanup */
	bzero(fileData, MAX_FILE_SIZE);

	return 0;
}

int opDEL(int s, char *filename){
	clearline(sendline);
	clearline(recline);
	
	short int filename_len = strlen(filename);
	//Send length of file name
	sprintf(sendline, "%d", filename_len);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}

	//Clear sendline var
	clearline(sendline);

	//Send the filename
	sprintf(sendline, "%s", filename);
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client send error!");
		exit(1);
	}
	
	//Clear sendline var
	clearline(sendline);
	
	//Receive confirmation
	int temp, conf;
	if ((temp=(recv(s, &conf, sizeof(conf), 0))) == -1)    {
		perror("client recieved error");
		exit(1);
	}
	if(DEBUG==1) printf("number recieved: %i\n", conf);
	
	//Clear recline var
	clearline(recline);
	
	if(conf==-1) {
		printf("File does not exist on server. \n");
		return 0;	
	}
	else if(conf!=1){
		printf("Error, unexpected value.\n");
		return 1;
	}
	else{
		char yn[4];
		printf("Are you sure you want to delete this file? (Yes/No): ");
		scanf("%s", yn);
		if(strcmp(yn, "Yes")==0){
			//Send the confirmation
			sprintf(sendline, "%s", yn);
			if(send(s, sendline, strlen(sendline), 0)==-1){
				perror("client send error!");
				exit(1);
			}
			//Clear sendline var
			clearline(sendline);
			
			int temp2, conf2;
			if ((temp2=(recv(s, &conf2, sizeof(conf2), 0))) == -1)    {
				perror("client recieved error");
				exit(1);
			}
			if(DEBUG==1) printf("number recieved: %i\n", conf2);
			
			if(conf2==1) printf("File successfully deleted.\n");
			else if(conf2==-1) printf("File NOT successfully deleted.\n");
			else printf("Error: unexpected response from server.\n");
		}
		else if(strcmp(yn, "No")==0){
			printf("Ok then.\n");
			
			//Send the confirmation
			sprintf(sendline, "%s", yn);
			if(send(s, sendline, strlen(sendline), 0)==-1){
				perror("client send error!");
				exit(1);
			}
			//Clear sendline var
			clearline(sendline);
			
			return 0;
		}
		else{
			printf("Invalid response. \n");
			return 0;
		}
	}
	
	return 0;
}

int opLIS(int s){
	clearline(sendline);
	clearline(recline);

	int num_files, temp;
	if ((temp=(recv(s, &num_files, sizeof(num_files), 0))) == -1)    {
		perror("client recieved error");
		exit(1);
	}
	if(DEBUG==1) printf("number recieved: %i\n", num_files);
	
	int i;
	for(i=0; i<num_files; i++){
		clearline(recline);
		clearline(sendline);
		if ((recv(s, recline, sizeof(recline), 0)) == -1)    {
			perror("error receiving filename from server\n");
			exit(1);
		}
		else{
			printf("filename %i: %s \n", i, recline);
		}
		strcpy(sendline, recline);
		if(send(s, sendline, strlen(sendline), 0) == -1){
			perror("error sending confirmation");
			exit(1);
		}
	}
	
	
	return 0;
}

void clearline(char *line){
	memset(line,0,strlen(line));
	bzero((char*)&line, sizeof(line));
}
