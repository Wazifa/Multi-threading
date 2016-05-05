/*
 * Name: Wazifa Jafar
 * Description: A program that will read a text file and return the number of
 * 		times that the user's input appears in the text file and 
 * 		the time it takes to find them.
 * 		It will also replace word1 with word2 both of which are provided by user.
 * 		To do this, it use shared memory, multiple processes and threads. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>


struct thread_data
{
  char *data;
  int start_address;
  int end_address;
  int word_length;		//length of word looking for
  char* temp;			//word looking for
  char *replace;		// word to replace temp with 
 
};

int count = 0;
pthread_mutex_t count_mutex;


/*Funcrion: countWords
 * Parameter : a pointer to a struct
 * Does not return anything
 * Description: Will search for the words from the memory that matches the user's provided input.
 * 		Will return the count of the number of matches found if user input is search.
 *     		Else, will replace the words if the user inputs the command replace.
 */

void *countWords (void *arg)
{
  struct thread_data *my_data;
  my_data = (struct thread_data *)arg;
  int x = my_data->start_address;
  int y = my_data->end_address;
  int length = my_data->word_length;
  char *word = my_data->temp;
  

  for (x; x<y;  x++)
  {
    if(!memcmp((my_data->data)+x, word, length))
    {
      pthread_mutex_lock(&count_mutex);
      count++;
      if (my_data->replace != NULL)
      {
        memcpy((my_data->data)+x, my_data->replace, strlen(my_data->replace));
      }
      pthread_mutex_unlock(&count_mutex);
    }
  }
  
}

int main( void ) 
{
  char input[255], **arr, *filename = "shakespeare.txt";
  char *data;
  int fd;
  void *status;
  
  struct stat buf; 
  struct timeval begin;
  struct timeval end;
  //signal (SIGINT, SIG_IGN);
  //signal(SIGTSTP, SIG_IGN);

/*
 * The program will copy the text file into shared memory and the threads will use the shared
 * memory to find the number of words from a different function.
 */
 
  
  fd = open("shakespeare.txt", O_RDWR);
  stat("shakespeare.txt", &buf);
    
  data = mmap((caddr_t)0, buf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0) ;

  printf("Welcome to the Shakespeare word count service.\n");
  printf("Enter: search [word] [workers] to start your search.\n");
/*
 * The program will first check for help, reset or quit and if none it will go into the search branch
 * of if-else.
*/

  while(1)
  {
    printf("> ");

    arr = malloc(5*sizeof(char*));
    char *temp;
    int i = -1;

    fgets(input, 255, stdin);

    temp = strtok(input, " \r\n\0");

    if (temp == NULL) continue;

    arr[i] = malloc(strlen(temp));
    arr[i] = temp;
    i++;

    while(temp != NULL)
    {
      arr[i] = malloc(strlen(temp));
      arr[i] = temp;
      temp = strtok(NULL, " \r\n\0");
      i++;
    }

    if(strcmp(arr[0], "quit") == 0)
    {
      exit(0);
    }

    // reset the original text file with the backup
    else if (strcmp(arr[0], "reset")== 0)
    {
      
      pid_t pid; 
      if ((pid = fork()) == 0)
      {
      int execReturn = execl("/bin/cp", "-i", "-p", "shakespeare_backup.txt", "shakespeare.txt", (char*)0);
  
        if(execReturn == -1)
        {
          printf("error\n");
        }
        if(munmap(data,buf.st_size) == -1)
        {
          perror("Error unmapping\n");
        }
      }

      int fd;
      fd = open("shakespeare.txt", O_RDWR);
      stat("shakespeare.txt", &buf);    
      data = mmap((caddr_t)0, buf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);                                                
    }
   
    else if (strcmp(arr[0], "help") == 0)
    {
      printf("Shakespeare Word Search Service Command Help\n");
      printf("_____________________________________________\n");
      printf("help    - displays this message.\n");
      printf("quit    - exits\n");
      printf("search [word] [workers] - searches the works of Shakespeare for [work] using [workers].");
      printf("  [workers] can be from 1 to 100.\n");
      printf("replace [word 1] [word 2] [workeres] - search the works of Shakespeare for [word 1] ");
      printf("using [workers] and replaces each instance with [word 2]. [workers] can be from 1 to 100.");
      printf("\nreset - will reset the memory mappped file back to its original state.\n");
    }
    
/*
 * This branch will divide the work (using pthread) into the provided number of workers and count 
 * the number of files from the text file.
 * the output will be printed to the user along with the time it took to find the number of instances.
 * The function countWords will be called to do this which will also handle a case where a user wants to replace a word.
 */

    else if (strcmp(arr[0], "search") == 0 || strcmp(arr[0], "replace") == 0)
    {
      int workers = 0;
      if (strcmp(arr[0], "search")== 0)
      {
        workers = atoi(arr[2]);
      }
    
      else 
      {
        workers = atoi(arr[3]); 
      } 

      int str_worker = 0;
  
      pthread_t thread[workers];
      
      gettimeofday(&begin, NULL);
      for (str_worker = 0; str_worker < workers; str_worker++)
      { 

        struct thread_data *thread_array = malloc( sizeof( struct thread_data ) );
        thread_array->start_address = (str_worker)/(float)workers*buf.st_size;

        thread_array->end_address = (str_worker+1)/(float)workers*buf.st_size; 
        thread_array->word_length= strlen(arr[1]);
        thread_array->temp = malloc (sizeof(arr[1]));
	strcpy(thread_array->temp, arr[1]);
        thread_array->data = data;	
        
        if (strcmp(arr[0], "replace") == 0)
        {
         
          if (strlen(arr[1]) > strlen(arr[2]))
          {
            int left = strlen(arr[1]) - strlen(arr[2]), i = 0;
            for (i = 0; i<left; i++)
            {
              strncat(arr[2], " ", 1);
            }
          }
          thread_array->replace = malloc(sizeof(arr[2]));
          strncat(arr[2], "\0", 1);
          strcpy(thread_array->replace, arr[2]);
        }
      
        pthread_create(&thread[str_worker],0, countWords, thread_array);
  
        int val;
        pthread_join( thread[str_worker], (void*)&val );
      }

      gettimeofday(&end, NULL);
      float elapsed_time = (end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec- begin.tv_usec);

      if (strcmp(arr[0], "search")==0)
      {
        printf("Found %d instances of %s in %f microseconds\n", count, arr[1], elapsed_time); 
      }

      count = 0;
    }

    else
    {
      printf("Command not found\n");
    }
 
  }
  close(fd);
  pthread_exit(NULL);
  return 0;
}
