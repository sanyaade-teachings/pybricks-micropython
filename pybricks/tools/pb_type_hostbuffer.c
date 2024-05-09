// SPDX-License-Identifier: MIT
// Copyright (c) 2024 The Pybricks Authors

#include "py/mpconfig.h"

#if PYBRICKS_PY_TOOLS_HOSTBUFFER

#include <string.h>

#include <pbsys/command.h>

#include "py/mphal.h"
#include "py/objstr.h"

#include <pybricks/tools.h>

#include <pybricks/util_mp/pb_kwarg_helper.h>
#include <pybricks/util_mp/pb_obj_helper.h>
#include <pybricks/util_pb/pb_error.h>

typedef struct _pb_type_hostbuffer_obj_t {
    mp_obj_base_t base;
    mp_obj_t format;
    mp_obj_str_t bytes_obj;
    uint8_t buffer[];
} pb_type_hostbuffer_obj_t;

// pointer to dynamically allocated hostbuffer singleton for driver callback.
static pb_type_hostbuffer_obj_t *hostbuffer_instance;

static void handle_write_data_buffer(uint16_t offset, uint32_t size, const uint8_t *data) {
    // Can't write if buffer does not exist or isn't big enough.
    if (!hostbuffer_instance || offset + size > hostbuffer_instance->bytes_obj.len) {
        return;
    }
    memcpy(hostbuffer_instance->buffer + offset, data, size);
}

STATIC mp_obj_t pb_type_hostbuffer_get_bytes(mp_obj_t self_in) {
    pb_type_hostbuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // Don't return internal bytes object but make a copy so the user bytes
    // object is constant as would be expected. Revisit: enable and return
    // a memoryview, especially if using large buffers.
    return mp_obj_new_bytes(self->bytes_obj.data, self->bytes_obj.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pb_type_hostbuffer_get_bytes_obj, pb_type_hostbuffer_get_bytes);

STATIC mp_obj_t pb_type_hostbuffer_get_values(mp_obj_t self_in) {

    // One-off import for ustruct.unpack.
    static mp_obj_t ustruct_unpack = NULL;
    if (!ustruct_unpack) {
        ustruct_unpack = pb_function_import_helper(MP_QSTR_ustruct, MP_QSTR_unpack);
    }

    // Host (sender) is responsible for making sure that each individual
    // value remains valid, i.e. is written in a single chunk, since the
    // following may allocate, and thus be updated between unpacking values.
    pb_type_hostbuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_call_function_2(ustruct_unpack, self->format, MP_OBJ_FROM_PTR(&self->bytes_obj));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pb_type_hostbuffer_get_values_obj, pb_type_hostbuffer_get_values);


STATIC mp_obj_t pb_type_hostbuffer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {

    PB_PARSE_ARGS_CLASS(n_args, n_kw, args,
        PB_ARG_REQUIRED(format));

    // Use ustruct.calcsize to parse user format for size.
    mp_obj_t ustruct_calcsize = pb_function_import_helper(MP_QSTR_ustruct, MP_QSTR_calcsize);
    size_t size = mp_obj_get_int(mp_call_function_1(ustruct_calcsize, format_in));

    // Can only create one instance for now.
    if (hostbuffer_instance) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("host buffer already allocated"));
    }

    // Use finalizer so we can deactivate the data callback when buffer is garbage collected.
    hostbuffer_instance = m_new_obj_var_with_finaliser(pb_type_hostbuffer_obj_t, uint8_t, size);
    hostbuffer_instance->base.type = type;
    hostbuffer_instance->format = format_in;

    // Keep buffer in bytes object format for compatibility with unpack.
    hostbuffer_instance->bytes_obj.base.type = &mp_type_bytes;
    hostbuffer_instance->bytes_obj.len = size;
    hostbuffer_instance->bytes_obj.data = hostbuffer_instance->buffer;

    // Activate callback now that we have allocated the buffer.
    pbsys_command_set_write_program_data_buffer_callback(handle_write_data_buffer);

    return MP_OBJ_FROM_PTR(hostbuffer_instance);
}

mp_obj_t pb_type_hostbuffer_close(mp_obj_t stream) {
    if (hostbuffer_instance) {
        pbsys_command_set_write_program_data_buffer_callback(NULL);
        hostbuffer_instance = NULL;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(pb_type_hostbuffer_close_obj, pb_type_hostbuffer_close);

STATIC const mp_rom_map_elem_t pb_type_hostbuffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),      MP_ROM_PTR(&pb_type_hostbuffer_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),        MP_ROM_PTR(&pb_type_hostbuffer_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_bytes),    MP_ROM_PTR(&pb_type_hostbuffer_get_bytes_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_values),   MP_ROM_PTR(&pb_type_hostbuffer_get_values_obj) },
};
STATIC MP_DEFINE_CONST_DICT(pb_type_hostbuffer_locals_dict, pb_type_hostbuffer_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(pb_type_hostbuffer,
    MP_QSTR_HostBuffer,
    MP_TYPE_FLAG_NONE,
    make_new, pb_type_hostbuffer_make_new,
    locals_dict, &pb_type_hostbuffer_locals_dict);

#endif // PYBRICKS_PY_TOOLS_HOSTBUFFER
