#include <syslog.h>

#include "gtest/gtest.h"

extern "C" {
#include "circular_buffer.h"
}

struct CircularBufferTest : testing::Test {

    int bufferSize = 5;
    cbuf_handle_t cbuf;

    CircularBufferTest() {
        cbuf = circular_buf_init(bufferSize);

        openlog("CircularBuffer", LOG_CONS, LOG_SYSLOG);
    }

    ~CircularBufferTest() {
        circular_buf_free(cbuf);

        closelog();
    }
};

TEST(CircularBuffer, circularBufInit) { EXPECT_EQ(NULL, circular_buf_init(0)); }

TEST(CircularBuffer, circularBufInitSingleSlot) {

    // Buffer with single slot
    cbuf_handle_t cbuf = circular_buf_init(1);

    // No data
    EXPECT_EQ(1, circular_buf_capacity(cbuf));
    EXPECT_EQ(0, circular_buf_size(cbuf));

    commandStreamStructure_t data;
    bool status = circular_buf_get(cbuf, &data);
    EXPECT_TRUE(status);

    // Put one data
    data.counter = 99;
    circular_buf_put(cbuf, data);
    EXPECT_EQ(1, circular_buf_size(cbuf));

    // Get the data
    commandStreamStructure_t dataGet;
    status = circular_buf_get(cbuf, &dataGet);
    EXPECT_FALSE(status);
    EXPECT_EQ(99, dataGet.counter);
    EXPECT_EQ(0, circular_buf_size(cbuf));

    circular_buf_free(cbuf);
}

TEST_F(CircularBufferTest, circularBufCapacity) {
    EXPECT_EQ(bufferSize, circular_buf_capacity(cbuf));
}

TEST_F(CircularBufferTest, circularBufPut) {

    // There is no lost of data
    bool lostData = false;
    int idx;
    for (idx = 0; idx < bufferSize; idx++) {
        commandStreamStructure_t data;
        data.counter = idx;
        lostData = circular_buf_put(cbuf, data);
        EXPECT_FALSE(lostData);
    }

    EXPECT_TRUE(circular_buf_size(cbuf) == circular_buf_capacity(cbuf));

    // There is the lost of data
    commandStreamStructure_t dataOverwrite;
    dataOverwrite.counter = idx + 1;
    lostData = circular_buf_put(cbuf, dataOverwrite);
    EXPECT_TRUE(lostData);

    // Check the value is overwritten
    commandStreamStructure_t dataRead;
    circular_buf_get(cbuf, &dataRead);

    EXPECT_EQ(1, dataRead.counter);
}

TEST_F(CircularBufferTest, circularBufGet) {

    // Put the data into buffer
    int idx;
    for (idx = 0; idx < bufferSize; idx++) {
        commandStreamStructure_t data;
        data.counter = idx;
        circular_buf_put(cbuf, data);
    }

    // Test the normal condition
    commandStreamStructure_t dataRead;
    bool status;
    for (idx = 0; idx < bufferSize; idx++) {
        status = circular_buf_get(cbuf, &dataRead);
        EXPECT_FALSE(status);
        EXPECT_EQ(idx, dataRead.counter);
    }

    // Test the condition that the buffer is empty
    status = circular_buf_get(cbuf, &dataRead);
    EXPECT_TRUE(status);
}

TEST_F(CircularBufferTest, circularBufSize) {

    EXPECT_EQ(0, circular_buf_size(cbuf));

    commandStreamStructure_t data;
    circular_buf_put(cbuf, data);

    EXPECT_EQ(1, circular_buf_size(cbuf));
}
