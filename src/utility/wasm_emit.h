#ifndef FRANCA2_WASM_EMIT_H
#define FRANCA2_WASM_EMIT_H
#include "primitives.h"
#include "arr_dyns.h"
#include "strings.h"

namespace wasm_emit {
    using namespace strings;
    using namespace arenas;

    struct wasm_emitter {
        arena& arena;

        stream wasm;
        stream section;
    };

#define using_emitter ref [arena, wasm, section] =

    wasm_emitter make_emitter(arena& arena = gta);

    enum struct wasm_section_id: u8 {
        sec_custom  =  0,
        sec_type    =  1,
        sec_import  =  2,
        sec_func    =  3,
        sec_table   =  4,
        sec_memory  =  5,
        sec_global  =  6,
        sec_export  =  7,
        sec_start   =  8,
        sec_element =  9,
        sec_code    = 10,
        sec_data    = 11,
        sec_data_count = 12,
    };

    enum wasm_value_type: u8 {
        vt_void = 0x40,
        vt_f64  = 0x7c,
        vt_f32  = 0x7d,
        vt_i64  = 0x7e,
        vt_i32  = 0x7f,
    };

    struct wasm_func_type {
        arr_view<wasm_value_type> params;
        arr_view<wasm_value_type> results;
    };
    typedef uint wasm_func_type_index;

    struct wasm_func {
        wasm_func_type_index type_index;
        arr_view<wasm_value_type> locals;
        arr_view<u8> body;
    };

    struct wasm_memory {
        uint min;
        uint max;
    };

    struct wasm_data {
        arr_view<u8> data;
    };

    struct wasm_func_import {
        string module;
        string name;
        wasm_func_type_index type_index;
    };

    enum struct wasm_export_kind: uint {
        ek_func   = 0x00,
        ek_table  = 0x01,
        ek_mem    = 0x02,
        ek_global = 0x03,
    };

    struct wasm_export {
        string name;
        wasm_export_kind kind;
        uint obj_index;
    };

    enum struct wasm_opcode: u8 {
        op_unreachable      = 0x00,
        op_nop              = 0x01,
        op_block            = 0x02,
        op_loop             = 0x03,
        op_if               = 0x04,
        op_else             = 0x05,

        op_end              = 0x0b,
        op_br               = 0x0c,
        op_br_if            = 0x0d,
        op_br_table         = 0x0e,
        op_return           = 0x0f,
        op_call             = 0x10,
        op_call_indirect    = 0x11,

        op_drop             = 0x1a,
        op_select           = 0x1b,
        op_select_t         = 0x1c,

        op_local_get        = 0x20,
        op_local_set        = 0x21,
        op_local_tee        = 0x22,
        op_global_get       = 0x23,
        op_global_set       = 0x24,
        op_table_get        = 0x25,
        op_table_set        = 0x26,

        op_i32_load         = 0x28,
        op_i64_load         = 0x29,
        op_f32_load         = 0x2a,
        op_f64_load         = 0x2b,

        op_i32_load_8_s     = 0x2c,
        op_i32_load_8_u     = 0x2d,
        op_i32_load_16_s    = 0x2e,
        op_i32_load_16_u    = 0x2f,

        op_i64_load_8_s     = 0x30,
        op_i64_load_8_u     = 0x31,
        op_i64_load_16_s    = 0x32,
        op_i64_load_16_u    = 0x33,
        op_i64_load_32_s    = 0x34,
        op_i64_load_32_u    = 0x35,

        op_i32_store        = 0x36,
        op_i64_store        = 0x37,
        op_f32_store        = 0x38,
        op_f64_store        = 0x39,

        op_i32_store_8      = 0x3a,
        op_i32_store_16     = 0x3b,
        op_i64_store_8      = 0x3c,
        op_i64_store_16     = 0x3d,
        op_i64_store_32     = 0x3e,

        op_memory_size      = 0x3f,
        op_memory_grow      = 0x40,

        op_i32_const        = 0x41,
        op_i64_const        = 0x42,
        op_f32_const        = 0x43,
        op_f64_const        = 0x44,

        op_i32_eqz          = 0x45,
        op_i32_eq           = 0x46,
        op_i32_ne           = 0x47,
        op_i32_lt_s         = 0x48,
        op_i32_lt_u         = 0x49,
        op_i32_gt_s         = 0x4a,
        op_i32_gt_u         = 0x4b,
        op_i32_le_s         = 0x4c,
        op_i32_le_u         = 0x4d,
        op_i32_ge_s         = 0x4e,
        op_i32_ge_u         = 0x4f,

        op_i64_eqz          = 0x50,
        op_i64_eq           = 0x51,
        op_i64_ne           = 0x52,
        op_i64_lt_s         = 0x53,
        op_i64_lt_u         = 0x54,
        op_i64_gt_s         = 0x55,
        op_i64_gt_u         = 0x56,
        op_i64_le_s         = 0x57,
        op_i64_le_u         = 0x58,
        op_i64_ge_s         = 0x59,
        op_i64_ge_u         = 0x5a,

        op_f32_eq           = 0x5b,
        op_f32_ne           = 0x5c,
        op_f32_lt           = 0x5d,
        op_f32_gt           = 0x5e,
        op_f32_le           = 0x5f,
        op_f32_ge           = 0x60,

        op_f64_eq           = 0x61,
        op_f64_ne           = 0x62,
        op_f64_lt           = 0x63,
        op_f64_gt           = 0x64,
        op_f64_le           = 0x65,
        op_f64_ge           = 0x66,

        op_i32_clz          = 0x67,
        op_i32_ctz          = 0x68,
        op_i32_popcnt       = 0x69,
        op_i32_add          = 0x6a,
        op_i32_sub          = 0x6b,
        op_i32_mul          = 0x6c,
        op_i32_div_s        = 0x6d,
        op_i32_div_u        = 0x6e,
        op_i32_rem_s        = 0x6f,
        op_i32_rem_u        = 0x70,
        op_i32_and          = 0x71,
        op_i32_or           = 0x72,
        op_i32_xor          = 0x73,
        op_i32_shl          = 0x74,
        op_i32_shr_s        = 0x75,
        op_i32_shr_u        = 0x76,
        op_i32_rotl         = 0x77,
        op_i32_rotr         = 0x78,

        op_i64_clz          = 0x79,
        op_i64_ctz          = 0x7a,
        op_i64_popcnt       = 0x7b,
        op_i64_add          = 0x7c,
        op_i64_sub          = 0x7d,
        op_i64_mul          = 0x7e,
        op_i64_div_s        = 0x7f,
        op_i64_div_u        = 0x80,
        op_i64_rem_s        = 0x81,
        op_i64_rem_u        = 0x82,
        op_i64_and          = 0x83,
        op_i64_or           = 0x84,
        op_i64_xor          = 0x85,
        op_i64_shl          = 0x86,
        op_i64_shr_s        = 0x87,
        op_i64_shr_u        = 0x88,
        op_i64_rotl         = 0x89,
        op_i64_rotr         = 0x8a,

        op_f32_abs          = 0x8b,
        op_f32_neg          = 0x8c,
        op_f32_ceil         = 0x8d,
        op_f32_floor        = 0x8e,
        op_f32_trunc        = 0x8f,
        op_f32_nearest      = 0x90,
        op_f32_sqrt         = 0x91,
        op_f32_add          = 0x92,
        op_f32_sub          = 0x93,
        op_f32_mul          = 0x94,
        op_f32_div          = 0x95,
        op_f32_min          = 0x96,
        op_f32_max          = 0x97,
        op_f32_copysign     = 0x98,

        op_f64_abs          = 0x99,
        op_f64_neg          = 0x9a,
        op_f64_ceil         = 0x9b,
        op_f64_floor        = 0x9c,
        op_f64_trunc        = 0x9d,
        op_f64_nearest      = 0x9e,
        op_f64_sqrt         = 0x9f,
        op_f64_add          = 0xa0,
        op_f64_sub          = 0xa1,
        op_f64_mul          = 0xa2,
        op_f64_div          = 0xa3,
        op_f64_min          = 0xa4,
        op_f64_max          = 0xa5,
        op_f64_copysign     = 0xa6,

        op_i32_wrap_i64     = 0xa7,
        op_i32_trunc_f32_s  = 0xa8,
        op_i32_trunc_f32_u  = 0xa9,
        op_i32_trunc_f64_s  = 0xaa,
        op_i32_trunc_f64_u  = 0xab,

        op_i64_extend_i32_s = 0xac,
        op_i64_extend_i32_u = 0xad,
        op_i64_trunc_f32_s  = 0xae,
        op_i64_trunc_f32_u  = 0xaf,
        op_i64_trunc_f64_s  = 0xb0,
        op_i64_trunc_f64_u  = 0xb1,

        op_f32_convert_i32_s = 0xb2,
        op_f32_convert_i32_u = 0xb3,
        op_f32_convert_i64_s = 0xb4,
        op_f32_convert_i64_u = 0xb5,
        op_f32_demote_f64    = 0xb6,

        op_f64_convert_i32_s = 0xb7,
        op_f64_convert_i32_u = 0xb8,
        op_f64_convert_i64_s = 0xb9,
        op_f64_convert_i64_u = 0xba,
        op_f64_promote_f32   = 0xbb,

        op_i32_reinterpret_f32 = 0xbc,
        op_i64_reinterpret_f64 = 0xbd,
        op_f32_reinterpret_i32 = 0xbe,
        op_f64_reinterpret_i64 = 0xbf,

        op_i32_extend8_s    = 0xc0,
        op_i32_extend16_s   = 0xc1,
        op_i64_extend8_s    = 0xc2,
        op_i64_extend16_s   = 0xc3,
        op_i64_extend32_s   = 0xc4,

        op_ref_null    = 0xd0,
        op_ref_is_null = 0xd1,
        op_ref_func    = 0xd2,
    };

    enum struct wasm_opcode_ex {
        op_fc_i32_trunc_sat_f32_s = 0xfc00,
        op_fc_i32_trunc_sat_f32_u = 0xfc01,
        op_fc_i32_trunc_sat_f64_s = 0xfc02,
        op_fc_i32_trunc_sat_f64_u = 0xfc03,
        op_fc_i64_trunc_sat_f32_s = 0xfc04,
        op_fc_i64_trunc_sat_f32_u = 0xfc05,
        op_fc_i64_trunc_sat_f64_s = 0xfc06,
        op_fc_i64_trunc_sat_f64_u = 0xfc07,

        op_memory_init = 0xfc08,
        op_data_drop   = 0xfc09,
        op_memory_copy = 0xfc0a,
        op_memory_fill = 0xfc0b,
        op_table_init  = 0xfc0c,
        op_elem_drop   = 0xfc0d,
        op_table_copy  = 0xfc0e,
        op_table_grow  = 0xfc0f,
        op_table_size  = 0xfc10,
        op_table_fill  = 0xfc11,
    };


    using enum wasm_opcode;
    using enum wasm_opcode_ex;
    using enum wasm_section_id;
    using enum wasm_export_kind;

    wasm_emitter make_emitter(arena& arena) { return wasm_emit::wasm_emitter {
        .arena   = arena,
        .wasm    = make_stream(1024, arena),
        .section = make_stream(1024, arena),
    };}

    void emit(u8 byte, stream& dst) { push(dst, byte); }

    void emit(uint n, stream& dst) {
        do {
            var byte = (u8)(n & 0x7f);
            n >>= 7;
            if (n != 0) {
                byte |= 0x80;
            }
            push(dst, byte);
        } while (n != 0);
    }

    void emit(int n, stream& dst) {
        var more = true;
        while (more) {
            var byte = (u8)(n & 0x7f);
            n >>= 7;
            if ((n == 0 && (byte & 0x40) == 0) || (n == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            push(dst, byte);
        }
    }

    void emit(size_t n, stream& dst) { emit((uint)n, dst); }
    void emit(float  f, stream& dst) { push(dst, {(u8*)&f, 4}); }

    void emit(wasm_export_kind kind, stream& dst) { push(dst, (u8)kind); }
    void emit(wasm_section_id    id, stream& dst) { push(dst, (u8)id); }
    void emit(wasm_value_type  type, stream& dst) { push(dst, (u8)type); }
    void emit(wasm_opcode        op, stream& dst) { push(dst, (u8)op); }
    void emit(wasm_opcode_ex     op, stream& dst) { push(dst, (u8)((u16)op >> 8)); push(dst, (u8)((u16)op & 0xff)); }

    void emit(wasm_opcode op, uint value, stream& dst) {
        emit((u8)op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, int value, stream& dst) {
        emit((u8)op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, u8 value, stream& dst) {
        emit((u8)op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, float value, stream& dst) {
        emit((u8)op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, uint value0, uint value1, stream& dst) {
        emit((u8)op, dst);
        emit(value0, dst);
        emit(value1, dst);
    }

    void emit(wasm_opcode op, int v0, int v1, stream& dst) {
        emit((u8)op, dst);
        emit(v0, dst);
        emit(v1, dst);
    }

    void emit(wasm_opcode_ex op, u8 v0, u8 v1, stream& dst) {
        emit(op, dst);
        emit(v0, dst); // memory.copy
        emit(v1, dst); // memory.copy dst mem idx
    }

    void emit_const(int value, stream& dst) {
        emit(op_i32_const, value, dst);
    }

    void emit_const(uint value, stream& dst) {
        emit_const((int)value, dst);
    }

    void emit_const(float value, stream& dst) {
        emit(op_f32_const, value, dst);
    }

    void emit_const(size_t value, stream& dst) {
        emit_const((int)value, dst);
    }

    void emit_memcpy(stream& dst) {
        emit(op_memory_copy, 0x00, 0x00, dst); // op, src mem, dst mem
    }

    void emit_while(stream& dst) {
        emit(op_block, vt_void, dst);
        emit(op_loop , vt_void, dst);
    }

    void emit_while_end(stream& dst) {
        emit(op_br, 0, dst);
        emit(op_end, dst);
        emit(op_end, dst);
    }

#define emit_while_scope(dst) emit_while(dst); defer { emit_while_end(dst); }

    void emit_header(wasm_emitter& emitter) {
        static u8 header[] = {
            0x00, 0x61, 0x73, 0x6d,
            0x01, 0x00, 0x00, 0x00
        };

        push(emitter.wasm, {header, sizeof header});
    }

    void emit_type_list(const arr_view<wasm_value_type>& types, stream& dst) {
        emit(types.count, dst);
        for (var i = (size_t)0; i < types.count; ++i) {
            emit(types[i], dst);
        }
    }

    void emit(const string& data, stream& dst) {
        emit(data.count, dst);
        push(dst, {(u8*)data.data, data.count});
    }

    void emit(const char* str, stream& dst) {
        emit(view(str), dst);
    }

    void emit(const arr_view<u8>& data, stream& dst) {
        emit(data.count, dst);
        push(dst, data);
    }

    void emit_section(wasm_section_id id, wasm_emitter& emitter) {
        using_emitter emitter;

        emit (id          , wasm);
        emit (section.data, wasm);
        reset(section);
    }

    void emit_function_type_id(stream& dst) {
        emit((u8)0x60, dst);
    }

    void emit_func_sig(const arr_view<wasm_value_type>& params, const arr_view<wasm_value_type>& returns, stream& dst) {
        emit_function_type_id(dst);
        emit_type_list(params, dst); // params
        emit_type_list(returns, dst); // returns
    }

    void emit_func_sig(std::initializer_list<wasm_value_type> params, std::initializer_list<wasm_value_type> returns, stream& dst) {
        emit_func_sig(view(params), view(returns), dst);
    }

    void emit_type_section(const arr_view<wasm_func_type>& types, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(types.count, section); // num types
        for (var i = (size_t)0; i < types.count; ++i) {
            let type = types[i];
            emit_func_sig(type.params, type.results, section);
        }

        emit_section(sec_type, emitter);
    }

    void emit_func_import(const string& module, const string& name, uint type_index, stream& dst) {
        emit(module, dst);
        emit(name, dst);
        emit(ek_func, dst); // import kind
        emit(type_index, dst); // import type index
    }

    void emit_import_section(const arr_view<wasm_func_import>& func_imports, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(func_imports.count, section); // num func_imports
        for (var i = (size_t)0; i < func_imports.count; ++i) {
            let import = func_imports[i];
            emit_func_import(import.module, import.name, import.type_index, section);
        }

        emit_section(sec_import, emitter);
    }

    void emit_func_section(const arr_view<wasm_func>& funcs, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(funcs.count, section); // num funcs_specs
        for (var i = (size_t)0; i < funcs.count; ++i) {
            let func = funcs[i];
            emit(func.type_index, section);
        }

        emit_section(sec_func, emitter);
    }

    void emit_mem(uint min, uint max, stream& dst) {
        emit((u8)0x01, dst); // type: limited
        emit(min, dst); // min
        emit(max, dst); // max
    }

    void emit_memory_section(const wasm_memory& mem, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(1, section); // num mems
        emit_mem(mem.min, mem.max, section);

        emit_section(sec_memory, emitter);
    }

    void emit_export_section(const arr_view<wasm_export>& exports, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(exports.count, section);

        for (var i = (size_t)0; i < exports.count; ++i) {
            let export_ = exports[i];
            emit(export_.name, section);
            emit((u8)export_.kind , section); // export kind
            emit(export_.obj_index, section); // object index
        }

        emit_section(sec_export, emitter);
    }

    void emit_code_section(const arr_view<wasm_func>& funcs, wasm_emitter& emitter) {
        using_emitter emitter;
        emit(funcs.count, section); // num funcs_specs
        for (var i = (size_t)0; i < funcs.count; ++i) {
            let func = funcs[i];
            let locals = func.locals;
            let body_wasm = func.body;

            //TODO: wrong if locals size is > 127!!!
            emit(body_wasm.count + 1 + 2 * locals.count, section); // size of the section
            emit(locals.count, section); // num locals
            for (var local_i = 0u; local_i < locals.count; ++local_i) {
                let local = locals[local_i];
                //TODO: group locals
                emit(1, section); // local type count
                emit((u8)local, section); // local type
            }
            push(section, body_wasm);
        }

        emit_section(sec_code, emitter);
    }

    void emit_data_section(const arr_view<wasm_data>& datas, wasm_emitter& emitter) {
        using_emitter emitter;
        emit(datas.count, section); // num wasm_data

        var offset = 0u;

        for (var i = (size_t)0; i < datas.count; ++i) {
            let data = datas[i];

            emit((u8)0x00    , section); // segment flags
            emit(op_i32_const, offset, section);
            emit(op_end      , section);
            emit(data.data   , section);

            offset += data.data.count;
        }

        emit_section(sec_data, emitter);
    }
}

#endif