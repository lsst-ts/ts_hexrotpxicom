#include <fstream>
#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "logTlm.h"
}

struct Data {
    int idx;
    int value;
};

struct LogTlmTest : testing::Test {

    char *pathDir = "./";
    char *formatFilename = "tlm_%m_%d_%Y_%H_%M_%S.log";

    int recordPerFile = 2;

    struct Data data;

    LogTlmTest() {
        logTlm_open(pathDir, formatFilename, recordPerFile);
    }

    ~LogTlmTest() {
        logTlm_close();
    }
};

TEST(LogTlm, logTlmOpen) {

    char *pathDir = "./";
    char *formatFilename = "tlm_%m_%d_%Y_%H_%M_%S.log";

    EXPECT_EQ(-1, logTlm_open(pathDir, formatFilename, 0));
    EXPECT_EQ(-1, logTlm_open(pathDir, formatFilename, -1));
}

TEST_F(LogTlmTest, logTlmGetFilename) {

    std::ifstream file(logTlm_getFilename());
    EXPECT_TRUE(file.good());
}

TEST_F(LogTlmTest, logTlmClose) { EXPECT_EQ(0, logTlm_close()); }

TEST_F(LogTlmTest, logTlmWrite) {

    // Assume we are opening a new telemetry file
    sleep(2);
    logTlm_open(pathDir, formatFilename, recordPerFile);

    // Write the data
    int numData = 2;
    struct Data datas[numData];

    int idxInData;
    int idx;
    for (idx = 0; idx < 10; idx++) {

        idxInData = idx % numData;
        datas[idxInData].idx = idx;
        datas[idxInData].value = 10 * idx;

        if (idxInData == (numData - 1)) {
            logTlm_write(datas, sizeof(datas));
        }
    }

    // Check the file is rotated or not
    std::string fileCurrent = logTlm_getFilename();
    std::ifstream fileOld0(fileCurrent + ".0", std::ifstream::binary);
    std::ifstream fileOld1(fileCurrent + ".1", std::ifstream::binary);
    EXPECT_TRUE(fileOld0.good());
    EXPECT_TRUE(fileOld1.good());

    // Check the data in file
    int numDataPerFile = numData * recordPerFile;
    int sizeFile = numDataPerFile * sizeof(Data);
    char dataBinary[sizeFile];

    // log.0
    fileOld0.read(dataBinary, sizeFile);
    struct Data *dataDecode = (struct Data *)dataBinary;

    for (idx = 0; idx < numDataPerFile; idx++) {
        EXPECT_EQ(idx, dataDecode[idx].idx);
        EXPECT_EQ(10 * idx, dataDecode[idx].value);
    }

    // log.1
    fileOld1.read(dataBinary, sizeFile);

    int idxExpect = 0;
    for (idx = 0; idx < numDataPerFile; idx++) {
        idxExpect = idx + numDataPerFile;
        EXPECT_EQ(idxExpect, dataDecode[idx].idx);
        EXPECT_EQ(10 * idxExpect, dataDecode[idx].value);
    }
}
