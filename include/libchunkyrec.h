#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include <freertos/task.h>

#include <SPI.h>
#include <SD.h>

#define LIBCHUNKYREC_RECNUM     708
#define LIBCHUNKYREC_RECLENGTH  240

#define LIBCHUNKYREC_BUFFER_SIZE    (LIBCHUNKYREC_RECNUM * LIBCHUNKYREC_RECLENGTH)

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

#ifndef LOG
#define LOG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__);
#endif

static void vChunkSaver(void* pvParameters);

struct LibChunkyState {
    QueueHandle_t       queue;
    SemaphoreHandle_t   done;

    size_t              chunk_count;
};

struct LibChunkyWaveHeader {
    char        riff[4]     = {'R', 'I', 'F', 'F'};
    uint32_t    file_size   = 0;

    char        wave[4]     = {'W', 'A', 'V', 'E'};
    char        fmt[4]      = {'f', 'm', 't', ' '};

    uint32_t    fmt_size    = 16;
    uint16_t    format      = 1;
    uint16_t    channels    = 1;

    uint32_t    sample_rate = 17000;
    uint32_t    byte_rate   = 17000 * sizeof(int16_t);

    uint16_t    block_align = sizeof(int16_t);
    uint16_t    bps         = 16;

    char        data[4]     = {'d', 'a', 't', 'a'};
    uint32_t    data_size   = 0;
};

class _LibChunkyRec {
    private:
        LibChunkyState  state;
    public:
        /* Initializes the library */
        bool init();

        /*
            Starts recording the input from the microphone. 
            Call ``stop_record()`` to stop the recording and 
            save it as an mp3 file on the SD card.
        */
        void record();

        /*
            Stops recording and saves the recording 
            under ``path`` as a wav file.
        */
        void stop_record(String path);

        /* Deinitializes the library */
        void deinit();
};

extern _LibChunkyRec LibChunkyRec;