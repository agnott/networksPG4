#include<stdlib.h>
#include<stdio.h>
#include<dirent.h>
int main(){
  DIR *d;
  struct dirent *dir;
  int dirSize = 0;
  d = opendir(".");
  
  if (d){
    
    while ( (dir = readdir(d)) != NULL){
    	if( strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name, "..") != 0 ){
		    printf("%s\n", dir->d_name);
		    dirSize++;
      }
    }

    closedir(d);
  }

	printf("Directory Size: %i\n", dirSize);

  return(0);
}
