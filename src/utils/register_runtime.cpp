#include "register_runtime.h"
//TODO: 
#include <vector>
#include <cstring>

#include "ast.h"

void register_runtime_content(struct ast::Ast& ast) {
	if (ast.object)
		return;
	using mut = ast::Mut;
	ast.object = ast.mk_class("Object");
	ast.object->used = true;
	auto obj_get_hash = ast.mk_method(mut::ANY, ast.object, "getHash", new ast::ConstInt64, {});
	obj_get_hash->used = true;
	auto obj_equals = ast.mk_method(mut::ANY, ast.object, "equals", new ast::ConstBool, { ast.get_conform_ref(ast.object) });
	obj_equals->used = true;
	ast.blob = ast.mk_class("Blob", {
		ast.mk_field("_count", new ast::ConstInt64()),
		ast.mk_field("_bytes", new ast::ConstInt64())  // ptr
	});
	ast.mk_method(mut::ANY, ast.blob, "capacity", new ast::ConstInt64, {});
	ast.mk_method(mut::MUTATING, ast.blob, "insert", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "delete", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "copy", new ast::ConstBool, { ast.tp_int64(), ast.get_conform_ref(ast.blob), ast.tp_int64(), ast.tp_int64() });
	ast.mk_method(mut::ANY, ast.blob, "get8At", new ast::ConstInt32, { ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "set8At", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int32() });
	ast.mk_method(mut::ANY, ast.blob, "get16At", new ast::ConstInt32, { ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "set16At", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int32() });
	ast.mk_method(mut::ANY, ast.blob, "get32At", new ast::ConstInt32, { ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "set32At", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int32() });
	ast.mk_method(mut::ANY, ast.blob, "get64At", new ast::ConstInt64, { ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "set64At", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
	ast.mk_method(mut::MUTATING, ast.blob, "putChAt", new ast::ConstInt64, { ast.tp_int64(), ast.tp_int32() });
	ast.mk_method(mut::MUTATING, ast.blob, "mkStr", new ast::ConstString, { ast.tp_int64(), ast.tp_int64() });

	ast.str_builder = ast.mk_class("StrBuilder");
	ast.str_builder->overloads[ast.blob];

	auto inst = new ast::MkInstance;
	inst->cls = ast.object.pinned();
	auto ref_to_object = new ast::RefOp;
	ref_to_object->p = inst;
	auto opt_ref_to_object = new ast::If;
	opt_ref_to_object->p[0] = new ast::ConstBool;
	opt_ref_to_object->p[1] = ref_to_object;
	auto weak_to_object = new ast::MkWeakOp;
	weak_to_object->p = inst;
	auto add_class_param = [&](ltm::pin<ast::Class> cls, const char* name = "T") {
		auto param = ltm::pin<ast::ClassParam>::make();
		param->index = cls->params.size();
		cls->params.push_back(param);
		param->base = ast.object;
		param->name = name;
		return param;
	};
	auto make_ptr_result = [&](ltm::pin<ast::UnaryOp> typer, ltm::pin<ast::AbstractClass> cls) {
		auto inst_t = new ast::MkInstance;
		inst_t->cls = cls;
		typer->p = inst_t;
		return typer;
	};
	auto make_opt_result = [&](ltm::pin<ast::Action> ref) {
		auto opt_ref_to_t = new ast::If;
		opt_ref_to_t->p[0] = new ast::ConstBool;
		opt_ref_to_t->p[1] = ref;
		return opt_ref_to_t;
	};
	auto make_factory = [&](auto m) {
		m->is_factory = true;
		m->type_expression = new ast::ConstVoid;
	};
	ast.own_array = ast.mk_class("Array", {
		ast.mk_field("_itemsCount", new ast::ConstInt64()),
		ast.mk_field("_items", new ast::ConstInt64())  // ptr
	});
	{
		auto t_cls = add_class_param(ast.own_array);
		auto ref_to_t_res = make_ptr_result(new ast::RefOp, t_cls);
		auto opt_ref_to_t_res = make_opt_result(ref_to_t_res);
		auto own_to_t = ast.get_own(t_cls);
		auto opt_own_to_t = ast.tp_optional(own_to_t);
		ast.mk_method(mut::ANY, ast.own_array, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, ast.own_array, "insert", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.own_array, "delete", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.own_array, "move", new ast::ConstBool, { ast.tp_int64(), ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::ANY, ast.own_array, "getAt", opt_ref_to_t_res, { ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.own_array, "setAt", new ast::ConstVoid, { ast.tp_int64(), own_to_t });
		ast.mk_method(mut::MUTATING, ast.own_array, "setOptAt", opt_ref_to_t_res, { ast.tp_int64(), opt_own_to_t });
		ast.mk_method(mut::MUTATING, ast.own_array, "spliceAt", new ast::ConstBool, { ast.tp_int64(), ast.tp_optional(ast.get_ref(t_cls)) });
	}
	ast.weak_array = ast.mk_class("WeakArray", {
		ast.mk_field("_itemsCount", new ast::ConstInt64()),
		ast.mk_field("_items", new ast::ConstInt64())  // ptr
	});
	{
		auto t_cls = add_class_param(ast.weak_array);
		ast.mk_method(mut::ANY, ast.weak_array, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, ast.weak_array, "insert", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.weak_array, "delete", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.weak_array, "move", new ast::ConstBool, { ast.tp_int64(), ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::ANY, ast.weak_array, "getAt", make_ptr_result(new ast::MkWeakOp, t_cls), { ast.tp_int64() });
		ast.mk_method(mut::MUTATING, ast.weak_array, "setAt", new ast::ConstVoid, { ast.tp_int64(), ast.get_weak(t_cls) });
	}
	{
		auto shared_array_cls = ast.mk_class("SharedArray", {
			ast.mk_field("_itemsCount", new ast::ConstInt64()),
			ast.mk_field("_items", new ast::ConstInt64())  // ptr
		});
		auto t_cls = add_class_param(shared_array_cls);
		ast.mk_method(mut::ANY, shared_array_cls, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, shared_array_cls, "insert", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, shared_array_cls, "delete", new ast::ConstVoid, { ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::MUTATING, shared_array_cls, "move", new ast::ConstBool, { ast.tp_int64(), ast.tp_int64(), ast.tp_int64() });
		ast.mk_method(mut::ANY, shared_array_cls, "getAt", make_opt_result(make_ptr_result(new ast::FreezeOp, t_cls)), { ast.tp_int64() });
		ast.mk_method(mut::MUTATING, shared_array_cls, "setAt", new ast::ConstVoid, { ast.tp_int64(), ast.get_shared(t_cls) });
	}
	ast.string_cls = ast.mk_class("String", {});
	ast.string_cls->used = true;
	ast.mk_overload(ast.string_cls, obj_get_hash);
	ast.mk_overload(ast.string_cls, obj_equals);
	{
		auto cursor_cls = ast.mk_class("Cursor", {
				ast.mk_field("_cursor", new ast::ConstInt64),
				ast.mk_field("_buffer", new ast::ConstString) });
		ast.mk_method(mut::MUTATING, cursor_cls, "getCh", new ast::ConstInt32, {});
		ast.mk_method(mut::ANY, cursor_cls, "peekCh", new ast::ConstInt32, {});
		ast.mk_method(mut::ANY, cursor_cls, "offset", new ast::ConstInt64, {});
		make_factory(ast.mk_method(mut::MUTATING, cursor_cls, "set", nullptr, { ast.get_shared(ast.string_cls) }));
	}
	{
		auto map_cls = ast.mk_class("Map", {
			ast.mk_field("_buckets", new ast::ConstInt64),
			ast.mk_field("_capacity", new ast::ConstInt64),
			ast.mk_field("_size", new ast::ConstInt64) });
		auto key_cls = add_class_param(map_cls, "K");
		auto val_cls = add_class_param(map_cls, "V");
		auto ref_to_val_res = make_ptr_result(new ast::RefOp, val_cls);
		auto opt_ref_to_val_res = make_opt_result(ref_to_val_res);
		auto opt_shared_to_key_res = make_opt_result(
			make_ptr_result(new ast::FreezeOp, key_cls));
		ast.mk_method(mut::ANY, map_cls, "size", new ast::ConstInt64, {});
		ast.mk_method(mut::ANY, map_cls, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, map_cls, "clear", new ast::ConstVoid, {});
		ast.mk_method(mut::MUTATING, map_cls, "delete", opt_ref_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::ANY, map_cls, "getAt", opt_ref_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::MUTATING, map_cls, "setAt", opt_ref_to_val_res, { ast.get_shared(key_cls), ast.get_own(val_cls) });
		ast.mk_method(mut::ANY, map_cls, "keyAt", opt_shared_to_key_res, { ast.tp_int64() });
		ast.mk_method(mut::ANY, map_cls, "valAt", opt_ref_to_val_res, { ast.tp_int64() });
	}
	{
		auto map_cls = ast.mk_class("SharedMap", {
			ast.mk_field("_buckets", new ast::ConstInt64),
			ast.mk_field("_capacity", new ast::ConstInt64),
			ast.mk_field("_size", new ast::ConstInt64) });
		auto key_cls = add_class_param(map_cls, "K");
		auto val_cls = add_class_param(map_cls, "V");
		auto shared_to_val_res = make_ptr_result(new ast::FreezeOp, val_cls);
		auto opt_shared_to_val_res = make_opt_result(shared_to_val_res);
		auto opt_shared_to_key_res = make_opt_result(
			make_ptr_result(new ast::FreezeOp, key_cls));
		ast.mk_method(mut::ANY, map_cls, "size", new ast::ConstInt64, {});
		ast.mk_method(mut::ANY, map_cls, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, map_cls, "clear", new ast::ConstVoid, {});
		ast.mk_method(mut::MUTATING, map_cls, "delete", opt_shared_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::ANY, map_cls, "getAt", opt_shared_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::MUTATING, map_cls, "setAt", opt_shared_to_val_res, { ast.get_shared(key_cls), ast.get_shared(val_cls) });
		ast.mk_method(mut::ANY, map_cls, "keyAt", opt_shared_to_key_res, { ast.tp_int64() });
		ast.mk_method(mut::ANY, map_cls, "valAt", opt_shared_to_val_res, { ast.tp_int64() });
	}
	{
		auto map_cls = ast.mk_class("WeakMap", {
			ast.mk_field("_buckets", new ast::ConstInt64),
			ast.mk_field("_capacity", new ast::ConstInt64),
			ast.mk_field("_size", new ast::ConstInt64) });
		auto key_cls = add_class_param(map_cls, "K");
		auto val_cls = add_class_param(map_cls, "V");
		auto weak_to_val_res = make_ptr_result(new ast::MkWeakOp, val_cls);
		auto opt_shared_to_key_res = make_opt_result(
			make_ptr_result(new ast::FreezeOp, key_cls));
		ast.mk_method(mut::ANY, map_cls, "size", new ast::ConstInt64, {});
		ast.mk_method(mut::ANY, map_cls, "capacity", new ast::ConstInt64, {});
		ast.mk_method(mut::MUTATING, map_cls, "clear", new ast::ConstVoid, {});
		ast.mk_method(mut::MUTATING, map_cls, "delete", weak_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::ANY, map_cls, "getAt", weak_to_val_res, { ast.get_shared(key_cls) });
		ast.mk_method(mut::MUTATING, map_cls, "setAt", weak_to_val_res, { ast.get_shared(key_cls), ast.get_weak(val_cls) });
		ast.mk_method(mut::ANY, map_cls, "keyAt", opt_shared_to_key_res, { ast.tp_int64() });
		ast.mk_method(mut::ANY, map_cls, "valAt", weak_to_val_res, { ast.tp_int64() });
	}
	ast.mk_fn("getParent", opt_ref_to_object, { ast.get_ref(ast.object) });
	ast.mk_fn("log", new ast::ConstVoid, { ast.get_conform_ref(ast.string_cls) });
	ast.mk_fn("hash", new ast::ConstInt64, { ast.get_shared(ast.object) });
	ast.mk_fn("nowMs", new ast::ConstInt64, {});
	ast.mk_fn("terminate", new ast::Break, { ast.tp_int64() });
	ast.mk_fn("setMainObject", new ast::ConstVoid, { ast.tp_optional(ast.get_ref(ast.object))});
	ast.mk_fn("weakExists", new ast::ConstBool, { ast.get_weak(ast.object) });
	ast.mk_fn("powDbl", new ast::ConstDouble, { ast.tp_double(), ast.tp_double() });
	ast.mk_fn("log10Dbl", new ast::ConstDouble, { ast.tp_double() });
	ast.mk_fn("postTimer", new ast::ConstVoid, {
		ast.tp_int64(),
		ast.tp_delegate({ ast.tp_void() })
	});
	{
		auto thread = ast.mk_class("Thread", {
			ast.mk_field("_internal", new ast::ConstInt64) });
		auto param = add_class_param(thread, "R");
		auto start = ast.mk_method(mut::MUTATING, thread, "start", nullptr, { ast.get_ref(param) });
		make_factory(start);
		ast.mk_method(mut::MUTATING, thread, "root", make_ptr_result(new ast::MkWeakOp, param), {});
	}
}
