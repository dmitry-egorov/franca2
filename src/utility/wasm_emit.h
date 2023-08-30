#ifndef FRANCA2_WASM_EMIT_H
#define FRANCA2_WASM_EMIT_H
#include "primitives.h"
#include "arrayds.h"
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
        custom  =  0,
        type    =  1,
        import  =  2,
        func    =  3,
        table   =  4,
        memory  =  5,
        global  =  6,
        export_ =  7,
        start   =  8,
        element =  9,
        code    = 10,
        data    = 11,
        data_count = 12,
    };

    enum wasm_value_type: u8 {
        vt_void = 0x40,
        vt_i32  = 0x7f,
        vt_f32  = 0x7d
    };

    struct wasm_func_type {
        arrayv<wasm_value_type> params;
        arrayv<wasm_value_type> results;
    };
    typedef uint wasm_func_type_index;

    struct wasm_func {
        wasm_func_type_index type_index;
        arrayv<wasm_value_type> locals;
        arrayv<u8> body;
    };

    struct wasm_memory {
        uint min;
        uint max;
    };

    struct wasm_data {
        arrayv<u8> data;
    };

    struct wasm_func_import {
        string module;
        string name;
        wasm_func_type_index type_index;
    };

    enum struct wasm_export_kind: uint {
        func   = 0x00,
        table  = 0x01,
        mem    = 0x02,
        global = 0x03,
    };

    struct wasm_export {
        string name;
        wasm_export_kind kind;
        uint obj_index;
    };

    enum struct wasm_opcode: uint {
        op_block           = 0x02,
        op_loop            = 0x03,
        op_if              = 0x04,
        op_br              = 0x0c,
        op_br_if           = 0x0d,
        op_end             = 0x0b,
        op_return          = 0x0f,
        op_call            = 0x10,
        op_drop            = 0x1a,
        op_local_get       = 0x20,
        op_local_set       = 0x21,
        op_i32_store_8     = 0x3a,
        op_i32_const       = 0x41,
        op_f32_const       = 0x43,
        op_i32_eqz         = 0x45,
        op_i32_eq          = 0x46,
        op_i32_ne          = 0x47,
        op_i32_lt_s        = 0x48,
        op_i32_lt_u        = 0x49,
        op_i32_gt_s        = 0x4a,
        op_i32_gt_u        = 0x4b,
        op_i32_le_s        = 0x4c,
        op_i32_le_u        = 0x4d,
        op_i32_ge_s        = 0x4e,
        op_i32_ge_u        = 0x4f,
        op_f32_eq          = 0x5b,
        op_f32_lt          = 0x5d,
        op_f32_gt          = 0x5e,
        op_i32_add         = 0x6a,
        op_i32_sub         = 0x6b,
        op_i32_div_s       = 0x6d,
        op_i32_div_u       = 0x6e,
        op_i32_rem_s       = 0x6f,
        op_i32_rem_u       = 0x70,
        op_i32_and         = 0x71,
        op_f32_add         = 0x92,
        op_f32_sub         = 0x93,
        op_f32_mul         = 0x94,
        op_f32_div         = 0x95,
        op_i32_trunc_f32_s = 0xa8
    };

    using enum wasm_opcode;

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

    void emit(wasm_export_kind kind, stream& dst) { push(dst, (u8)kind); }
    void emit(wasm_section_id    id, stream& dst) { push(dst, (u8)id); }
    void emit(wasm_value_type  type, stream& dst) { push(dst, (u8)type); }
    void emit(wasm_opcode        op, stream& dst) { push(dst, (u8)op); }

    void emit(wasm_opcode op, uint value, stream& dst) {
        push(dst, (u8)op);
        emit(value, dst);
    }

    void emit(wasm_opcode op, int value, stream& dst) {
        push(dst, (u8)op);
        emit(value, dst);
    }

    void emit(wasm_opcode op, u8 value, stream& dst) {
        push(dst, (u8)op);
        emit(value, dst);
    }

    void emit(wasm_opcode op, uint value0, uint value1, stream& dst) {
        push(dst, (u8)op);
        emit(value0, dst);
        emit(value1, dst);
    }

    void emit(wasm_opcode op, int value0, int value1, stream& dst) {
        push(dst, (u8)op);
        emit(value0, dst);
        emit(value1, dst);
    }

    void emit_const(int value, stream& dst) {
        emit(op_i32_const, value, dst);
    }

    void emit_const(uint value, stream& dst) {
        emit_const((int)value, dst);
    }

    void emit_const(size_t value, stream& dst) {
        emit_const((int)value, dst);
    }

    void emit_memcpy(stream& dst) {
        emit((u8)0xfc, dst); // prefix
        emit((u8)0x0a, dst); // memory.copy
        emit((u8)0x00, dst); // memory.copy src mem idx
        emit((u8)0x00, dst); // memory.copy dst mem idx
    }

    void emit_block(wasm_value_type type, stream& dst) {
        emit((u8)0x02, dst); // block
        emit((u8)type, dst); // type
    }

    void emit_loop(stream& dst) {
        emit((u8)0x03, dst); // loop
        emit((u8)0x40, dst); // void
    }

    void emit_while(stream& dst) {
        emit_block(vt_void, dst);
        emit_loop (dst);
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

    void emit_type_list(const arrayv<wasm_value_type>& types, stream& dst) {
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

    void emit(const arrayv<u8>& data, stream& dst) {
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
        push(dst, (u8)0x60);
    }

    void emit_func_sig(const arrayv<wasm_value_type>& params, const arrayv<wasm_value_type>& returns, stream& dst) {
        emit_function_type_id(dst);
        emit_type_list(params, dst); // params
        emit_type_list(returns, dst); // returns
    }

    void emit_func_sig(std::initializer_list<wasm_value_type> params, std::initializer_list<wasm_value_type> returns, stream& dst) {
        emit_func_sig(view_of(params), view_of(returns), dst);
    }

    void emit_type_section(const arrayv<wasm_func_type>& types, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(types.count, section); // num types
        for (var i = (size_t)0; i < types.count; ++i) {
            let type = types[i];
            emit_func_sig(type.params, type.results, section);
        }

        emit_section(wasm_section_id::type, emitter);
    }

    void emit_func_import(const string& module, const string& name, uint type_index, stream& dst) {
        emit(module, dst);
        emit(name, dst);
        emit(wasm_export_kind::func, dst); // import kind
        emit(type_index, dst); // import type index
    }

    void emit_import_section(const arrayv<wasm_func_import>& func_imports, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(func_imports.count, section); // num func_imports
        for (var i = (size_t)0; i < func_imports.count; ++i) {
            let import = func_imports[i];
            emit_func_import(import.module, import.name, import.type_index, section);
        }

        emit_section(wasm_section_id::import, emitter);
    }

    void emit_func_section(const arrayv<wasm_func>& funcs, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(funcs.count, section); // num funcs_specs
        for (var i = (size_t)0; i < funcs.count; ++i) {
            let func = funcs[i];
            emit(func.type_index, section);
        }

        emit_section(wasm_section_id::func, emitter);
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

        emit_section(wasm_section_id::memory, emitter);
    }

    void emit_export_section(const arrayv<wasm_export>& exports, wasm_emitter& emitter) {
        using_emitter emitter;

        emit(exports.count, section);

        for (var i = (size_t)0; i < exports.count; ++i) {
            let export_ = exports[i];
            emit(export_.name, section);
            emit((u8)export_.kind , section); // export kind
            emit(export_.obj_index, section); // object index
        }

        emit_section(wasm_section_id::export_, emitter);
    }

    void emit_code_section(const arrayv<wasm_func>& funcs, wasm_emitter& emitter) {
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

        emit_section(wasm_section_id::code, emitter);
    }

    void emit_data_section(const arrayv<wasm_data>& datas, wasm_emitter& emitter) {
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

        emit_section(wasm_section_id::data, emitter);
    }
}

#endif