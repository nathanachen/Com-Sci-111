#define _POSIX_C_SOURCE 200809L
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/wait.h>

char charbuf[1024];
int debugOrNot = 0;
int shellOrNot = 0;
int par_to_ch[2] = {0, 0};
int ch_to_par[2] = {0, 0};

struct termios tmpOG;

void handl()
{
  fprintf(stderr, "SIGPIPE received \n");
}

void resetMode()
{
  if (debugOrNot == 1)
  {
    fprintf(stderr, "DEBUG msg: resetting terminal changes \n");
  }
  if(tcsetattr(STDIN_FILENO, TCSANOW, &tmpOG) < 0)
  {
        fprintf(stderr, "Could not set the attributes\n");
        exit(1);
  }
}

void setupTerm()
{
    struct termios tmp;
    struct termios *temp_ptr;
    temp_ptr = &tmp;
    
    // get the original terminal attributes
    int tcgetResult = tcgetattr(STDIN_FILENO, temp_ptr);
    tcgetattr(STDIN_FILENO, &tmpOG);
    if (tcgetResult < 0)
    {
      fprintf(stderr, "ERROR: cannot retrieve terminal attributes.\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
        
    // prepare the termios struct to be used in setting new attributes
    temp_ptr->c_iflag = ISTRIP; /* only lower 7 bits	*/
    temp_ptr->c_oflag = 0; /* no processing	*/
    temp_ptr->c_lflag = 0; /* no processing	*/
    // put the keyboard into character-at-a-time, no-echo mode
    int tcsetResult = tcsetattr(STDIN_FILENO, TCSANOW, temp_ptr);
    
    if (tcsetResult < 0)
    {
      fprintf(stderr, "ERROR: cannot get attributes.\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
    }
}

void shellOptionChosen()
{
  ////////////////////////////////////////3333333333////////////////////////////////////
  ////////////////////////////////////////3333333333////////////////////////////////////
  ////////////////////////////////////////3333333333////////////////////////////////////
  // setup the pipes before fork
  // "You will need two pipes, one for each direction of communication"
  int pidch;
  signal(SIGPIPE, handl);
  if (pipe(par_to_ch) == -1)
  {
    fprintf(stderr, "ERROR: Unable to pipe\n");
    fprintf(stderr, "Details: %s\n", strerror(errno));
    exit(1);
  }
  if (pipe(ch_to_par) == -1)
  {
    fprintf(stderr, "ERROR: Unable to pipe\n");
    fprintf(stderr, "Details: %s\n", strerror(errno));
    exit(1);
  }
  // fork the current process
  pidch = fork();
  if (pidch < 0) // error
  {
    fprintf(stderr, "ERROR: Unable to fork\n");
    fprintf(stderr, "Details: %s\n", strerror(errno));
    exit(1);
  }
  else if (pidch == 0) // child
  {
    // setup the pipes after fork
    int closer = close(par_to_ch[1]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    closer = close(ch_to_par[0]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    int dupper = dup2(par_to_ch[0], STDIN_FILENO);
    if (dupper < 0)
    {
      fprintf(stderr, "ERROR: Unable to dup for stdin \n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    dupper = dup2(ch_to_par[1], STDOUT_FILENO);
    if (dupper < 0)
    {
      fprintf(stderr, "ERROR: Unable to dup for stdout \n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    dupper = dup2(ch_to_par[1], STDERR_FILENO);
    if (dupper < 0)
    {
      fprintf(stderr, "ERROR: Unable to dup for stderr \n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    
    closer = close(par_to_ch[0]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    closer = close(ch_to_par[1]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    
    // setup the exec'd shell's stdin/stdout/stderr
    char * execArg1 = "/bin/bash";
    char * execArgList[2] = {execArg1, NULL};
    int execResult = execvp("/bin/bash", execArgList);
    if (execResult < 0)
    {
      fprintf(stderr, "ERROR: Unable to perform exec syscall\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
  }
  else if (pidch > 0) // parent
  {
    struct pollfd pollList[2];
    
    // setup pipes after fork
    int closer = close(par_to_ch[0]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    closer = close(ch_to_par[1]);
    if (closer < 0)
    {
      fprintf(stderr, "ERROR: Unable to close\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    
    // KEYBOARD
    pollList[0].fd = STDIN_FILENO; pollList[0].events = POLLHUP | POLLERR | POLLIN;
    // SHELL
    pollList[1].fd = ch_to_par[0]; pollList[1].events = POLLHUP | POLLERR | POLLIN;
    
    while (true)
    {
      /* parameters:
         # of items 2
         block 0 seconds to wait for fd to be ready */
      int pollResult = poll(pollList, 2, 0);
      if (pollResult < 0)
      {
        fprintf(stderr, "Unable to use poll function \n");
        fprintf(stderr, "Details: %s\n", strerror(errno));
        exit(1);
      }
      else if (pollResult > 0)
      {
        if (POLLIN & pollList[0].revents)
        {
          int count;
          ssize_t count2;
          if ((count2 = read(0, charbuf, 1024)) < 0)
          {
            fprintf(stderr, "Unable to use poll function \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
          int pollResult2;
          for (count = 0; count < count2; count++)
          {
            int stat;
            char temp = charbuf[count];
            char carr[2] = {'\r', '\n'};
            if (temp == 0x04)
            {
              if (debugOrNot == 1)
              {
                fprintf(stderr, "DEBUG msg: read an EOT (terminal) \n");
              }
              pollResult2 = close(par_to_ch[1]);
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to close pipe \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
              int pollResult2 = waitpid(pidch, &stat, 0);
              if(pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to use waitpid \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
              fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \n", WTERMSIG(stat), WEXITSTATUS(stat));
              exit(0);
            }
            else if (temp == 0x03)
            {
              if (debugOrNot == 1)
              {
                fprintf(stderr, "DEBUG msg: read an ETX (terminal) \n");
              }
              pollResult2 = kill(pidch, SIGINT);
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to kill \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
            }
            else if (temp == '\r' || temp == '\n')
            {
              if (debugOrNot == 1)
              {
                fprintf(stderr, "DEBUG msg: read a carriage return or line feed (terminal) \n");
              }
              pollResult2 = write(STDOUT_FILENO, &carr[0], 2*sizeof(char));
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
              pollResult2 = write(STDOUT_FILENO, &carr[1], sizeof(char));
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
              pollResult2 = write(par_to_ch[1], &carr[1], sizeof(char));
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
            }
            else
            {
              pollResult2 = write(1, &temp, sizeof(char));
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
              pollResult2 = write(par_to_ch[1], &charbuf, sizeof(char));
              if (pollResult2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
            }
          }
        }
        if ((POLLERR | POLLHUP) & pollList[0].revents)//
        {
          int stat;
          int num2 = kill(pidch, SIGINT);
          if(num2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to kill \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
          int pollResult2 = waitpid(pidch, &stat, 0);
          if(pollResult2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to use waitpid \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
          fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \n", WTERMSIG(stat), WEXITSTATUS(stat));
          pollResult2 = close(ch_to_par[0]);
          if (pollResult2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to write \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
        }
        if (POLLIN & pollList[1].revents)
        {
          ssize_t pollResult2 = read(pollList[1].fd, charbuf, 1024);
          int temp2;
          char tempCharBuf[2];
          if (pollResult2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to read from in \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
          int count;
          for(count = 0; count < pollResult2; count++)
          {
            char temp = charbuf[count];
            if (temp == '\r' || temp == '\n')
            {
              tempCharBuf[1] = '\n';
              tempCharBuf[0] = '\r';
              temp2 = write(1, &tempCharBuf, 2*sizeof(char));
              if (temp2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write from in \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
            }
            else
            {
              temp2 = write(1, &temp, sizeof(char));
              if(temp2 < 0)
              {
                fprintf(stderr, "ERROR: Unable to write from in \n");
                fprintf(stderr, "Details: %s\n", strerror(errno));
                exit(1);
              }
            }
          }
          if (pollResult2 == 0)
          {
            pid_t waiter;
            int stat;
            if ((waiter = waitpid(pidch, &stat, 0)) < 0)
            {
              fprintf(stderr, "ERROR: Unable to use waitpid \n");
              fprintf(stderr, "Details: %s\n", strerror(errno));
            }
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \n", WTERMSIG(stat), WEXITSTATUS(stat));
          }
        }
        if ((POLLERR | POLLHUP) & pollList[1].revents)//
        {
          int stat;
          
          int pollResult2 = waitpid(pidch, &stat, 0);
          if(pollResult2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to use waitpid \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
          fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \n", WTERMSIG(stat), WEXITSTATUS(stat));
          pollResult2 = close(ch_to_par[0]);
          if (pollResult2 < 0)
          {
            fprintf(stderr, "ERROR: Unable to write \n");
            fprintf(stderr, "Details: %s\n", strerror(errno));
            exit(1);
          }
        }
      }
    }
  }
}

int main(int argc, char* argv[])
{
  int um;
  // array of structs that hold long options that we can use for this function on the command line
  struct option options[] = { {"shell", 0, NULL, 's'}, {"debug", 0, NULL, 'd'}, {0, 0, 0, 0} };
  // parse the command line until there are no more options to parse
  while ((um = getopt_long(argc, argv, "", options, NULL)) != -1)
  {
    if (um == 'd')
    {
      debugOrNot = 1;
    }
    else if (um == 's') // --shell option
    {
      shellOrNot = 1;
    }
    else
    {
      fprintf(stderr, "ERROR: Incorrect option usage. Use --debug and/or -shell \n");
      exit(1);
    }
  }
  setupTerm();
  atexit(resetMode);
  if (shellOrNot == 1)
  {
    shellOptionChosen();
  }
  else if (shellOrNot == 0)
  {
    ////////////////////////////////////////2222222222////////////////////////////////////
    ////////////////////////////////////////2222222222////////////////////////////////////
    ////////////////////////////////////////2222222222////////////////////////////////////
    
    size_t num = sizeof(char);
    char temp3; // char buffer to read/write into/from
    int readResult;
    int rnWriteResult;
    char rn[2] = {'\r', '\n'};
    while ((readResult = read(0, &temp3, num)) > 0)
    {
      // replace a carriage return or line feed with BOTH <cr> and <lf>
      if ((temp3 == '\n') || (temp3 == '\r'))
      {
        if (debugOrNot == 1)
        {
          fprintf(stderr, "DEBUG msg: read a carriage return or line feed");
        }
        if ((rnWriteResult = write(1, &rn, 2*num)) < 0)
        {
          fprintf(stderr, "ERROR: unable to write to stdout.\n");
          fprintf(stderr, "Details: %s\n", strerror(errno));
          exit(1);
        }
      }
      else if (temp3 == 0x04) // ^D is EOT, ASCII 0x04
      {
        if (debugOrNot == 1)
        {
          fprintf(stderr, "DEBUG msg: read an EOT");
        }
        exit(0);
      }
      // write back out the character that was read in
      else 
      {
        if (debugOrNot == 1)
        {
          fprintf(stderr, "DEBUG msg: read a regular character");
        }
        if ((write(1, &temp3, num) < 0))
        {
          fprintf(stderr, "ERROR: Unable to write to output\n");
          fprintf(stderr, "Details: %s\n", strerror(errno));
          exit(1);
        }
      }
    }
    if (readResult < 0)
    {
      fprintf(stderr, "ERROR: unable to read from stdin.\n");
      fprintf(stderr, "Details: %s\n", strerror(errno));
      exit(1);
    }
    return 0;
  }
}
