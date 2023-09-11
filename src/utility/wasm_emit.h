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

    void init();
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

#define WASM_VALUE_TYPES \
    OP(vt_void   , 0x40) \
    OP(vt_f64    , 0x7c) \
    OP(vt_f32    , 0x7d) \
    OP(vt_i64    , 0x7e) \
    OP(vt_i32    , 0x7f) \

    const uint wasm_value_type_count = 5;

#define OP(name, value) name = value,
    enum wasm_type: u8 {
        vt_unknown = 0x00,
        WASM_VALUE_TYPES
        vt_invalid = 0xff,
    };
#undef OP

    struct wasm_func_type {
        arr_view<wasm_type> params;
        arr_view<wasm_type> results;
    };
    typedef uint wasm_func_type_index;

    struct wasm_func {
        wasm_func_type_index type_index;
        arr_view<wasm_type> locals;
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

#define WASM_OPCODES \
    OP(op_unreachable        , 0x0000) \
    OP(op_nop                , 0x0001) \
    OP(op_block              , 0x0002) \
    OP(op_loop               , 0x0003) \
    OP(op_if                 , 0x0004) \
    OP(op_else               , 0x0005) \
                                       \
    OP(op_end                , 0x000b) \
    OP(op_br                 , 0x000c) \
    OP(op_br_if              , 0x000d) \
    OP(op_br_table           , 0x000e) \
    OP(op_return             , 0x000f) \
    OP(op_call               , 0x0010) \
    OP(op_call_indirect      , 0x0011) \
                                       \
    OP(op_drop               , 0x001a) \
    OP(op_select             , 0x001b) \
    OP(op_select_t           , 0x001c) \
                                       \
    OP(op_local_get          , 0x0020) \
    OP(op_local_set          , 0x0021) \
    OP(op_local_tee          , 0x0022) \
    OP(op_global_get         , 0x0023) \
    OP(op_global_set         , 0x0024) \
    OP(op_table_get          , 0x0025) \
    OP(op_table_set          , 0x0026) \
                                       \
    OP(op_i32_load           , 0x0028) \
    OP(op_i64_load           , 0x0029) \
    OP(op_f32_load           , 0x002a) \
    OP(op_f64_load           , 0x002b) \
                                       \
    OP(op_i32_load_8_s       , 0x002c) \
    OP(op_i32_load_8_u       , 0x002d) \
    OP(op_i32_load_16_s      , 0x002e) \
    OP(op_i32_load_16_u      , 0x002f) \
                                       \
    OP(op_i64_load_8_s       , 0x0030) \
    OP(op_i64_load_8_u       , 0x0031) \
    OP(op_i64_load_16_s      , 0x0032) \
    OP(op_i64_load_16_u      , 0x0033) \
    OP(op_i64_load_32_s      , 0x0034) \
    OP(op_i64_load_32_u      , 0x0035) \
                                       \
    OP(op_i32_store          , 0x0036) \
    OP(op_i64_store          , 0x0037) \
    OP(op_f32_store          , 0x0038) \
    OP(op_f64_store          , 0x0039) \
                                       \
    OP(op_i32_store8         , 0x003a) \
    OP(op_i32_store16        , 0x003b) \
    OP(op_i64_store8         , 0x003c) \
    OP(op_i64_store16        , 0x003d) \
    OP(op_i64_store32        , 0x003e) \
                                       \
    OP(op_memory_size        , 0x003f) \
    OP(op_memory_grow        , 0x0040) \
                                       \
    OP(op_i32_const          , 0x0041) \
    OP(op_i64_const          , 0x0042) \
    OP(op_f32_const          , 0x0043) \
    OP(op_f64_const          , 0x0044) \
                                       \
    OP(op_i32_eqz            , 0x0045) \
    OP(op_i32_eq             , 0x0046) \
    OP(op_i32_ne             , 0x0047) \
    OP(op_i32_lt_s           , 0x0048) \
    OP(op_i32_lt_u           , 0x0049) \
    OP(op_i32_gt_s           , 0x004a) \
    OP(op_i32_gt_u           , 0x004b) \
    OP(op_i32_le_s           , 0x004c) \
    OP(op_i32_le_u           , 0x004d) \
    OP(op_i32_ge_s           , 0x004e) \
    OP(op_i32_ge_u           , 0x004f) \
                                       \
    OP(op_i64_eqz            , 0x0050) \
    OP(op_i64_eq             , 0x0051) \
    OP(op_i64_ne             , 0x0052) \
    OP(op_i64_lt_s           , 0x0053) \
    OP(op_i64_lt_u           , 0x0054) \
    OP(op_i64_gt_s           , 0x0055) \
    OP(op_i64_gt_u           , 0x0056) \
    OP(op_i64_le_s           , 0x0057) \
    OP(op_i64_le_u           , 0x0058) \
    OP(op_i64_ge_s           , 0x0059) \
    OP(op_i64_ge_u           , 0x005a) \
                                       \
    OP(op_f32_eq             , 0x005b) \
    OP(op_f32_ne             , 0x005c) \
    OP(op_f32_lt             , 0x005d) \
    OP(op_f32_gt             , 0x005e) \
    OP(op_f32_le             , 0x005f) \
    OP(op_f32_ge             , 0x0060) \
                                       \
    OP(op_f64_eq             , 0x0061) \
    OP(op_f64_ne             , 0x0062) \
    OP(op_f64_lt             , 0x0063) \
    OP(op_f64_gt             , 0x0064) \
    OP(op_f64_le             , 0x0065) \
    OP(op_f64_ge             , 0x0066) \
                                       \
    OP(op_i32_clz            , 0x0067) \
    OP(op_i32_ctz            , 0x0068) \
    OP(op_i32_popcnt         , 0x0069) \
    OP(op_i32_add            , 0x006a) \
    OP(op_i32_sub            , 0x006b) \
    OP(op_i32_mul            , 0x006c) \
    OP(op_i32_div_s          , 0x006d) \
    OP(op_i32_div_u          , 0x006e) \
    OP(op_i32_rem_s          , 0x006f) \
    OP(op_i32_rem_u          , 0x0070) \
    OP(op_i32_and            , 0x0071) \
    OP(op_i32_or             , 0x0072) \
    OP(op_i32_xor            , 0x0073) \
    OP(op_i32_shl            , 0x0074) \
    OP(op_i32_shr_s          , 0x0075) \
    OP(op_i32_shr_u          , 0x0076) \
    OP(op_i32_rotl           , 0x0077) \
    OP(op_i32_rotr           , 0x0078) \
                                       \
    OP(op_i64_clz            , 0x0079) \
    OP(op_i64_ctz            , 0x007a) \
    OP(op_i64_popcnt         , 0x007b) \
    OP(op_i64_add            , 0x007c) \
    OP(op_i64_sub            , 0x007d) \
    OP(op_i64_mul            , 0x007e) \
    OP(op_i64_div_s          , 0x007f) \
    OP(op_i64_div_u          , 0x0080) \
    OP(op_i64_rem_s          , 0x0081) \
    OP(op_i64_rem_u          , 0x0082) \
    OP(op_i64_and            , 0x0083) \
    OP(op_i64_or             , 0x0084) \
    OP(op_i64_xor            , 0x0085) \
    OP(op_i64_shl            , 0x0086) \
    OP(op_i64_shr_s          , 0x0087) \
    OP(op_i64_shr_u          , 0x0088) \
    OP(op_i64_rotl           , 0x0089) \
    OP(op_i64_rotr           , 0x008a) \
                                       \
    OP(op_f32_abs            , 0x008b) \
    OP(op_f32_neg            , 0x008c) \
    OP(op_f32_ceil           , 0x008d) \
    OP(op_f32_floor          , 0x008e) \
    OP(op_f32_trunc          , 0x008f) \
    OP(op_f32_nearest        , 0x0090) \
    OP(op_f32_sqrt           , 0x0091) \
    OP(op_f32_add            , 0x0092) \
    OP(op_f32_sub            , 0x0093) \
    OP(op_f32_mul            , 0x0094) \
    OP(op_f32_div            , 0x0095) \
    OP(op_f32_min            , 0x0096) \
    OP(op_f32_max            , 0x0097) \
    OP(op_f32_copysign       , 0x0098) \
                                       \
    OP(op_f64_abs            , 0x0099) \
    OP(op_f64_neg            , 0x009a) \
    OP(op_f64_ceil           , 0x009b) \
    OP(op_f64_floor          , 0x009c) \
    OP(op_f64_trunc          , 0x009d) \
    OP(op_f64_nearest        , 0x009e) \
    OP(op_f64_sqrt           , 0x009f) \
    OP(op_f64_add            , 0x00a0) \
    OP(op_f64_sub            , 0x00a1) \
    OP(op_f64_mul            , 0x00a2) \
    OP(op_f64_div            , 0x00a3) \
    OP(op_f64_min            , 0x00a4) \
    OP(op_f64_max            , 0x00a5) \
    OP(op_f64_copysign       , 0x00a6) \
                                       \
    OP(op_i32_wrap_i64       , 0x00a7) \
    OP(op_i32_trunc_f32_s    , 0x00a8) \
    OP(op_i32_trunc_f32_u    , 0x00a9) \
    OP(op_i32_trunc_f64_s    , 0x00aa) \
    OP(op_i32_trunc_f64_u    , 0x00ab) \
                                       \
    OP(op_i64_extend_i32_s   , 0x00ac) \
    OP(op_i64_extend_i32_u   , 0x00ad) \
    OP(op_i64_trunc_f32_s    , 0x00ae) \
    OP(op_i64_trunc_f32_u    , 0x00af) \
    OP(op_i64_trunc_f64_s    , 0x00b0) \
    OP(op_i64_trunc_f64_u    , 0x00b1) \
                                       \
    OP(op_f32_convert_i32_s  , 0x00b2) \
    OP(op_f32_convert_i32_u  , 0x00b3) \
    OP(op_f32_convert_i64_s  , 0x00b4) \
    OP(op_f32_convert_i64_u  , 0x00b5) \
    OP(op_f32_demote_f64     , 0x00b6) \
                                       \
    OP(op_f64_convert_i32_s  , 0x00b7) \
    OP(op_f64_convert_i32_u  , 0x00b8) \
    OP(op_f64_convert_i64_s  , 0x00b9) \
    OP(op_f64_convert_i64_u  , 0x00ba) \
    OP(op_f64_promote_f32    , 0x00bb) \
                                       \
    OP(op_i32_reinterpret_f32, 0x00bc) \
    OP(op_i64_reinterpret_f64, 0x00bd) \
    OP(op_f32_reinterpret_i32, 0x00be) \
    OP(op_f64_reinterpret_i64, 0x00bf) \
                                       \
    OP(op_i32_extend8_s      , 0x00c0) \
    OP(op_i32_extend16_s     , 0x00c1) \
    OP(op_i64_extend8_s      , 0x00c2) \
    OP(op_i64_extend16_s     , 0x00c3) \
    OP(op_i64_extend32_s     , 0x00c4) \
                                       \
    OP(op_ref_null           , 0x00d0) \
    OP(op_ref_is_null        , 0x00d1) \
    OP(op_ref_func           , 0x00d2)

#define WASM_OPCODES_FC                \
    OP(op_i32_trunc_sat_f32_s, 0xfc00) \
    OP(op_i32_trunc_sat_f32_u, 0xfc01) \
    OP(op_i32_trunc_sat_f64_s, 0xfc02) \
    OP(op_i32_trunc_sat_f64_u, 0xfc03) \
    OP(op_i64_trunc_sat_f32_s, 0xfc04) \
    OP(op_i64_trunc_sat_f32_u, 0xfc05) \
    OP(op_i64_trunc_sat_f64_s, 0xfc06) \
    OP(op_i64_trunc_sat_f64_u, 0xfc07) \
                                       \
    OP(op_memory_init        , 0xfc08) \
    OP(op_data_drop          , 0xfc09) \
    OP(op_memory_copy        , 0xfc0a) \
    OP(op_memory_fill        , 0xfc0b) \
    OP(op_table_init         , 0xfc0c) \
    OP(op_elem_drop          , 0xfc0d) \
    OP(op_table_copy         , 0xfc0e) \
    OP(op_table_grow         , 0xfc0f) \
    OP(op_table_size         , 0xfc10) \
    OP(op_table_fill         , 0xfc11)

#define OP(name, value) name = value,
    enum struct wasm_opcode: u16 {
        WASM_OPCODES
        WASM_OPCODES_FC
        op_invalid = 0xffff,
    };
#undef OP

    using enum wasm_opcode;
    using enum wasm_section_id;
    using enum wasm_export_kind;

    struct wasm_type_mapping {
        string name;
        wasm_type to;
    };

    string opcode_map   [(size_t)0xff];
    string opcode_fc_map[(size_t)0x12];
    wasm_type_mapping wasm_value_type_map[wasm_value_type_count];

    void init() {
        using enum wasm_type;
        var i = 0u;
        #define OP(name, value) wasm_value_type_map[i] = { view(#name), (wasm_type)value }; i++;
            WASM_VALUE_TYPES
        #undef OP
        #define OP(name, value) opcode_map[value] = view(#name);
            WASM_OPCODES
        #undef OP
        #define OP(name, value) opcode_fc_map[value & 0x00ff] = view(#name);
            WASM_OPCODES_FC
        #undef OP
    }

    wasm_emitter make_emitter(arena& arena) { return wasm_emit::wasm_emitter {
        .arena   = arena,
        .wasm    = make_stream(1024, arena),
        .section = make_stream(1024, arena),
    };}

    void emit(u8  byte, stream& dst) { push(dst, byte); }
    void emit(u16 byte, stream& dst) { emit((u8)(byte >> 8), dst); emit((u8)byte, dst); }
    void emit(wasm_opcode op, stream& dst) {
        if ((u16)op < 0x00ff) emit((u8)op, dst); else emit((u16)op, dst);
    }

    wasm_type find_value_type(string op_name) {
        for (var i = 0u; i < wasm_value_type_count; ++i) {
            ref mapping = wasm_value_type_map[i];
            if (mapping.name == op_name) return mapping.to;
        }

        return vt_invalid;
    }

    wasm_opcode find_op(string op_name) {
        for (u8 i = 0; i < (u8)0xff; ++i)
            if (op_name == opcode_map[i])
                return (wasm_opcode)i;

        for (u8 i = 0; i < (u8)0x12; ++i) {
            if (op_name == opcode_fc_map[i])
                return (wasm_opcode)(i + 0xfc00);
        }

        return op_invalid;
    }

    void emit_op(string op_name, stream& dst) {
        let op = find_op(op_name);
        if (op != op_invalid); else { printf("Unknown opcode: %.*s\n", (int)op_name.count, op_name.data); dbg_fail_return;}

        emit(op, dst);
    }

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
    void emit(wasm_type        type, stream& dst) { push(dst, (u8)type); }

    void emit(wasm_opcode op, uint value, stream& dst) {
        emit(op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, int value, stream& dst) {
        emit(op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, u8 value, stream& dst) {
        emit(op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, float value, stream& dst) {
        emit(op, dst);
        emit(value, dst);
    }

    void emit(wasm_opcode op, uint value0, uint value1, stream& dst) {
        emit(op, dst);
        emit(value0, dst);
        emit(value1, dst);
    }

    void emit(wasm_opcode op, int v0, int v1, stream& dst) {
        emit(op, dst);
        emit(v0, dst);
        emit(v1, dst);
    }

    void emit_const_get(int value, stream& dst) {
        emit(op_i32_const, value, dst);
    }

    void emit_const_get(uint value, stream& dst) {
        emit_const_get((int) value, dst);
    }

    void emit_const_get(float value, stream& dst) {
        emit(op_f32_const, value, dst);
    }

    void emit_const_get(size_t value, stream& dst) {
        emit_const_get((int) value, dst);
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

    void emit_type_list(const arr_view<wasm_type>& types, stream& dst) {
        emit(types.count, dst);
        for (var i = (size_t)0; i < types.count; ++i) {
            emit(types[i], dst);
        }
    }

    void emit(const string& data, stream& dst) {
        emit(data.count, dst);
        push(dst, {(u8*)data.data, data.count});
    }

    void emit(cstr str, stream& dst) {
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

    void emit_func_sig(const arr_view<wasm_type>& params, const arr_view<wasm_type>& returns, stream& dst) {
        emit_function_type_id(dst);
        emit_type_list(params, dst); // params
        emit_type_list(returns, dst); // returns
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
            emit(body_wasm.count + 2 + 2 * locals.count, section); // size of the section
            emit(locals.count, section); // num locals
            for (var local_i = 0u; local_i < locals.count; ++local_i) {
                let local = locals[local_i];
                //TODO: group locals
                emit(1, section); // local type count
                emit((u8)local, section); // local type
            }
            push(section, body_wasm);
            push(section, (u8)op_end);
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