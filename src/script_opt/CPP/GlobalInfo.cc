// See the file "COPYING" in the main distribution directory for copyright.

#include "zeek/ZeekString.h"
#include "zeek/Desc.h"
#include "zeek/RE.h"
#include "zeek/script_opt/CPP/Compile.h"

using namespace std;

namespace zeek::detail
	{

string CPP_GlobalsInfo::Name(int index) const
	{
	return base_name + "[" + Fmt(index) + "]";
	}

void CPP_GlobalsInfo::AddInstance(shared_ptr<CPP_GlobalInfo> g)
	{
	auto init_cohort = g->InitCohort();

	if ( static_cast<int>(instances.size()) <= init_cohort )
		instances.resize(init_cohort + 1);

	g->SetOffset(this, size++);

	instances[init_cohort].push_back(move(g));
	}

string CPP_GlobalsInfo::Declare() const
	{
	return string("std::vector<") + CPPType() + "> " + base_name + ";";
	}

void CPP_GlobalsInfo::GenerateInitializers(CPPCompile* c)
	{
	c->NL();

	auto gt = GlobalsType();

	c->Emit("%s %s = %s(%s, %s,", gt, InitializersName(), gt, base_name, Fmt(offset_set));

	c->IndentUp();
	c->Emit("{");

	for ( auto& cohort : instances )
		{
		c->Emit("{");
		BuildCohort(c, cohort);
		c->Emit("},");
		}

	c->Emit("}");
	c->IndentDown();
	c->Emit(");");
	}

void CPP_GlobalsInfo::BuildCohort(CPPCompile* c, std::vector<std::shared_ptr<CPP_GlobalInfo>>& cohort)
	{
	for ( auto& co : cohort )
		{
		vector<string> ivs;
		co->InitializerVals(ivs);

		string full_init = Fmt(co->Offset());
		if ( ! ivs.empty() )
			{
			for ( auto& iv : ivs )
				full_init += string(", ") + iv;
			}

		c->Emit("std::make_shared<%s>(%s),", co->InitializerType(), full_init);
		}
	}


void CPP_BasicConstGlobalsInfo::GenerateInitializers(CPPCompile* c)
	{
	vector<int> offsets_vec;

	for ( auto& cohort : instances )
		{
		vector<int> offsets;
		offsets.reserve(cohort.size());
		for ( auto& co : cohort )
			offsets.push_back(co->Offset());

		offsets_vec.push_back(c->IndMgr().AddIndices(offsets));
		}

	offset_set = c->IndMgr().AddIndices(offsets_vec);

	CPP_GlobalsInfo::GenerateInitializers(c);
	}

void CPP_BasicConstGlobalsInfo::BuildCohort(CPPCompile* c, std::vector<std::shared_ptr<CPP_GlobalInfo>>& cohort)
	{
	for ( auto& co : cohort )
		{
		vector<string> ivs;
		co->InitializerVals(ivs);
		ASSERT(ivs.size() == 1);
		c->Emit(ivs[0] + ",");
		}
	}


string CPP_GlobalInfo::ValElem(CPPCompile* c, ValPtr v)
	{
	string init_type;
	string init_args;

	if ( v )
		{
		auto gi = c->RegisterConstant(v);
		init_cohort = max(init_cohort, gi->InitCohort() + 1);
		auto gl = gi->MainGlobal();

		init_type = string("CPP_ValElem<") + gl->CPPType() + ">";
		init_args = gl->GlobalsName() + ", " + Fmt(gi->Offset());
		}
	else
		{
		init_type = string("CPP_AbstractValElem");
		// default empty init_args works fine.
		}

	return string("std::make_shared<") + init_type + ">(" + init_args + ")";
	}

DescConstInfo::DescConstInfo(string _name, ValPtr v)
	: CPP_GlobalInfo(), name(move(_name))
	{
	ODesc d;
	v->Describe(&d);
	init = string("\"") + d.Description() + "\"";
	}

string DescConstInfo::InitializerType() const
	{
	return string("CPP_BasicConst<") + name + "ValPtr, const char*, " + name + "Val>";
	}

EnumConstInfo::EnumConstInfo(CPPCompile* c, ValPtr v)
	{
	auto ev = v->AsEnumVal();
	auto& ev_t = ev->GetType();
	e_type = c->TypeOffset(ev_t);
	init_cohort = c->TypeCohort(ev_t) + 1;
	e_val = v->AsEnum();
	}

StringConstInfo::StringConstInfo(ValPtr v)
	: CPP_GlobalInfo()
	{
	auto s = v->AsString();
	const char* b = (const char*)(s->Bytes());

	len = s->Len();
	rep = CPPEscape(b, len);
	}

PatternConstInfo::PatternConstInfo(ValPtr v)
	: CPP_GlobalInfo()
	{
	auto re = v->AsPatternVal()->Get();
	pattern = CPPEscape(re->OrigText());
	is_case_insensitive = re->IsCaseInsensitive();
	}

CompoundConstInfo::CompoundConstInfo(CPPCompile* _c, ValPtr v)
	: CPP_GlobalInfo(), c(_c)
	{
	auto& t = v->GetType();
	type = c->TypeOffset(t);
	init_cohort = c->TypeCohort(t) + 1;
	}

ListConstInfo::ListConstInfo(CPPCompile* _c, ValPtr v)
	: CompoundConstInfo(_c)
	{
	auto lv = cast_intrusive<ListVal>(v);
	auto n = lv->Length();

	for ( auto i = 0; i < n; ++i )
		vals += ValElem(c, lv->Idx(i)) + ", ";
	}

VectorConstInfo::VectorConstInfo(CPPCompile* c, ValPtr v)
	: CompoundConstInfo(c, v)
	{
	auto vv = cast_intrusive<VectorVal>(v);
	auto n = vv->Size();

	for ( auto i = 0; i < n; ++i )
		vals += ValElem(c, vv->ValAt(i)) + ", ";
	}

RecordConstInfo::RecordConstInfo(CPPCompile* c, ValPtr v)
	: CompoundConstInfo(c, v)
	{
	auto r = cast_intrusive<RecordVal>(v);
	auto n = r->NumFields();

	type = c->TypeOffset(r->GetType());

	for ( auto i = 0; i < n; ++i )
		vals += ValElem(c, r->GetField(i)) + ", ";
	}

TableConstInfo::TableConstInfo(CPPCompile* c, ValPtr v)
	: CompoundConstInfo(c, v)
	{
	auto tv = cast_intrusive<TableVal>(v);

	for ( auto& tv_i : tv->ToMap() )
		{
		indices += ValElem(c, tv_i.first) + ", ";
		vals += ValElem(c, tv_i.second) + ", ";
		}
	}

FuncConstInfo::FuncConstInfo(CPPCompile* _c, ValPtr v)
	: CompoundConstInfo(_c, v), fv(v->AsFuncVal())
	{
	// This is slightly hacky.  There's a chance that this constant
	// depends on a lambda being registered.  Here we use the knowledge
	// that LambdaRegistrationInfo sets its cohort to 1 more than
	// the function type, so we can ensure any possible lambda has
	// been registered by setting ours to 2 more.  CompoundConstInfo
	// has already set our cohort to 1 more.
	++init_cohort;
	}

void FuncConstInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	auto f = fv->AsFunc();
	const auto& fn = f->Name();

	string hashes;

	if ( ! c->NotFullyCompilable(fn) )
		{
		const auto& bodies = f->GetBodies();

		for ( const auto& b : bodies )
			hashes += Fmt(c->BodyHash(b.stmts.get())) + ", ";
		}

	ivs.emplace_back(string("\"") + fn + "\"");
	ivs.emplace_back(Fmt(type));
	ivs.emplace_back(string("std::vector<p_hash_type>({ ") + hashes + "})");
	}


AttrInfo::AttrInfo(CPPCompile* c, const AttrPtr& attr)
	: CPP_GlobalInfo()
	{
	tag = c->AttrName(attr->Tag());
	auto a_e = attr->GetExpr();

	if ( a_e )
		{
		auto gi = c->RegisterType(a_e->GetType());
		init_cohort = max(init_cohort, gi->InitCohort() + 1);

		auto expr_type = gi->Name();

		if ( ! CPPCompile::IsSimpleInitExpr(a_e) )
			{
			gi = c->RegisterInitExpr(a_e);
			init_cohort = max(init_cohort, gi->InitCohort() + 1);
			e_init_type = "CPP_CallAttrExpr";
			e_init_args = Fmt(gi->Offset());
			}

		else if ( a_e->Tag() == EXPR_CONST )
			{
			e_init_type = "CPP_ConstAttrExpr";
			e_init_args = ValElem(c, a_e->AsConstExpr()->ValuePtr());
			}

		else if ( a_e->Tag() == EXPR_NAME )
			{
			auto g = a_e->AsNameExpr()->Id();
			auto gi = c->RegisterGlobal(g);
			init_cohort = max(init_cohort, gi->InitCohort() + 1);

			e_init_type = "CPP_NameAttrExpr";
			e_init_args = c->GlobalName(a_e);
			}

		else
			{
			ASSERT(a_e->Tag() == EXPR_RECORD_COERCE);
			e_init_type = "CPP_RecordAttrExpr";
			e_init_args = gi->Name();
			}
		}

	else
		e_init_type = "CPP_AbstractAttrExpr";
	}

AttrsInfo::AttrsInfo(CPPCompile* c, const AttributesPtr& _attrs)
	: CPP_GlobalInfo()
	{
	for ( const auto& a : _attrs->GetAttrs() )
		{
		ASSERT(c->processed_attr.count(a.get()) > 0);
		auto gi = c->processed_attr[a.get()];
		init_cohort = max(init_cohort, gi->InitCohort() + 1);
		attrs.push_back(gi->Offset());
		}
	}

void AttrsInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	string attr_list;

	for ( auto a : attrs )
		attr_list += Fmt(a) + ", ";

	ivs.emplace_back(string("std::vector<int>({ ") + attr_list + "})");
	}

GlobalInitInfo::GlobalInitInfo(CPPCompile* c, const ID* g, string _CPP_name)
	: CPP_GlobalInfo(), CPP_name(move(_CPP_name))
	{
	Zeek_name = g->Name();

	auto gi = c->RegisterType(g->GetType());
	init_cohort = max(init_cohort, gi->InitCohort() + 1);
	type = gi->Offset();

	gi = c->RegisterAttributes(g->GetAttrs());
	if ( gi )
		{
		init_cohort = max(init_cohort, gi->InitCohort() + 1);
		attrs = gi->Offset();
		}
	else
		attrs = -1;

	exported = g->IsExport();

	val = ValElem(c, g->GetVal());
	}

void GlobalInitInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.push_back(CPP_name);
	ivs.push_back(string("\"") + Zeek_name + "\"");
	ivs.push_back(Fmt(type));
	ivs.push_back(Fmt(attrs));
	ivs.push_back(val);
	ivs.push_back(Fmt(exported));
	}


CallExprInitInfo::CallExprInitInfo(CPPCompile* c, ExprPtr _e, string _e_name, string _wrapper_class)
	: e(move(_e)), e_name(move(_e_name)), wrapper_class(move(_wrapper_class))
	{
	auto gi = c->RegisterType(e->GetType());
	init_cohort = max(init_cohort, gi->InitCohort() + 1);
	}


LambdaRegistrationInfo::LambdaRegistrationInfo(CPPCompile* c, string _name, FuncTypePtr ft, string _wrapper_class, p_hash_type _h, bool _has_captures)
	: name(move(_name)), wrapper_class(move(_wrapper_class)), h(_h), has_captures(_has_captures)
	{
	auto gi = c->RegisterType(ft);
	init_cohort = max(init_cohort, gi->InitCohort() + 1);
	func_type = gi->Offset();
	}

void LambdaRegistrationInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.emplace_back(string("\"") + name + "\"");
	ivs.emplace_back(Fmt(func_type));
	ivs.emplace_back(Fmt(h));
	ivs.emplace_back(has_captures ? "true" : "false");
	}

void BaseTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.emplace_back(CPPCompile::TypeTagName(t->Tag()));
	}

void EnumTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	string elem_list, val_list;
	auto et = t->AsEnumType();

	for ( const auto& name_pair : et->Names() )
		{
		elem_list += string("\"") + name_pair.first + "\", ";
		val_list += Fmt(int(name_pair.second)) + ", ";
		}

	ivs.emplace_back(string("\"") + t->GetName() + "\"");
	ivs.emplace_back(string("std::vector<const char*>({ ") + elem_list + "})");
	ivs.emplace_back(string("std::vector<int>({ ") + val_list + "})");
	}


TypeTypeInfo::TypeTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t))
	{
	tt = t->AsTypeType()->GetType();
	auto gi = c->RegisterType(tt);
	if ( gi )
		init_cohort = gi->InitCohort();
	}

void TypeTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.emplace_back(to_string(c->TypeOffset(tt)));
	}

void VectorTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.emplace_back(to_string(c->TypeOffset(yield)));
	}

VectorTypeInfo::VectorTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t))
	{
	yield = t->Yield();
	auto gi = c->RegisterType(yield);
	if ( gi )
		init_cohort = gi->InitCohort();
	}

ListTypeInfo::ListTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t)), types(t->AsTypeList()->GetTypes())
	{
	for ( auto& tl_i : types )
		{
		auto gi = c->RegisterType(tl_i);
		if ( gi )
			init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

void ListTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	string type_list;
	for ( auto& t : types )
		type_list += Fmt(c->TypeOffset(t)) + ", ";

	ivs.emplace_back(string("std::vector<int>({ ") + type_list + "})");
	}

TableTypeInfo::TableTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t))
	{
	auto tbl = t->AsTableType();

	auto gi = c->RegisterType(tbl->GetIndices());
	ASSERT(gi);
	indices = gi->Offset();
	init_cohort = gi->InitCohort();

	yield = tbl->Yield();

	if ( yield )
		{
		gi = c->RegisterType(yield);
		if ( gi )
			init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

void TableTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	ivs.emplace_back(Fmt(indices));
	ivs.emplace_back(Fmt(yield ? c->TypeOffset(yield) : -1));
	}

FuncTypeInfo::FuncTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t))
	{
	auto f = t->AsFuncType();

	flavor = f->Flavor();
	params = f->Params();
	yield = f->Yield();

	auto gi = c->RegisterType(f->Params());
	if ( gi )
		init_cohort = gi->InitCohort();

	if ( yield )
		{
		gi = c->RegisterType(f->Yield());
		if ( gi )
			init_cohort = max(init_cohort, gi->InitCohort());
		}
	}

void FuncTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	string fl_name;
	if ( flavor == FUNC_FLAVOR_FUNCTION )
		fl_name = "FUNC_FLAVOR_FUNCTION";
	else if ( flavor == FUNC_FLAVOR_EVENT )
		fl_name = "FUNC_FLAVOR_EVENT";
	else if ( flavor == FUNC_FLAVOR_HOOK )
		fl_name = "FUNC_FLAVOR_HOOK";

	ivs.emplace_back(Fmt(c->TypeOffset(params)));
	ivs.emplace_back(Fmt(yield ? c->TypeOffset(yield) : -1));
	ivs.emplace_back(fl_name);
	}

RecordTypeInfo::RecordTypeInfo(CPPCompile* _c, TypePtr _t)
	: CompoundTypeInfo(_c, move(_t))
	{
	auto r = t->AsRecordType()->Types();

	if ( ! r )
		return;

	for ( const auto& r_i : *r )
		{
		field_names.emplace_back(r_i->id);

		auto gi = c->RegisterType(r_i->type);
		if ( gi )
			init_cohort = max(init_cohort, gi->InitCohort());
		// else it's a recursive type, no need to adjust cohort here

		field_types.push_back(r_i->type);

		if ( r_i->attrs )
			{
			gi = c->RegisterAttributes(r_i->attrs);
			init_cohort = max(init_cohort, gi->InitCohort() + 1);
			field_attrs.push_back(gi->Offset());
			}
		else
			field_attrs.push_back(-1);
		}
	}

void RecordTypeInfo::InitializerVals(std::vector<std::string>& ivs) const
	{
	string names, types, attrs;

	for ( auto& n : field_names )
		names += string("\"") + n + "\", ";

	for ( auto& t : field_types )
		{
		// Because RecordType's can be recursively defined,
		// during construction we couldn't reliably access
		// the field type's offsets.  At this point, though,
		// they should all be available.
		types += Fmt(c->TypeOffset(t)) + ", ";
		}

	for ( auto& a : field_attrs )
		attrs += Fmt(a) + ", ";

	ivs.emplace_back(string("std::vector<const char*>({ ") + names + "})");
	ivs.emplace_back(string("std::vector<int>({ ") + types + "})");
	ivs.emplace_back(string("std::vector<int>({ ") + attrs + "})");
	}


void IndicesManager::Generate(CPPCompile* c)
	{
	c->Emit("int CPP__Indices__init[] =");
	c->StartBlock();

	int nset = 0;
	for ( auto& is : indices_set )
		{
		auto line = string("/* ") + to_string(nset++) + " */ ";
		line += to_string(is.size()) + ", ";

		auto n = 1;
		for ( auto i : is )
			{
			line += to_string(i) + ", ";
			if ( ++n % 10 == 0 )
				{
				c->Emit(line);
				line.clear();
				}
			}

		if ( line.size() > 0)
			c->Emit(line);
		}

	c->Emit("-1");
	c->EndBlock(true);
	}

	} // zeek::detail
