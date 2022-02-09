#include <pthread.h>
#include <time.h>

#include "gtest/gtest.h"

extern "C" {
#include "circular_buffer.h"
}

struct CircularBufferThreadTest : testing::Test {

    int bufferSize = 599;
    cbuf_handle_t cbuf;

    CircularBufferThreadTest() { cbuf = circular_buf_init(bufferSize); }

    ~CircularBufferThreadTest() { circular_buf_free(cbuf); }
};

struct circular_buf_test_t {
    cbuf_handle_t cbuf;
    int idxStart;
    int numProc;
    int numLostData;
};

void *producer(void *pBufTestInput) {

    // Get the circular buffer
    circular_buf_test_t *pCbufTest = (circular_buf_test_t *)pBufTestInput;
    circular_buf_test_t cbufTest = *pCbufTest;

    // Sleep 0.01 sec
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 10000000;

    int idxMax = cbufTest.idxStart + cbufTest.numProc;
    int numLostData = 0;
    int idx;
    for (idx = cbufTest.idxStart; idx < idxMax; idx++) {
        commandStreamStructure_t data;
        data.counter = idx;
        if (circular_buf_put(cbufTest.cbuf, data)) {
            numLostData += 1;
        }

        if (nanosleep(&sleepTime, NULL) < 0) {
            printf("Nano sleep system call failed.\n");
            return 0;
        }
    }

    pCbufTest->numLostData = numLostData;

    printf("The producer thread is done.\n");
    return 0;
}

// Just over-fill the buffer by 1
TEST_F(CircularBufferThreadTest, circularBufPut) {

    struct circular_buf_test_t cbufTest;
    cbufTest.cbuf = cbuf;
    cbufTest.idxStart = 0;
    cbufTest.numProc = 300;

    // Create two new threads as the producers
    pthread_t threadId1, threadId2;

    int error = pthread_create(&threadId1, NULL, producer, (void *)&cbufTest);
    if (error) {
        printf("Thread 1 creation failed: %d.\n", error);
        exit(1);
    }

    error = pthread_create(&threadId2, NULL, producer, (void *)&cbufTest);
    if (error) {
        printf("Thread 2 creation failed: %d.\n", error);
        exit(1);
    }

    pthread_join(threadId1, NULL);
    pthread_join(threadId2, NULL);
    printf("Finish the waiting of producer threads.\n");

    EXPECT_EQ(bufferSize, circular_buf_size(cbuf));

    // Main thread as the consumer
    commandStreamStructure_t data;

    // The first data's counter should be 0
    circular_buf_get(cbuf, &data);
    EXPECT_EQ(0, data.counter);

    // The second data's counter should be 1 because of the overwrite
    circular_buf_get(cbuf, &data);
    EXPECT_EQ(1, data.counter);
}

// Each threads will over-fill the buffer size
TEST_F(CircularBufferThreadTest, circularBufPutOverfill) {

    struct circular_buf_test_t cbufTest1, cbufTest2;
    cbufTest1.cbuf = cbuf;
    cbufTest1.idxStart = 0;
    cbufTest1.numProc = 650;

    cbufTest2.cbuf = cbuf;
    cbufTest2.idxStart = 1000;
    cbufTest2.numProc = 700;

    // Create two new threads as the producers
    pthread_t threadId1, threadId2;

    int error = pthread_create(&threadId1, NULL, producer, (void *)&cbufTest1);
    if (error) {
        printf("Thread 1 creation failed: %d (test put over-fill).\n", error);
        exit(1);
    }

    error = pthread_create(&threadId2, NULL, producer, (void *)&cbufTest2);
    if (error) {
        printf("Thread 2 creation failed: %d (test put over-fill).\n", error);
        exit(1);
    }

    // Wait the producer threads
    pthread_join(threadId1, NULL);
    pthread_join(threadId2, NULL);
    printf("Finish the waiting of producer threads (test put over-fill).\n");

    // Check the lost data
    int numLostDataTotal = cbufTest1.numLostData + cbufTest2.numLostData;
    EXPECT_EQ(751, numLostDataTotal);

    // Main thread as the consumer
    EXPECT_EQ(bufferSize, circular_buf_size(cbuf));

    commandStreamStructure_t data;
    bool isBufEmpty;
    while (!(isBufEmpty = circular_buf_get(cbuf, &data))) {
        ;
    }

    EXPECT_EQ(1699, data.counter);
}

// The total number in two threads will over-fill the buffer size
TEST_F(CircularBufferThreadTest, circularBufGet) {

    int numProc = 500;

    struct circular_buf_test_t cbufTest;
    cbufTest.cbuf = cbuf;
    cbufTest.idxStart = 0;
    cbufTest.numProc = numProc;

    // Create two new threads as the producers
    pthread_t threadId1, threadId2;

    int error = pthread_create(&threadId1, NULL, producer, (void *)&cbufTest);
    if (error) {
        printf("Thread 1 creation failed: %d (test get).\n", error);
        exit(1);
    }

    error = pthread_create(&threadId2, NULL, producer, (void *)&cbufTest);
    if (error) {
        printf("Thread 2 creation failed: %d (test get).\n", error);
        exit(1);
    }

    // Main thread as the consumer

    // Sleep for 0.02 second first to make sure there is some data in buffer
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 20000000;
    if (nanosleep(&sleepTime, NULL) < 0) {
        printf("Nano sleep system call failed (test get).\n");
        exit(1);
    }

    int numGetData = 0;
    sleepTime.tv_nsec = 10000000;
    commandStreamStructure_t data;

    int indexMax = 2 * numProc;
    int index;
    for (index = 0; index < indexMax; index++) {
        if (circular_buf_get(cbuf, &data) == 0) {
            numGetData++;
        }

        if (nanosleep(&sleepTime, NULL) < 0) {
            printf("Nano sleep system call failed (test get).\n");
            exit(1);
        }
    }

    EXPECT_EQ(1000, numGetData);
    EXPECT_EQ(499, data.counter);

    // Wait the producer threads
    pthread_join(threadId1, NULL);
    pthread_join(threadId2, NULL);
    printf("Finish the waiting of producer threads (test get).\n");
}
