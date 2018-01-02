#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

struct job {
  unsigned int jid;  // job ID
  char** args;
  int numArgs;
  int pid;  // process ID
  struct job* next;
};

int bckgdjobs = 0;
int totaljobs = 0;
int jidcount = 1;
int batchmode = 0;

int freeTheList(struct job* firstJob) {
  struct job* fr = firstJob;
  struct job* fr2;
  while (fr != NULL) {
    int i;
    for (i = 0; i < fr->numArgs; i++) {
      free(fr->args[i]);
    }
    free(fr->args);
    fr2 = fr->next;
    free(fr);
    fr = fr2;
  }
  return 0;
}

int thirtyTwoBacks(struct job* firstJob) {
  struct job* trav = firstJob;
  pid_t killval;
  int count = 0;

  while (trav != NULL) {
    killval = waitpid(trav->pid, 0, WNOHANG);
    if (killval == 0) {
      count++;
    }
    trav = trav->next;
  }
  if (count < 32) {
    return 1;
  } else {
    return 0;
  }
}
int
main(int argc, char* argv[]) {
  char buffer[514];
  buffer[513] = '\0';

  if (argc == 2) {
    char* batchfile = NULL;
    batchfile = strdup(argv[1]);
    close(STDIN_FILENO);

    // try to open file, see if it exists
    int fd = open(batchfile, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "Error: Cannot open file %s\n", batchfile);
      fflush(stderr);
      exit(1);
    }
    batchmode = 1;
    free(batchfile);
  } else if (argc == 1) {
    //  do some stuff, this is when we do interactive mode
  } else {
    char* errmsg = "Usage: mysh [batchFile]\n\0";
    write(2, errmsg, strlen(errmsg));
    //  fprintf(stderr, "Usage: mysh [batchFile]");
    exit(1);
  }

  //  SET UP FIRST JOB
  struct job * firstJob = NULL;
  while (1) {
    if (batchmode == 0) {
      printf("mysh> ");
      fflush(stdout);
    }

    char** arguments = (char**)malloc(256 * sizeof(char *));
    {
      if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        fflush(stdin);
        if (batchmode == 1) {
          fflush(stdout);
            int bytes = write(STDOUT_FILENO, buffer, strlen(buffer));
            if (bytes != strlen(buffer)) {
              free(arguments);
              freeTheList(firstJob);
              exit(1);
            }
            if (strlen(buffer) > 513)
             continue;
            fflush(stdout);
          }
        int tokens = 0;  //  NUMBER OF ARGUMENTS
        char* token = NULL;

        token = strtok(buffer, " \t\n");

        int i = 0;  //  array index counter
        while (token != NULL) {
          tokens++;
          arguments[i] = token;
          //  printf("arg #%d is: %s\n", i, arguments[i]);
          token = strtok(NULL, " \t\n");
          i++;
        }
        int justAmp = 0;
        int background = 0;

        if ((tokens > 0) &&
            (arguments[tokens-1][strlen(arguments[tokens-1])-1] == '&')) {
          if ((tokens == 1) && (strcmp(arguments[0], "&") == 0)) {
            justAmp = 1;
          }
          if (strlen(arguments[tokens-1]) != 1) {
            arguments[tokens-1][strlen(arguments[tokens-1])-1] = '\0';
            background = 1;
          }

          // will turn this off when you do a built in command
        }
        arguments[tokens] = NULL;
        char* EXIT = "exit";
        char* j = "j";
        char* myw = "myw";
        char* mywAmp = "myw&";
        if (justAmp == 1) {
          // Nothing
          free(arguments);
        } else if (tokens == 0) {
          free(arguments);
        } else if ((tokens == 1 && (strcmp(arguments[0], EXIT) == 0))
            ||(tokens == 2 && (strcmp(arguments[0], EXIT) == 0) &&
              strcmp(arguments[1], "&") == 0) || ((tokens == 1) &&
              (strcmp(arguments[0], "exit&") == 0))) {
          background = 0;
          free(arguments);
          freeTheList(firstJob);
          exit(0);
        } else if ((tokens == 1 && (strcmp(arguments[0], j) == 0))
            || (tokens == 2 && (strcmp(arguments[0], j) == 0) &&
              (strcmp(arguments[1], "&") == 0)) || ((tokens == 1)
                && (strcmp(arguments[0], "j&") == 0))) {
          background = 0;
          struct job* trav = firstJob;
          pid_t killval;

          while (trav != NULL) {
            killval = waitpid(trav->pid, 0, WNOHANG);
            if (killval == 0) {
              printf("%d :", trav->jid);
              fflush(stdout);
              int x = trav->numArgs;
              for (x = 0; x < (trav->numArgs); x++) {
                printf(" %s", (trav->args[x]));
                fflush(stdout);
              }
              printf("\n");
              fflush(stdout);
            }
            trav = trav->next;
          }
          free(arguments);
        } else if (((strcmp(arguments[0], myw) == 0) && tokens == 2)
            || (tokens == 3 && (strcmp(arguments[0], myw) == 0) &&
              strcmp(arguments[2], "&") == 0) ||
            ((strcmp(arguments[0], mywAmp) == 0) && (tokens == 2))) {
          background = 0;
          int jobID = atoi(arguments[1]);
          int processID;
          processID = 0;
          struct job * traverse = firstJob;
          while (traverse != NULL) {
            if (traverse->jid == jobID) {
              processID = traverse->pid;
              break;
            }
            traverse = traverse->next;
          }
          if (processID != 0) {
            struct timeval tv;
            time_t before;
            unsigned int timetocomplete = 0;

            gettimeofday(&tv, NULL);
            before = (tv.tv_usec) + (tv.tv_sec*1000000);
            int returnval = waitpid(traverse->pid, NULL, 0);
            if (returnval != -1) {
              gettimeofday(&tv, NULL);
              time_t after = (tv.tv_usec) + (tv.tv_sec*1000000);
              timetocomplete = after - before;
            }
            printf("%u : Job %d terminated\n", timetocomplete, traverse->jid);
            fflush(stdout);
            /*
            // Now remove this job from the list of jobs
            struct job * newTrav = firstJob;
            if (newTrav == traverse) {
            if (newTrav->next != NULL) {
            } else {
            firstJob = firstJob->next;
            firstJob = firstJob->next;
            free(traverse);
            }*/
          } else {
            fprintf(stderr, "Invalid jid %d\n", jobID);
            fflush(stderr);
          }
          free(arguments);
        } else {
          if ((tokens > 1) && (strcmp(arguments[tokens - 1], "&") == 0)) {
            arguments[tokens-1] = NULL;
            tokens--;
            background = 1;
          }
          /*if (tokens != 2 && (strcmp(arguments[0], myw) == 0)) {
            free(arguments);
            continue;
          }*/

          int rc = fork();

          if (rc == 0) {
            char* execErr = ": Command not found\n";
            fflush(stdout);
            execvp(arguments[0], arguments);
            write(STDERR_FILENO, arguments[0], strlen(arguments[0]));
            write(STDERR_FILENO, execErr, strlen(execErr));
            free(arguments);
            freeTheList(firstJob);
            exit(1);
          } else if (rc > 0) {
            // parent

            if (background == 0) {
              (void)wait(NULL);
            }
            if (background && thirtyTwoBacks(firstJob)) {
              bckgdjobs++;
              struct job* newJob = malloc(sizeof(struct job));
              newJob->args = (char**)malloc(256 * sizeof(char*));
              newJob->pid = rc;
              // this is the first job right now
              if (firstJob == NULL) {
                firstJob = newJob;
                firstJob->jid = jidcount;
                jidcount++;
                // extract arguments
                int a;
                for (a=0; a < tokens; a++) {
                  firstJob->args[a] = (char*)malloc(((strlen(arguments[a]) + 1)
                        * sizeof(char)));
                  strcpy(firstJob->args[a], arguments[a]);
                }
                firstJob->numArgs = tokens;
                firstJob->next = NULL;
                totaljobs++;
              } else {  // this is not the first job
                struct job* current = firstJob;

                while (current->next != NULL) {
                  current = current->next;
                }
                newJob->jid = jidcount;
                jidcount++;
                // extract arguments
                int a;
                for (a=0; a < tokens; a++) {
                  newJob->args[a] = (char*)malloc(((strlen(arguments[a])+1)
                        * sizeof(char)));
                  strcpy(newJob->args[a], arguments[a]);
                }
                newJob->numArgs = tokens;
                newJob->next = NULL;
                current->next = newJob;
                totaljobs++;
              }
            } else {
              if (background == 0 && batchmode == 1) {
                (void)waitpid(rc, NULL, 0);
              }
              jidcount++;
            }

          } else {
          }
          free(arguments);
        }
      } else {
        // END OF FILE
        free(arguments);
        freeTheList(firstJob);
        exit(0);
      }
    }
  }
  freeTheList(firstJob);
  return 0;
}
