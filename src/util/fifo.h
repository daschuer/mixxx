#pragma once

#include "pam_ringbuffer.h"
#include "util/class.h"
#include "util/math.h"

template <class DataType>
class FIFO {
  public:
    explicit FIFO(int size)
            : m_data(roundUpToPowerOf2(size)) {
        // If we can't represent the next higher power of 2 then bail.
        if (m_data.size() == 0) {
            return;
        }
        PamUtil_InitializeRingBuffer(&m_ringBuffer,
                static_cast<ring_buffer_size_t>(sizeof(DataType)),
                static_cast<ring_buffer_size_t>(m_data.size()),
                m_data.data());
    }
    virtual ~FIFO() {
    }
    int readAvailable() const {
        return PamUtil_GetRingBufferReadAvailable(&m_ringBuffer);
    }
    int writeAvailable() const {
        return PamUtil_GetRingBufferWriteAvailable(&m_ringBuffer);
    }
    int read(DataType* pData, int count) {
        return PamUtil_ReadRingBuffer(&m_ringBuffer, pData, count);
    }
    int write(const DataType* pData, int count) {
        return PamUtil_WriteRingBuffer(&m_ringBuffer, pData, count);
    }
    void writeBlocking(const DataType* pData, int count) {
        int written = 0;
        while (written < count) {
            written += write(pData + written, count - written);
        }
    }
    int aquireWriteRegions(int count,
            DataType** dataPtr1, ring_buffer_size_t* sizePtr1,
            DataType** dataPtr2, ring_buffer_size_t* sizePtr2) {
        return PamUtil_GetRingBufferWriteRegions(&m_ringBuffer,
                count,
                (void**)dataPtr1,
                sizePtr1,
                (void**)dataPtr2,
                sizePtr2);
    }
    int releaseWriteRegions(int count) {
        return PamUtil_AdvanceRingBufferWriteIndex(&m_ringBuffer, count);
    }
    int aquireReadRegions(int count,
            DataType** dataPtr1, ring_buffer_size_t* sizePtr1,
            DataType** dataPtr2, ring_buffer_size_t* sizePtr2) {
        return PamUtil_GetRingBufferReadRegions(&m_ringBuffer,
                count,
                (void**)dataPtr1,
                sizePtr1,
                (void**)dataPtr2,
                sizePtr2);
    }
    int releaseReadRegions(int count) {
        return PamUtil_AdvanceRingBufferReadIndex(&m_ringBuffer, count);
    }
    int flushReadData(int count) {
        int flush = math_min(readAvailable(), count);
        return PamUtil_AdvanceRingBufferReadIndex(&m_ringBuffer, flush);
    }

  private:
    std::vector<DataType> m_data;
    PaUtilRingBuffer m_ringBuffer;
    DISALLOW_COPY_AND_ASSIGN(FIFO);
};
