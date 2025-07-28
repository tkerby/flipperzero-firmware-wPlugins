/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "ringbuffer.h"
static const char* TAG = "RingBuffer";

double RingBuffer_get(const RingBuffer* ringBuffer, size_t idx) {
    if(ringBuffer == NULL || ringBuffer->buffer == NULL || idx >= ringBuffer->count) {
        return 0; // Invalid input or index out of range
    }

    size_t startIndex =
        (ringBuffer->head + ringBuffer->bufferSize - ringBuffer->count) % ringBuffer->bufferSize;
    size_t actualIndex = (startIndex + idx) % ringBuffer->bufferSize;

    return ringBuffer->buffer[actualIndex];
}

/**
 * @brief Adds a value to the ring buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @param value Value to be added.
 */
double RingBuffer_getLast(RingBuffer* rb) {
    return RingBuffer_get(rb, 0);
}

/**
 * @brief Adds a value to the ring buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @param value Value to be added.
 */
void RingBuffer_add(RingBuffer* rb, double value) {
    rb->buffer[rb->head] = value;
    rb->head = (rb->head + 1) % rb->bufferSize;
    rb->count = rb->count < rb->bufferSize ? rb->count + 1 : rb->bufferSize;
}

/**
 * @brief Calculates and returns the average of the elements in the buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @return double The average of the elements.
 */
double RingBuffer_getAverage(RingBuffer* rb) {
    if(rb->count == 0) {
        return 0;
    }

    uint64_t sum = 0;
    size_t index = rb->head == 0 ? rb->bufferSize - 1 : rb->head - 1;

    for(size_t i = 0; i < rb->count; i++) {
        sum += rb->buffer[index];
        index = index == 0 ? rb->bufferSize - 1 : index - 1;
    }

    return (double)(sum / rb->count);
}

/**
 * @brief Computes the variance of the data in the buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @return double The variance of the buffer's elements.
 */
double RingBuffer_getVariance(RingBuffer* rb) {
    if(rb->count == 0) {
        return 0.0;
    }

    double mean = RingBuffer_getAverage(rb);
    double sum = 0.0;
    size_t index = rb->head == 0 ? rb->bufferSize - 1 : rb->head - 1;

    for(size_t i = 0; i < rb->count; i++) {
        double diff = rb->buffer[index] - mean;
        sum += diff * diff;
        index = index == 0 ? rb->bufferSize - 1 : index - 1;
    }

    return sum / rb->count;
}

/**
 * @brief Mappe la variance calculée sur un uint8_t.
 *
 * @param rb Pointeur sur le RingBuffer.
 * @return uint8_t Variance mappée dans un uint8_t.
 */
uint8_t RingBuffer_getVarianceMapped(RingBuffer* rb) {
    // Récupérer la variance du RingBuffer
    double variance = RingBuffer_getVariance(rb);

    // Limiter la variance à une valeur maximale
    if(variance > MAX_VARIANCE_MV) {
        variance = MAX_VARIANCE_MV;
    }

    // Mapper la variance sur une échelle de 0 à 255
    uint8_t mappedVariance = (uint8_t)((variance / MAX_VARIANCE_MV) * 255);

    return mappedVariance;
}

/**
 * @brief Calculates the standard deviation of the buffer's contents.
 *
 * @param ctx Pointer to the ring buffer context.
 * @return double The standard deviation of the elements.
 */
double RingBuffer_getStandardDeviation(RingBuffer* rb) {
    return sqrt(RingBuffer_getVariance(rb));
}

/**
 * @brief Mappe la déviation standard calculée sur un uint8_t.
 *
 * @param rb Pointeur sur le RingBuffer.
 * @return uint8_t Déviation standard mappée dans un uint8_t.
 */
uint8_t RingBuffer_getStandardDeviationMapped(RingBuffer* rb, double minVal, double maxVal) {
    // Récupérer la déviation standard du RingBuffer
    double stdDev = RingBuffer_getStandardDeviation(rb);

    // Calculer la déviation standard maximale attendue à partir des valeurs réelles
    double maxStdDev = (double)((maxVal - minVal) / 2); // Estimation max théorique de l'écart-type

    // Eviter une division par zéro si les valeurs sont constantes
    if(maxStdDev == 0) {
        return 0;
    }

    // Limiter la déviation standard à la valeur maximale calculée
    if(stdDev > maxStdDev) {
        stdDev = maxStdDev;
    }

    // Mapper la déviation standard sur une échelle de 0 à 255
    uint8_t mappedStdDev = (uint8_t)((stdDev / maxStdDev) * 255);

    return mappedStdDev;
}
/**
 * @brief Finds and returns the minimum value in the buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @return double The minimum value, or UINT16_MAX if the buffer is empty.
 */
double RingBuffer_getMin(RingBuffer* rb) {
    if(rb->count == 0) {
        return UINT16_MAX;
    }

    size_t index = rb->head == 0 ? rb->bufferSize - 1 : rb->head - 1;
    double min = UINT16_MAX;

    for(size_t i = 0; i < rb->count; i++) {
        if(rb->buffer[index] < min) {
            min = rb->buffer[index];
        }
        index = index == 0 ? rb->bufferSize - 1 : index - 1;
    }

    return min;
}

/**
 * @brief Finds and returns the maximum value in the buffer.
 *
 * @param ctx Pointer to the ring buffer context.
 * @return double The maximum value, or 0 if the buffer is empty.
 */
double RingBuffer_getMax(RingBuffer* rb) {
    if(rb->count == 0) {
        return 0;
    }

    size_t index = rb->head == 0 ? rb->bufferSize - 1 : rb->head - 1;
    double max = 0;

    for(size_t i = 0; i < rb->count; i++) {
        if(rb->buffer[index] > max) {
            max = rb->buffer[index];
        }
        index = index == 0 ? rb->bufferSize - 1 : index - 1;
    }

    return max;
}

/**
 * @brief Récupère les valeurs minimale et maximale du RingBuffer en une seule passe.
 *
 * @param rb Pointeur sur le RingBuffer.
 * @param min Pointeur pour stocker la valeur minimale.
 * @param max Pointeur pour stocker la valeur maximale.
 */
void RingBuffer_getMinMax(RingBuffer* rb, double* min, double* max) {
    // Vérifier si le RingBuffer est vide
    if(rb->count == 0) {
        *min = 0;
        *max = 0;
        return;
    }

    // Initialiser l'index du RingBuffer au dernier élément ajouté
    size_t index = rb->head == 0 ? rb->bufferSize - 1 : rb->head - 1;

    // Initialiser min et max avec la première valeur
    double minValue = rb->buffer[index];
    double maxValue = rb->buffer[index];

    // Parcourir tous les éléments du RingBuffer
    for(size_t i = 0; i < rb->count; i++) {
        if(rb->buffer[index] < minValue) {
            minValue = rb->buffer[index];
        }
        if(rb->buffer[index] > maxValue) {
            maxValue = rb->buffer[index];
        }
        // Passer à l'élément précédent (gestion circulaire)
        index = index == 0 ? rb->bufferSize - 1 : index - 1;
    }

    // Mettre à jour les valeurs min et max
    *min = minValue;
    *max = maxValue;
}
/**
 * @brief Reset the RingBuffer
 *
 * @param ctx Pointer to the ring buffer context.
 */
void RingBuffer_reset(RingBuffer* rb) {
    rb->head = 0;
    rb->count = 0;
    for(size_t i = 0; i < rb->bufferSize; i++) {
        rb->buffer[i] = 0;
    }
}
/**
 * @brief Change the size of the RingBuffer
 *
 * @param ctx Pointer to the ring buffer context.
 * @param newSize New size of the buffer.
 */
void RingBuffer_changeSize(RingBuffer* rb, size_t newSize) {
    double* newBuffer = (double*)realloc(rb->buffer, newSize * sizeof(double));
    if(newBuffer == NULL) {
        FURI_LOG_E(TAG, "Error reallocating buffer");
        return;
    }

    rb->buffer = newBuffer;
    rb->bufferSize = newSize;
    rb->count = rb->count > newSize ? newSize : rb->count;
    rb->head = rb->count; // Reset head to the start of the buffer if reduced
    for(size_t i = rb->count; i < newSize; i++) {
        rb->buffer[i] = 0; // Initialize new elements to zero if the buffer is expanded
    }
}

/**
 * @brief Creates and initializes a new ring buffer of a specified size.
 *
 * This function allocates memory for a new ring buffer structure and its associated data array.
 * The buffer is initialized to hold double elements with all values set to zero.
 * The function sets up initial conditions for an empty buffer with specified capacity.
 *
 * @param initialSize The size of the buffer to be created, representing the maximum number of double elements it can hold.
 * @return RingBuffer* A pointer to the newly created ring buffer structure. Returns NULL if memory allocation fails.
 *
 * @note It is the caller's responsibility to ensure that the returned ring buffer is eventually freed by calling `RingBuffer_free` to avoid memory leaks.
 */

RingBuffer* RingBuffer_create(size_t initialSize) {
    RingBuffer* rb = (RingBuffer*)malloc(sizeof(RingBuffer));
    if(rb == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate RingBuffer struct");
        return NULL;
    }

    rb->buffer = (double*)calloc(initialSize, sizeof(double));
    if(rb->buffer == NULL) {
        free(rb);
        FURI_LOG_E(TAG, "Failed to allocate buffer");
        return NULL;
    }

    rb->bufferSize = initialSize;
    rb->head = 0;
    rb->count = 0;
    return rb;
}

/**
 * @brief Frees the memory allocated for a ring buffer and its internal buffer.
 *
 * This function safely deallocates the memory used by the ring buffer structure and its associated data array.
 * It is essential to call this function for any ring buffer created with `RingBuffer_create` to avoid memory leaks.
 * After calling this function, the pointer to the ring buffer should be considered invalid and should not be used further.
 *
 * @param rb A pointer to the ring buffer to be freed. If NULL is passed, the function has no effect.
 *
 * @note After this function is called, any attempt to use the ring buffer pointer will result in undefined behavior.
 *       It is advisable to set the pointer to NULL in the calling code after freeing to prevent accidental usage.
 */
void RingBuffer_free(RingBuffer* rb) {
    FURI_LOG_I(TAG, "RingBuffer_free");
    free(rb->buffer);
    free(rb);
}
