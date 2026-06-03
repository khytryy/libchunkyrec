#include "libchunkyrec.h"

_LibChunkyRec LibChunkyRec;

bool _LibChunkyRec::init() {
    M5Cardputer.Mic.begin();

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        LOG("libchunkyrec: ERROR: Failed to initialize SD card.\n");
        return false;
    }

    this->state.queue   = xQueueCreate(10, sizeof(int16_t*));
    this->state.done    = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(vChunkSaver, "libchunkyrec ChunkSaver", 4096, (void* )&this->state, 6, NULL, 1);

    return true;
}

void _LibChunkyRec::record() {
    if (M5Cardputer.Mic.isEnabled()) {
        int16_t* chunk = (int16_t*)heap_caps_malloc(LIBCHUNKYREC_RECLENGTH * sizeof(int16_t), MALLOC_CAP_8BIT);
        if (!chunk) {
            LOG("libchunkyrec: ERROR: Failed to allocate chunk.\n");
            return;
        }
        if (M5Cardputer.Mic.record(chunk, LIBCHUNKYREC_RECLENGTH, 17000)) {
            xQueueSend(this->state.queue, &chunk, portMAX_DELAY);
        } else {
            free(chunk);
        }
    }
}

void _LibChunkyRec::deinit() {
    M5Cardputer.Mic.end();
}

static void vChunkSaver(void* pvParameters) {
    int16_t* chunk;
    LibChunkyState* state = (LibChunkyState* )pvParameters;
    state->chunk_count = 0;

    if (!SD.mkdir("libchunkyrec_tmp") && !SD.exists("libchunkyrec_tmp")) {
        LOG("libchunkyrec: ERROR: Failed to create a temp directory.\n");
        vTaskDelete(NULL);
    }

    for (;;) {
        if (xQueueReceive(state->queue, &chunk, portMAX_DELAY)) {
            if (chunk == nullptr) {
                xSemaphoreGive(state->done);
                vTaskDelete(NULL);
            }

            File fchunk = SD.open("/libchunkyrec_tmp/" + String(state->chunk_count) + ".raw", FILE_WRITE);

            if (!fchunk) {
                LOG("libchunkyrec: ERROR: Failed to open the chunk\n");
                free(chunk);

                xSemaphoreGive(state->done);
                vTaskDelete(NULL);
            }
            fchunk.write((uint8_t* )chunk, LIBCHUNKYREC_RECLENGTH * sizeof(int16_t));

            fchunk.close();
            free(chunk);

            state->chunk_count++;
        }
    }
}

void _LibChunkyRec::stop_record(String path) {
    // Send a nullptr to the queue to indicate that the recording should stop
    int16_t* t = nullptr;

    xQueueSend(this->state.queue, &t, portMAX_DELAY);
    xSemaphoreTake(this->state.done, portMAX_DELAY);

    File wav = SD.open(path, FILE_WRITE);
    if (!wav) {
        LOG("libchunkyrec: ERROR: Failed to open %s\n", path);
        return;
    }

    LibChunkyWaveHeader header;
    header.data_size = (this->state.chunk_count * LIBCHUNKYREC_RECLENGTH) * sizeof(int16_t);
    header.file_size = 36 + header.data_size;

    wav.write((uint8_t* )&header, sizeof(LibChunkyWaveHeader));

    for (size_t i = 0; i < this->state.chunk_count; i++) {
        File chunk = SD.open("/libchunkyrec_tmp/" + String(i) + ".raw");

        int16_t* raw_chunk = (int16_t* )malloc(LIBCHUNKYREC_RECLENGTH * sizeof(int16_t));
        size_t bytes = chunk.read((uint8_t* )raw_chunk, LIBCHUNKYREC_RECLENGTH * sizeof(int16_t));

        if (bytes != LIBCHUNKYREC_RECLENGTH * sizeof(int16_t)) {
            LOG("libchunkyrec: WARNING: Chunk %d is incomplete (%d/%d bytes)\n", i, bytes, LIBCHUNKYREC_RECLENGTH * sizeof(int16_t));
        }

        wav.write((uint8_t* )raw_chunk, LIBCHUNKYREC_RECLENGTH * sizeof(int16_t));

        chunk.close();
        free(raw_chunk);
    }

    File tmp = SD.open("/libchunkyrec_tmp");

    // Loop over all of the files in the temp folder and delete them
    while (true) {
        File f = tmp.openNextFile();
        if (!f) break;

        SD.remove(f.path());
        f.close();
    }

    tmp.close();
    // Remove temp dir
    SD.rmdir("/libchunkyrec_tmp");

    wav.close();
}