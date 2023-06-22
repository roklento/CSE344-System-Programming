#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#define PATH_MAX 4096
#define NAME_MAX 255

typedef struct {
    int sourceFd;
    int destinationFd;
    char* fileName;
} BufferItem;

typedef struct {
    BufferItem* buffer;
    int bufferSize;
    int count;
    int in;
    int out;
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
    int done;
} Buffer;

typedef struct {
    pthread_t* threads;
    int threadCount;
    pthread_mutex_t mutex;
} ThreadPool;

typedef struct {
    char* source;
    char* destination;
} FilePaths;

typedef struct {
    FilePaths* paths;
    Buffer* buffer;
} ProducerArgs;

typedef struct {
    long filesCopied;
    long bytesCopied;
    long regularFilesCopied;
    long directoryFilesCopied;
} Statistics;

volatile sig_atomic_t flag = 0;

Statistics stats = {0, 0, 0, 0};

void sigintHandler(int sig_num) 
{ 
    flag = 1;
} 

Buffer* createBuffer(int bufferSize) {
    Buffer* buffer = malloc(sizeof(Buffer));
    buffer->buffer = malloc(sizeof(BufferItem) * bufferSize);
    buffer->bufferSize = bufferSize;
    buffer->count = 0;
    buffer->in = 0;
    buffer->out = 0;
    buffer->done = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->notEmpty, NULL);
    pthread_cond_init(&buffer->notFull, NULL);
    return buffer;
}

void destroyBuffer(Buffer* buffer) {
    free(buffer->buffer);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->notEmpty);
    pthread_cond_destroy(&buffer->notFull);
    free(buffer);
}

ThreadPool* createThreadPool(int threadCount) {
    ThreadPool* pool = malloc(sizeof(ThreadPool));
    pool->threads = malloc(sizeof(pthread_t) * threadCount);
    pool->threadCount = threadCount;
    pthread_mutex_init(&pool->mutex, NULL);
    return pool;
}

void startThreads(ThreadPool* pool, void* (*threadFunction)(void*), void* arg) {
    for (int i = 0; i < pool->threadCount; i++) {
        pthread_create(&pool->threads[i], NULL, threadFunction, arg);
    }
}

void waitForThreads(ThreadPool* pool) {
    for (int i = 0; i < pool->threadCount; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

void destroyThreadPool(ThreadPool* pool) {
    free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

void handleFile(const char* sourcePath, const char* destinationPath, Buffer* buffer) {
    int sourceFd = open(sourcePath, O_RDONLY);
    if (sourceFd == -1) {
        perror("Failed to open source file");
        return;
    }
    int destinationFd = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (destinationFd == -1) {
        perror("Failed to open destination file");
        close(sourceFd);
        return;
    }
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == buffer->bufferSize) {
        pthread_cond_wait(&buffer->notFull, &buffer->mutex);
    }
    BufferItem item = {sourceFd, destinationFd};
    item.fileName = malloc(strlen(sourcePath) + 1);  
    strncpy(item.fileName, sourcePath, sizeof(item.fileName)); 
    buffer->buffer[buffer->in] = item;
    buffer->in = (buffer->in + 1) % buffer->bufferSize;
    buffer->count++;

    pthread_cond_signal(&buffer->notEmpty);
    pthread_mutex_unlock(&buffer->mutex);

    struct stat s;
    if (stat(sourcePath, &s) == 0) {
        stats.filesCopied++;
        stats.bytesCopied += s.st_size;
        stats.regularFilesCopied++;
    }
}

void handleDirectory(const char* sourceDir, const char* destDir, Buffer* buffer) {
    DIR* dir = opendir(sourceDir);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char sourcePath[PATH_MAX];
        snprintf(sourcePath, PATH_MAX, "%s/%s", sourceDir, entry->d_name);

        char destinationPath[PATH_MAX];
        snprintf(destinationPath, PATH_MAX, "%s/%s", destDir, entry->d_name);

        struct stat s;
        if (stat(sourcePath, &s) == 0 && S_ISDIR(s.st_mode)) {
            mkdir(destinationPath, s.st_mode);
            
            stats.directoryFilesCopied++;
            handleDirectory(sourcePath, destinationPath, buffer);
        } else {
            handleFile(sourcePath, destinationPath, buffer);
        }
    }
    closedir(dir);
}

void* producer(void* arg) {
    ProducerArgs* args = (ProducerArgs*)arg;
    FilePaths* paths = args->paths;
    Buffer* buffer = args->buffer;

    handleDirectory(paths->source, paths->destination, buffer);

    pthread_mutex_lock(&buffer->mutex);
    buffer->done = 1;
    pthread_cond_broadcast(&buffer->notEmpty);
    pthread_mutex_unlock(&buffer->mutex);

    return NULL;
}

void* consumer(void* arg) {
    Buffer* buffer = (Buffer*)arg;
    while (1) {
        pthread_mutex_lock(&buffer->mutex);
        while (buffer->count == 0 && !buffer->done) {
            pthread_cond_wait(&buffer->notEmpty, &buffer->mutex);
        }

        if (buffer->count == 0 && buffer->done) {
            pthread_mutex_unlock(&buffer->mutex);
            break;
        }
        BufferItem item = buffer->buffer[buffer->out];
        buffer->out = (buffer->out + 1) % buffer->bufferSize;
        buffer->count--;

        pthread_cond_signal(&buffer->notFull);
        pthread_mutex_unlock(&buffer->mutex);

        char copyBuffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(item.sourceFd, copyBuffer, sizeof(copyBuffer))) > 0) {
            if (write(item.destinationFd, copyBuffer, bytesRead) != bytesRead) {
                perror("Failed to write to destination file");
                break;
            }
        }
        close(item.sourceFd);
        close(item.destinationFd);;
    }

    return NULL;
}

void displayStatistics() {
    printf("Total files copied: %ld\n", stats.filesCopied);
    printf("Total bytes copied: %ld\n", stats.bytesCopied);
    printf("Regular files copied: %ld\n", stats.regularFilesCopied);
    printf("Directory files copied: %ld\n", stats.directoryFilesCopied);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sigintHandler);

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <buffer size> <number of threads> <source directory> <destination directory>\n", argv[0]);
        return 1;
    }

    int bufferSize = atoi(argv[1]);
    int threadCount = atoi(argv[2]);
    char* sourceDirectory = argv[3];
    char* destinationDirectory = argv[4];

    struct timeval start, end;
    gettimeofday(&start, NULL);
    Buffer* buffer = createBuffer(bufferSize);
    ThreadPool* pool = createThreadPool(threadCount);

    pthread_t producerThread;
    FilePaths paths = {sourceDirectory, destinationDirectory};

    ProducerArgs args;
    args.paths = &paths;
    args.buffer = buffer;
    pthread_create(&producerThread, NULL, producer, &args);
    startThreads(pool, consumer, buffer);
    
    while (!flag) {
        pthread_join(producerThread, NULL);
        waitForThreads(pool);
    }

    gettimeofday(&end, NULL);
    double timeTaken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time taken: %.6f seconds\n", timeTaken);
    displayStatistics();

    printf("CTRL+C was pressed. Program is exiting...\n");

    destroyBuffer(buffer);
    destroyThreadPool(pool);

    return 0;
}
