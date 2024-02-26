#include <fstream>
#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "logTlm.h"
}

struct Data {
    uint idx;
    uint value;
};

struct LogTlmTest : testing::Test {

    char *pathDir = "./";
    char *formatFilename = "tlm_%m_%d_%Y_%H_%M_%S.log";

    uint sizeBuffer = 2;
    uint recordPerFile = 2;

    struct Data data;

    LogTlmTest() {
        logTlm_open(pathDir, formatFilename, sizeBuffer, sizeof(Data),
                    recordPerFile);
    }

    ~LogTlmTest() {
        logTlm_close();
        logTlm_freeBuffer();
    }
};

TEST(LogTlm, logTlmOpen) {

    char *pathDir = "./";
    char *formatFilename = "tlm_%m_%d_%Y_%H_%M_%S.log";

    EXPECT_EQ(-1, logTlm_open(pathDir, formatFilename, 0, 1, 1));
    EXPECT_EQ(-1, logTlm_open(pathDir, formatFilename, 1, 0, 1));
    EXPECT_EQ(-1, logTlm_open(pathDir, formatFilename, 1, 1, 0));
}

TEST_F(LogTlmTest, logTlmGetFilename) {

    std::ifstream file(logTlm_getFilename());
    EXPECT_TRUE(file.good());
}

TEST_F(LogTlmTest, logTlmFreeBuffer) {

    EXPECT_NE(nullptr, logTlm_getBuffer());

    logTlm_freeBuffer();
    EXPECT_EQ(nullptr, logTlm_getBuffer());
}

TEST_F(LogTlmTest, logTlmClose) { EXPECT_EQ(0, logTlm_close()); }

TEST_F(LogTlmTest, logTlmWrite) {

    // Assume we are opening a new telemetry file
    sleep(2);
    logTlm_open(pathDir, formatFilename, sizeBuffer, sizeof(Data),
                recordPerFile);

    // Write the data
    uint idx = 0;
    for (idx = 0; idx < 10; idx++) {
        data.idx = idx;
        data.value = 10 * idx;
        logTlm_write(&data);
    }

    // Check the current data in buffer
    struct Data *datas = (struct Data *)logTlm_getBuffer();

    EXPECT_EQ(8, datas[0].idx);
    EXPECT_EQ(80, datas[0].value);
    EXPECT_EQ(9, datas[1].idx);
    EXPECT_EQ(90, datas[1].value);

    // Check the file is rotated or not
    std::string fileCurrent = logTlm_getFilename();
    std::ifstream fileOld0(fileCurrent + ".0", std::ifstream::binary);
    std::ifstream fileOld1(fileCurrent + ".1", std::ifstream::binary);
    EXPECT_TRUE(fileOld0.good());
    EXPECT_TRUE(fileOld1.good());

    // Check the data in file
    uint numRecordPerFile = sizeBuffer * recordPerFile;
    uint sizeFile = numRecordPerFile * sizeof(Data);
    char dataBinary[sizeFile];

    // log.0
    fileOld0.read(dataBinary, sizeFile);
    struct Data *dataDecode = (struct Data *)dataBinary;

    for (idx = 0; idx < numRecordPerFile; idx++) {
        EXPECT_EQ(idx, dataDecode[idx].idx);
        EXPECT_EQ(10 * idx, dataDecode[idx].value);
    }

    // log.1
    fileOld1.read(dataBinary, sizeFile);

    int idxExpect = 0;
    for (idx = 0; idx < numRecordPerFile; idx++) {
        idxExpect = idx + numRecordPerFile;
        EXPECT_EQ(idxExpect, dataDecode[idx].idx);
        EXPECT_EQ(10 * idxExpect, dataDecode[idx].value);
    }
}
