#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PRINT_DEBUG(...) if(DEBUG) fprintf(stderr, __VA_ARGS__)
#define DEBUG 1

pthread_mutex_t fifo_mutex;
static unsigned int thread_cnt;
pthread_mutex_t thread_cnt_mutex;

void sig(int signum) {
  switch(signum) {
    case SIGINT:
      exit(0);
      break;
    default:
      printf("%d\n", signum);
  }
}

char *fifo_path = "/tmp/fifo";
//char *process_cmd = "date";
char *process_cmd = "echo \"Hello World!\"";

void *do_fifo(void *arg) {
  FILE *fifo_fd = NULL;
  FILE *proc_fd = NULL;
  char rw_buf[BUFSIZ] = {0x0};
  size_t read_count = 0;
  size_t write_count = 0;
  int feof_status = -1;
  int ret = -1;

  // open should block until reader is ready
  fifo_fd = fopen(fifo_path, "w");

  do {
    PRINT_DEBUG("Executed process\n");
    proc_fd = popen(process_cmd, "r");

    ret = unlink(fifo_path);
    PRINT_DEBUG("Deleted fifo.\n");
    feof_status = feof(proc_fd);
    while (feof_status == 0) {
      read_count = fread(rw_buf, 1, BUFSIZ, proc_fd);
      feof_status = feof(proc_fd);
      write_count = fwrite(rw_buf, 1, read_count, fifo_fd);
      PRINT_DEBUG("r: %zu w: %zu EOF: %d\n", read_count, write_count,
        feof_status);
    }
    fflush(fifo_fd);
    PRINT_DEBUG("Flushed output.\n");

    if (fifo_fd != NULL)
      fclose(fifo_fd);
    PRINT_DEBUG("Closed file1.\n");
    pclose(proc_fd);
    PRINT_DEBUG("Closed files.\n");

    ret = mkfifo(fifo_path, S_IRWXU | S_IRWXG | S_IRWXO);
    PRINT_DEBUG("Created new fifo.\n");
    fifo_fd = fopen(fifo_path, "w");
    PRINT_DEBUG("fifo_fd: %d\n", fifo_fd == NULL);
  } while (fifo_fd != NULL);

  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  pthread_t *worker_threads;
  void *status;
  int ret = -1;

  thread_cnt = sysconf(_SC_NPROCESSORS_ONLN);
  worker_threads = malloc(thread_cnt * sizeof(pthread_t));

  signal(SIGHUP, sig);
  signal(SIGINT, sig);
  signal(SIGQUIT, sig);
  signal(SIGILL, sig);
  signal(SIGABRT, sig);
  signal(SIGFPE, sig);
  //signal(SIGKILL, sig);
  signal(SIGALRM, sig);
  signal(SIGTERM, sig);
  signal(SIGUSR1, sig);
  signal(SIGUSR2, sig);
  signal(SIGCONT, sig);
  //signal(SIGSTOP, sig);
  signal(SIGTSTP, sig);
  signal(SIGTTIN, sig);
  signal(SIGTTOU, sig);

  // Initialize pthread mutexes
  pthread_mutex_init(&thread_cnt_mutex, NULL);
  pthread_mutex_init(&fifo_mutex, NULL);

  pthread_mutex_lock(&fifo_mutex);
  if ( access(fifo_path, F_OK) != -1) {
    PRINT_DEBUG("Removed existing file.\n");
    ret = unlink(fifo_path);
  }
  ret = mkfifo(fifo_path, S_IRWXU | S_IRWXG | S_IRWXO);
  PRINT_DEBUG("Created fifo.\n");

  printf("Ready to receive reads ...\n");
  pthread_create(&worker_threads[0], NULL, do_fifo, NULL);

  pthread_join(worker_threads[0], &status);

  pthread_mutex_destroy(&thread_cnt_mutex);
  pthread_mutex_destroy(&fifo_mutex);

  ret = unlink(fifo_path);
  PRINT_DEBUG("Removed fifo.\n");

  pthread_exit(NULL);
  return 0;
}


/* vim: se ts=4:et */
