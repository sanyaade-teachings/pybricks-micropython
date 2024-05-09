// SPDX-License-Identifier: MIT
// Copyright (c) 2022-2023 The Pybricks Authors

#include <assert.h>
#include <stdint.h>

#include <pbdrv/reset.h>
#include <pbio/protocol.h>
#include <pbsys/command.h>

#include "./bluetooth.h"
#include "./program_load.h"
#include "./program_stop.h"

static pbsys_command_write_program_data_buffer_callback_t write_program_data_buffer_callback = NULL;

/**
 * Sets callback for the write program data buffer command.
 *
 * @param [in]  callback  The callback to set or @c NULL to unset.
 */
void pbsys_command_set_write_program_data_buffer_callback(pbsys_command_write_program_data_buffer_callback_t callback) {
    write_program_data_buffer_callback = callback;
}

/**
 * Parses binary data for command and dispatches handler for command.
 * @param [in]  data    The raw command data.
 * @param [in]  size    The size of @p data in bytes.
 */
pbio_pybricks_error_t pbsys_command(const uint8_t *data, uint32_t size) {
    assert(data);
    assert(size);

    pbio_pybricks_command_t cmd = data[0];

    switch (cmd) {
        case PBIO_PYBRICKS_COMMAND_STOP_USER_PROGRAM:
            pbsys_program_stop(false);
            return PBIO_PYBRICKS_ERROR_OK;
        case PBIO_PYBRICKS_COMMAND_START_USER_PROGRAM:
            return pbio_pybricks_error_from_pbio_error(pbsys_program_load_start_user_program());
        case PBIO_PYBRICKS_COMMAND_START_REPL:
            return pbio_pybricks_error_from_pbio_error(pbsys_program_load_start_repl());
        case PBIO_PYBRICKS_COMMAND_WRITE_USER_PROGRAM_META:
            return pbio_pybricks_error_from_pbio_error(pbsys_program_load_set_program_size(
                pbio_get_uint32_le(&data[1])));
        case PBIO_PYBRICKS_COMMAND_WRITE_USER_RAM:
            return pbio_pybricks_error_from_pbio_error(pbsys_program_load_set_program_data(
                pbio_get_uint32_le(&data[1]), &data[5], size - 5));
        case PBIO_PYBRICKS_COMMAND_REBOOT_TO_UPDATE_MODE:
            pbdrv_reset(PBDRV_RESET_ACTION_RESET_IN_UPDATE_MODE);
            return PBIO_PYBRICKS_ERROR_OK;
        case PBIO_PYBRICKS_COMMAND_WRITE_STDIN:
            #if PBSYS_CONFIG_BLUETOOTH
            if (pbsys_bluetooth_rx_get_free() < size - 1) {
                return PBIO_PYBRICKS_ERROR_BUSY;
            }
            pbsys_bluetooth_rx_write(&data[1], size - 1);
            #endif
            // If no consumers are configured, goes to "/dev/null" without error
            return PBIO_PYBRICKS_ERROR_OK;
        case PBIO_PYBRICKS_COMMAND_WRITE_PROGRAM_DATA_BUFFER:
            if (write_program_data_buffer_callback && size > 3) {
                uint16_t offset = pbio_get_uint16_le(&data[1]);
                uint16_t data_size = size - 3;
                const uint8_t *data_write = &data[3];
                write_program_data_buffer_callback(offset, data_size, data_write);
            }
            // If no consumers are configured, goes to "/dev/null" without error
            return PBIO_PYBRICKS_ERROR_OK;
        default:
            return PBIO_PYBRICKS_ERROR_INVALID_COMMAND;
    }
}
