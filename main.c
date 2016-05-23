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

static unsigned int thread_cnt;
pthread_mutex_t thread_cnt_mutex;
static unsigned int thread_cnt_max;

void sig(int signum) {
  switch(signum) {
    case SIGINT:
      exit(0);
      break;
    default:
      printf("%d\n", signum);
  }
}

typedef struct {
  char *path;
  char *cmd;
  int cachefile_time;
  pthread_mutex_t file_mutex;
  pthread_cond_t done_cond;
} file_info_t;

typedef struct {
  file_info_t *file_info;
  FILE *fd;
} file_info_work_t;

void *do_fifo_work(void *arg) {
  file_info_work_t *file_info_work = (file_info_work_t *)arg;
  file_info_t *file_info = file_info_work->file_info;
  FILE *proc_fd = NULL;
  char rw_buf[BUFSIZ] = {0x0};
  size_t read_count = 0;
  size_t write_count = 0;
  int feof_status = -1;
  int ret = -1;

  pthread_mutex_lock(&file_info->file_mutex);
  PRINT_DEBUG("Executed process\n");
  proc_fd = popen(file_info->cmd, "r");

  feof_status = feof(proc_fd);
  while (feof_status == 0) {
    read_count = fread(rw_buf, 1, BUFSIZ, proc_fd);
    feof_status = feof(proc_fd);
    write_count = fwrite(rw_buf, 1, read_count, file_info_work->fd);
    //PRINT_DEBUG("r: %zu w: %zu EOF: %d\n", read_count, write_count,
    //  feof_status);
  }
  fflush(file_info_work->fd);
  //PRINT_DEBUG("Flushed output.\n");

  if (file_info_work->fd != NULL)
    fclose(file_info_work->fd);
  PRINT_DEBUG("Closed file1.\n");
  pclose(proc_fd);
  PRINT_DEBUG("Closed files.\n");

  // Let the master thread for this fifo know that we're done
  pthread_cond_signal(&file_info->done_cond);
  pthread_mutex_unlock(&file_info->file_mutex);

  pthread_exit(NULL);
}

void *do_fifo(void *arg) {
  file_info_t *file_info = (file_info_t *)arg;
  file_info_work_t *file_info_work;
  pthread_t *worker_threads;
  FILE *fifo_fd = NULL;
  int ret = -1;

  worker_threads = malloc(thread_cnt_max * sizeof(pthread_t));

  pthread_mutex_lock(&file_info->file_mutex);
  if ( access(file_info->path, F_OK) != -1) {
    PRINT_DEBUG("Removed existing file.\n");
    ret = unlink(file_info->path);
  }

  do {
    ret = mkfifo(file_info->path, S_IRWXU | S_IRWXG | S_IRWXO);
    PRINT_DEBUG("Created fifo.\n");
  
    printf("Ready to receive reads ...\n");
  
    // open should block until reader is ready
    fifo_fd = fopen(file_info->path, "w");

    unlink(file_info->path);
    PRINT_DEBUG("Deleted fifo.\n");
  
    // Wait for an available worker thread count
    pthread_mutex_lock(&thread_cnt_mutex);
    if (++thread_cnt > thread_cnt_max) {
      thread_cnt--;
      //pthread_cond_wait(&thread_cnt_avail, &thread_cnt_mutex);
      thread_cnt++;
    }
    pthread_mutex_unlock(&thread_cnt_mutex);
  
    file_info_work = malloc(sizeof(file_info_work_t));
    file_info_work->file_info = file_info;
    file_info_work->fd = fifo_fd;
    pthread_mutex_unlock(&file_info->file_mutex);
    pthread_create(&worker_threads[0], NULL, do_fifo_work,
      (void *)file_info_work);
    
    //Wait for thread to finish with file
    pthread_cond_wait(&file_info->done_cond, &file_info->file_mutex);
    if (file_info->cachefile_time > 0) {
      // Set timer to delete file and replace with new fifo
      //sleep(file_info->cachefile_time*1000);
      unlink(file_info->path);
    }
    pthread_mutex_unlock(&file_info->file_mutex);
    
    free(file_info_work);
  } while (1);

  ret = unlink(file_info->path);
  PRINT_DEBUG("Removed fifo.\n");

  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  pthread_t *worker_threads;
  file_info_t file_info;
  void *status;
  int ret = -1;

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

  pthread_mutex_lock(&thread_cnt_mutex);
  thread_cnt = 0;
  thread_cnt_max = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_mutex_unlock(&thread_cnt_mutex);

  worker_threads = malloc(thread_cnt_max * sizeof(pthread_t));

  file_info.path = "/tmp/fifo";
  //file_info.cmd = "date";
  file_info.cmd = "echo \"Hello World!\"";
  file_info.cachefile_time = 0;
  pthread_mutex_init(&file_info.file_mutex, NULL);
  pthread_cond_init(&file_info.done_cond, NULL);

  pthread_create(&worker_threads[0], NULL, do_fifo, (void *)&file_info);

  // Wait for all processes to end
  pthread_join(worker_threads[0], &status);

  pthread_cond_destroy(&file_info.done_cond);
  pthread_mutex_destroy(&file_info.file_mutex);

  pthread_mutex_destroy(&thread_cnt_mutex);

  pthread_exit(NULL);
  return 0;
}


/* vim: se ts=4:et */
