#include <fstream>
#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "logTlm.h"
}

struct DataThread {
    uint idx;
    double valueArray[2];
    double valueSingle;
};

struct LogTlmThreadTest : testing::Test {

    char *pathDir = "./";
    char *formatFilename = "tlmThread_%m_%d_%Y_%H_%M_%S.log";

    uint sizeBuffer = 1000;
    uint recordPerFile = 4000;

    struct DataThread data;

    LogTlmThreadTest() {
        logTlm_createBuffer(sizeBuffer, sizeof(DataThread));
        logTlm_open(pathDir, formatFilename, recordPerFile);
    }

    ~LogTlmThreadTest() {
        logTlm_closeThread();

        logTlm_close();
        logTlm_freeBuffer();
    }
};

TEST(LogTlmThread, logTlmRunFlushInNewThread) {

    int timeInMs = 0;
    EXPECT_EQ(-1, logTlm_runFlushInNewThread(&timeInMs));

    timeInMs = -1;
    EXPECT_EQ(-1, logTlm_runFlushInNewThread(&timeInMs));
}

static void *startWriter(void *pData) {

    // Loop delay time (100 ms)
    struct timespec sleepTime;
    sleepTime.tv_nsec = 1e8;
    sleepTime.tv_sec = 0;

    // Write the data
    struct DataThread data;
    bool isFlushing = false;
    uint idx;
    for (idx = 0; idx < 8001; idx++) {
        while (isFlushing) {
            nanosleep(&sleepTime, NULL);
            printf("Wait the flushing thread to do the job\n");

            isFlushing = logTlm_isFlushing();
        }

        data.idx = idx;
        data.valueArray[0] = 10 * idx + 0.1;
        data.valueArray[1] = 10 * idx + 0.2;
        data.valueSingle = 10 * idx + 0.3;
        if (logTlm_write(&data) == -1) {
            logTlm_flush(false);
            isFlushing = true;
        };
    }

    // Flush the final data
    logTlm_flush(false);

    return 0;
}

TEST_F(LogTlmThreadTest, logTlmRunFlushInNewThread) {
    int timeInMs = 500;
    int status = logTlm_runFlushInNewThread(&timeInMs);

    EXPECT_EQ(0, status);

    pthread_t threadWriter;
    int error = pthread_create(&threadWriter, NULL, startWriter, NULL);
    if (error != 0) {
        perror("Thread of writer creation failed");
        exit(1);
    }

    // Wait the thread
    pthread_join(threadWriter, NULL);
    printf("Finish the waiting of writer thread.\n");

    // Wait for some time to make sure all the data are flushed
    sleep(2);
    EXPECT_FALSE(logTlm_isFlushing());

    // Check the current data in buffer
    struct DataThread *datas = (struct DataThread *)logTlm_getBuffer();

    EXPECT_EQ(8000, datas[0].idx);
    EXPECT_DOUBLE_EQ(80000.1, datas[0].valueArray[0]);
    EXPECT_DOUBLE_EQ(80000.2, datas[0].valueArray[1]);
    EXPECT_DOUBLE_EQ(80000.3, datas[0].valueSingle);

    EXPECT_EQ(7001, datas[1].idx);
    EXPECT_DOUBLE_EQ(70010.1, datas[1].valueArray[0]);
    EXPECT_DOUBLE_EQ(70010.2, datas[1].valueArray[1]);
    EXPECT_DOUBLE_EQ(70010.3, datas[1].valueSingle);

    EXPECT_EQ(7002, datas[2].idx);
    EXPECT_DOUBLE_EQ(70020.1, datas[2].valueArray[0]);
    EXPECT_DOUBLE_EQ(70020.2, datas[2].valueArray[1]);
    EXPECT_DOUBLE_EQ(70020.3, datas[2].valueSingle);

    // Check the file is rotated or not
    std::string fileCurrent = logTlm_getFilename();
    std::ifstream fileOld0(fileCurrent + ".0", std::ifstream::binary);
    std::ifstream fileOld1(fileCurrent + ".1", std::ifstream::binary);
    EXPECT_TRUE(fileOld0.good());
    EXPECT_TRUE(fileOld1.good());

    // Check the data in file
    uint sizeFile = recordPerFile * sizeof(DataThread);
    char dataBinary[sizeFile];

    // log.0
    fileOld0.read(dataBinary, sizeFile);
    struct DataThread *dataDecode = (struct DataThread *)dataBinary;

    uint idx;
    for (idx = 0; idx < recordPerFile; idx++) {
        EXPECT_EQ(idx, dataDecode[idx].idx);
        EXPECT_DOUBLE_EQ(10 * idx + 0.1, dataDecode[idx].valueArray[0]);
        EXPECT_DOUBLE_EQ(10 * idx + 0.2, dataDecode[idx].valueArray[1]);
        EXPECT_DOUBLE_EQ(10 * idx + 0.3, dataDecode[idx].valueSingle);
    }

    // log.1
    fileOld1.read(dataBinary, sizeFile);

    int idxExpect = 0;
    for (idx = 0; idx < recordPerFile; idx++) {
        idxExpect = idx + recordPerFile;
        EXPECT_EQ(idxExpect, dataDecode[idx].idx);
        EXPECT_DOUBLE_EQ(10 * idxExpect + 0.1, dataDecode[idx].valueArray[0]);
        EXPECT_DOUBLE_EQ(10 * idxExpect + 0.2, dataDecode[idx].valueArray[1]);
        EXPECT_DOUBLE_EQ(10 * idxExpect + 0.3, dataDecode[idx].valueSingle);
    }
}
