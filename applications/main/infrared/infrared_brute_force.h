/**
 * @file infrared_brute_force.h
 * @brief Infrared signal brute-forcing library.
 *
 * The BruteForce library is used to send large quantities of signals,
 * sorted by a category. It is used to implement the Universal Remote
 * feature.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "infrared_error_code.h"

/**
 * @brief InfraredBruteForce opaque type declaration.
 */
typedef struct InfraredBruteForce InfraredBruteForce;

/**
 * @brief Create a new InfraredBruteForce instance.
 *
 * @returns pointer to the created instance.
 */
InfraredBruteForce* infrared_brute_force_alloc(void);

/**
 * @brief Delete an InfraredBruteForce instance.
 *
 * @param[in,out] brute_force pointer to the instance to be deleted.
 */
void infrared_brute_force_free(InfraredBruteForce* brute_force);

/**
 * @brief Set an InfraredBruteForce instance to use a signal database contained in a file.
 *
 * @param[in,out] brute_force pointer to the instance to be configured.
 * @param[in] db_filename pointer to a zero-terminated string containing a full path to the database file.
 */
void infrared_brute_force_set_db_filename(InfraredBruteForce* brute_force, const char* db_filename);

/**
 * @brief Build a signal dictionary from a previously set database file.
 *
 * This function must be called each time after setting the database via
 * a infrared_brute_force_set_db_filename() call.
 *
 * @param[in,out] brute_force pointer to the instance to be updated.
 * @param[in] auto_detect_buttons bool whether to automatically register newly discovered buttons.
 * @returns InfraredErrorCodeNone on success, otherwise error code.
 */
InfraredErrorCode infrared_brute_force_calculate_messages(
    InfraredBruteForce* brute_force,
    bool auto_detect_buttons);

/**
 * @brief Start transmitting signals from a category stored in an InfraredBruteForce's instance dictionary.
 *
 * @param[in,out] brute_force pointer to the instance to be started.
 * @param[in] index index of the signal category in the dictionary.
 * @returns true on success, false otherwise.
 */
bool infrared_brute_force_start(
    InfraredBruteForce* brute_force,
    uint32_t index,
    uint32_t* record_count);

/**
 * @brief Determine whether the transmission was started.
 *
 * @param[in] brute_force pointer to the instance to be tested.
 * @returns true if transmission was started, false otherwise.
 */
bool infrared_brute_force_is_started(const InfraredBruteForce* brute_force);

/**
 * @brief Stop transmitting the signals.
 *
 * @param[in] brute_force pointer to the instance to be stopped.
 */
void infrared_brute_force_stop(InfraredBruteForce* brute_force);

/**
 * @brief Send an arbitrary signal from the chosen category.
 * 
 * @param[in] brute_force pointer to the instance
 * @param signal_index the index of the signal within the category, must be
 *                     between 0 and `record_count` as told by
 *                     `infrared_brute_force_start`
 * 
 * @returns true on success, false otherwise
 */
bool infrared_brute_force_send(InfraredBruteForce* brute_force, uint32_t signal_index);

/**
 * @brief Add a signal category to an InfraredBruteForce instance's dictionary.
 *
 * @param[in,out] brute_force pointer to the instance to be updated.
 * @param[in] index index of the category to be added.
 * @param[in] name name of the category to be added.
 */
void infrared_brute_force_add_record(
    InfraredBruteForce* brute_force,
    uint32_t index,
    const char* name);

/**
 * @brief Reset an InfraredBruteForce instance.
 *
 * @param[in,out] brute_force pointer to the instance to be reset.
 */
void infrared_brute_force_reset(InfraredBruteForce* brute_force);

/**
 * @brief Get the total number of unique button names in the database, for example, 
 *        if a button name is "Power" and it appears 3 times in the db, then the 
 *        db_size is 1, instead of 3.
 *
 * @param[in] brute_force pointer to the InfraredBruteForce instance.
 * @return size_t number of unique button names.
 */
size_t infrared_brute_force_get_button_count(const InfraredBruteForce* brute_force);

/**
 * @brief Get the button name at the specified index.
 *
 * @param[in] brute_force pointer to the InfraredBruteForce instance.
 * @param[in] index index of the button name to retrieve.
 * @return const char* button name, or NULL if index is out of range.
 */
const char*
    infrared_brute_force_get_button_name(const InfraredBruteForce* brute_force, size_t index);
