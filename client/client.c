
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
//#include <mhash.h>
#include <openssl/md5.h>
#define MAX_LINE 1024
#define DEBUG 1

char sendline[MAX_LINE];
char buf[10];
char recline[MAX_LINE];

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

	printf("Pick an operation: REQ, UPL, LIS, DEL, XIT\n");
	scanf("%s", buf);
	//prompt user for operation state
	while (strcmp(buf, "XIT") != 0 )	{
		if(send(s, buf, strlen(buf), 0)==-1){
			perror("client send error!");
			exit(1);
		}

		//REQ
		if (strcmp(buf, "REQ") == 0)	{	
			/* get length of file name and add it to filename string-- send
			 *          * the combination to server*/
			printf("Please enter a file to send:\n");
			scanf("%s", filename);
			if(DEBUG==1) printf("file being sent: %s\n", filename);
			int res = opREQ(s, filename);
			if(res!=0) printf("REQ Error: %d", res);

		} else if (strcmp(buf, "UPL") == 0) {
			if(DEBUG==1) printf("command: %s", buf);
			printf("Please enter a file to upload:\n");
			scanf("%s", filename);
			if(DEBUG==1) printf("file being upload: %s\n", filename);
			int res = opUPL(s, filename);
			if(res!=0) printf("REQ Error: %d", res);

		} else if (strcmp(buf, "DEL") == 0) {
			if(DEBUG==1) printf("command: %s", buf);
			
			printf("Please enter a file to upload:\n");
			scanf("%s", filename);

			int res = opDEL(s,filename);
			if(res!=0) printf("DEL Error: %d", res);

		} else if (strcmp(buf, "LIS") == 0) {
			if(DEBUG==1) printf("command: %s", buf);
			
			int res = opLIS(s);
			if(res!=0) printf("LIS Error: %d", res);
		}
		//Clear sendline var
		clearline(sendline);
		printf("Pick an operation: REQ, UPL, LIS, DEL, XIT\n");
		scanf("%s", buf);
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

	/* receive size of requested file from server. If negative value, error */
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
		if ((file_size - downloaded) >= MAX_LINE)       {
			buffer_len=MAX_LINE;
		}       else    {
			buffer_len=(file_size-downloaded);
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


	/*computes MD5 Hash value based on content received, compare to original MD5 -- use same steps as computing initial hash in server */
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];
	unsigned char c[MD5_DIGEST_LENGTH];

	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, 1024, file)) != 0)
		MD5_Update (&mdContext, data, bytes);
	MD5_Final (c,&mdContext);



	/*if MD5 values don't match, signal error and exit */
	int l;
	for (l=0; l<sizeof(c); l++)     {
		if (c[l]!=serverHash[l])        {
			printf("error: hashes do not match\n");
			exit(1);
		}
	}

	//Say that hash matches, print out information
	printf("Hash Matches\n");
	printf("%i bytes transferred in %5.2f seconds\n", file_size, ((end.tv_sec + end.tv_usec/1000000.)- (start.tv_sec + start.tv_usec/1000000.)));
	printf("Throughput: %5.3f Megabytes/sec\n", (file_size/1000000.)/((end.tv_sec+end.tv_usec/1000000.)- (start.tv_sec + start.tv_usec/1000000.)));
	printf("File MD5sum: ");
	int j;
	for(j = 0; j < MD5_DIGEST_LENGTH; j++) printf("%02x", c[j]);
	printf ("\n");
	
	return 0;
}

int opUPL(int s, char *filename){
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
	
	return 0;
}

int opDEL(int s, char *filename){
	clearline(sendline);
	clearline(recline);

	return 0;
}

int opLIS(int s){
	clearline(sendline);
	clearline(recline);
	//send LIS
	//rec char *, = size of directory as int
	// loop # items long
		// rec each item's name
		// print it
	
	sprintf(sendline, "LIS");
	if(send(s, sendline, strlen(sendline), 0)==-1){
		perror("client sending LIS command!");
		exit(1);
	}
	int num_files, temp;
	if ((temp=(recv(s, &num_files, sizeof(num_files), 0))) == -1)    {
		perror("client recieved error");
		exit(1);
	}
	
	int i;
	for(i=0; i<num_files; i++){
		if ((recv(s, recline, sizeof(recline), 0)) == -1)    {
			perror("error receiving filename from server\n");
			exit(1);
		}
		else{
			printf("filename %i: %s \n", i, recline);
		}
		clearline(recline);
	}
	
	
	return 0;
}

void clearline(char *line){
	memset(line,0,strlen(line));
	bzero((char*)&line, sizeof(line));
}
