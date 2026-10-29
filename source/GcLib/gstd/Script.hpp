/* ---------------------------------- */
/* 2004..2005 YT、mkm(本体担当ですよ)*/
/* このソースコードは煮るなり焼くなり好きにしろライセンスの元で配布します。*/
/* A-3、A-4に従い、このソースを組み込んだ.exeにはライセンスは適用されません。*/
/* ---------------------------------- */
/* NYSL Version 0.9982 */
/* A. 本ソフトウェアは Everyone'sWare です。このソフトを手にした一人一人が、*/
/*    ご自分の作ったものを扱うのと同じように、自由に利用することが出来ます。*/
/* A-1. フリーウェアです。作者からは使用料等を要求しません。*/
/* A-2. 有料無料や媒体の如何を問わず、自由に転載・再配布できます。*/
/* A-3. いかなる種類の 改変・他プログラムでの利用 を行っても構いません。*/
/* A-4. 変更したものや部分的に使用したものは、あなたのものになります。*/
/*      公開する場合は、あなたの名前の下で行って下さい。*/
/* B. このソフトを利用することによって生じた損害等について、作者は */
/*    責任を負わないものとします。各自の責任においてご利用下さい。*/
/* C. 著作者人格権は ○○○○ に帰属します。著作権は放棄します。*/
/* D. 以上の３項は、ソース・実行バイナリの双方に適用されます。 */
/* ---------------------------------- */

#ifndef __SCRIPT_H__
#define __SCRIPT_H__
#include "ScriptReal.hpp"
#include <cassert>
#include <utility>
#ifdef __EMSCRIPTEN__
#include "BoundedAllocationCache.hpp"
#endif
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_SCALARS)
#define PH3_SCRIPT_INLINE_SCALARS 1
#endif
#if defined(PH3_SCRIPT_INLINE_SCALARS) && defined(PH3_ENABLE_SCRIPT_STRINGS) && !defined(PH3_LEGACY_SCRIPT_STRINGS)
#define PH3_SCRIPT_STRING_CACHE 1
#endif
#if defined(PH3_SCRIPT_INLINE_SCALARS) && !defined(PH3_LEGACY_SCRIPT_LITERALS)
#define PH3_SCRIPT_LITERAL_POOL 1
#endif
#if defined(PH3_SCRIPT_INLINE_SCALARS) && !defined(PH3_LEGACY_SCRIPT_ARITHMETIC)
#define PH3_SCRIPT_FAST_ARITHMETIC 1
#endif
#if defined(PH3_SCRIPT_INLINE_SCALARS) && !defined(PH3_LEGACY_SCRIPT_BUILTINS)
#define PH3_SCRIPT_FAST_BUILTINS 1
#endif
#if defined(PH3_SCRIPT_LITERAL_POOL) && !defined(PH3_LEGACY_SCRIPT_CODE_LAYOUT)
#define PH3_SCRIPT_COMPACT_CODE 1
#endif
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_PARAMETERS)
#define PH3_SCRIPT_DIRECT_PARAMETERS 1
#endif

#pragma warning(disable : 4786) //STL Warning抑止
#pragma warning(disable : 4018) //signed と unsigned の数値を比較
#pragma warning(disable : 4244) //double' から 'float' に変換
#include <list>
#ifdef __EMSCRIPTEN__
#include <deque>
#include <memory>
#endif
#include <map>
#include <string>
#include <vector>

//重複宣言チェックをしない
//#define __SCRIPT_H__NO_CHECK_DUPLICATED

//-------- 汎用
namespace gstd {

#ifdef PH3_VM_OPCODE_PROFILE
void profile_script_scalar_box();
void profile_script_body_request();
#endif

std::string to_mbcs(std::wstring const& s);
std::wstring to_wide(std::string const& s);

template <typename T>
class lightweight_vector {
private:
	// clear() is also used for VM unwinding and must not release its values.
	// Remember only that hidden prefix, rather than rescanning every spare slot
	// each time an environment with a mostly empty stack is recycled.
	unsigned retained_length_ = 0;
public:
	unsigned length;
	unsigned capacity;
	T* at;

	lightweight_vector()
		: length(0)
		, capacity(0)
		, at(NULL)
	{
	}

	lightweight_vector(lightweight_vector const& source);

	~lightweight_vector()
	{
		if (at != NULL) {
			delete[] at;
		}
	}

	lightweight_vector& operator=(lightweight_vector const& source);

	void expand();

	void push_back(T const& value)
	{
		if (length == capacity) {
			// The argument may be an element of this vector. Expansion destroys
			// the old storage, so preserve that element before reallocating.
			T saved(value);
			expand();
			at[length++] = std::move(saved);
			return;
		}
		at[length] = value;
		++length;
	}

	void push_back(T&& item)
	{
		if (length == capacity) {
			T saved(std::move(item));
			expand();
			at[length++] = std::move(saved);
			return;
		}
		at[length++] = std::move(item);
	}

	void pop_back()
	{
		--length;

		T temp;
		at[length] = temp;
	}

	void clear()
	{
		if (retained_length_ < length) retained_length_ = length;
		length = 0;
	}

	// For dead VM environments only. clear() intentionally keeps its original
	// stack-unwind semantics; a recycled environment must release *all* retained
	// references, including slots hidden by clear(), without reallocating small
	// stacks/variable arrays on every function call.
	void reset_for_reuse(unsigned maximum_capacity)
	{
		if (capacity > maximum_capacity) { release(); return; }
#ifdef PH3_LEGACY_STACK_RESET
		const unsigned populated = capacity;
#else
		const unsigned populated = length > retained_length_ ? length : retained_length_;
		assert(populated <= capacity);
#endif
		for (unsigned i = 0; i < populated; ++i) at[i] = T();
		length = 0;
		retained_length_ = 0;
	}

	void release()
	{
		length = 0;
		retained_length_ = 0;
		if (at != NULL) {
			delete[] at;
			at = NULL;
			capacity = 0;
		}
	}

	unsigned size() const
	{
		return length;
	}

	T& operator[](unsigned i)
	{
		return at[i];
	}

	T const& operator[](unsigned i) const
	{
		return at[i];
	}

	T& back()
	{
		return at[length - 1];
	}

	T* begin()
	{
		return &at[0];
	}

	void erase(T* pos);
	void insert(T* pos, T const& value);
};

template <typename T>
lightweight_vector<T>::lightweight_vector(lightweight_vector const& source)
{
	length = source.length;
	capacity = source.capacity;
	if (source.capacity > 0) {
		at = new T[source.capacity];
		for (int i = length - 1; i >= 0; --i)
			at[i] = source.at[i];
	} else {
		at = NULL;
	}
}

template <typename T>
lightweight_vector<T>& lightweight_vector<T>::operator=(lightweight_vector<T> const& source)
{
	if (this == &source) return *this;
	if (at != NULL)
		delete[] at;
	length = source.length;
	capacity = source.capacity;
	retained_length_ = 0; // Assignment copies only the source's active prefix.
	if (source.capacity > 0) {
		at = new T[source.capacity];
		for (int i = length - 1; i >= 0; --i)
			at[i] = source.at[i];
	} else {
		at = NULL;
	}
	return *this;
}

template <typename T>
void lightweight_vector<T>::expand()
{
	if (capacity == 0) {
		// delete[] at;
		capacity = 4;
		at = new T[4];
	} else {
		capacity *= 2;
		T* n = new T[capacity];
		for (int i = length - 1; i >= 0; --i)
			n[i] = std::move_if_noexcept(at[i]);
		delete[] at;
		at = n;
	}
	// Only active elements were moved; delete[] released any hidden prefix.
	retained_length_ = 0;
}

template <typename T>
void lightweight_vector<T>::erase(T* pos)
{
	--length;
	for (T* i = pos; i < at + length; ++i) {
		*i = std::move_if_noexcept(*(i + 1));
	}
	at[length] = T();
}

template <typename T>
void lightweight_vector<T>::insert(T* pos, T const& value)
{
	T saved(value);
	if (length == capacity) {
		unsigned pos_index = pos - at;
		expand();
		pos = at + pos_index;
	}
	for (T* i = at + length; i > pos; --i) {
		*i = std::move_if_noexcept(*(i - 1));
	}
	*pos = std::move(saved);
	++length;
}

//-------- ここから

class type_data {
public:
	enum type_kind {
		tk_real,
		tk_char,
		tk_boolean,
		tk_array
	};

	type_data(type_kind k, type_data* t = NULL)
		: kind(k)
		, element(t)
	{
	}

	type_data(type_data const& source)
		: kind(source.kind)
		, element(source.element)
	{
	}

	//デストラクタはデフォルトに任せる

	type_kind get_kind()
	{
		return kind;
	}

	type_data* get_element()
	{
		return element;
	}

private:
	type_kind kind;
	type_data* element;

	type_data& operator=(type_data const& source);
};

class value {
public:
	value()
		: data(NULL) {}

	value(type_data* t, gstd::script_real v)
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(t && t->get_kind()==type_data::tk_real){
			data=NULL;scalar_type_=t;scalar_.real_value=v;return;
		}
#endif
		data = new body();
		data->ref_count = 1;
		data->type = t;
		data->real_value = v;
	}

	value(type_data* t, wchar_t v)
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(t && t->get_kind()==type_data::tk_char){
			data=NULL;scalar_type_=t;scalar_.char_value=v;return;
		}
#endif
		data = new body();
		data->ref_count = 1;
		data->type = t;
		data->char_value = v;
	}

	value(type_data* t, bool v)
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(t && t->get_kind()==type_data::tk_boolean){
			data=NULL;scalar_type_=t;scalar_.boolean_value=v;return;
		}
#endif
		data = new body();
		data->ref_count = 1;
		data->type = t;
		data->boolean_value = v;
	}

	value(type_data* t, std::wstring v);

	value(value const& source)
	{
		data = source.data;
#ifdef PH3_SCRIPT_INLINE_SCALARS
		scalar_type_=source.scalar_type_;scalar_=source.scalar_;
#endif
		if (data != NULL)
			++(data->ref_count);
	}
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
	value(value&& source) noexcept : data(source.data) {
#ifdef PH3_SCRIPT_INLINE_SCALARS
		scalar_type_=source.scalar_type_;scalar_=source.scalar_;source.scalar_type_=NULL;
#endif
		source.data = NULL;
	}
	value& operator=(value&& source) noexcept
	{
		if(this == &source)return *this;
		// source can be an element of this array. Take ownership before
		// release() destroys the array and its source slot.
		body* incoming = source.data;
#ifdef PH3_SCRIPT_INLINE_SCALARS
		type_data* incoming_type=source.scalar_type_;
		const auto incoming_scalar=source.scalar_;
		source.scalar_type_=NULL;
#endif
		source.data = NULL;
		release();
		data = incoming;
#ifdef PH3_SCRIPT_INLINE_SCALARS
		scalar_type_=incoming_type;scalar_=incoming_scalar;
#endif
		return *this;
	}
#endif

	~value()
	{
		release();
	}

	value& operator=(value const& source)
	{
		body* incoming = source.data;
#ifdef PH3_SCRIPT_INLINE_SCALARS
		// source may be a scalar element of the array being replaced.
		type_data* incoming_type=source.scalar_type_;
		const auto incoming_scalar=source.scalar_;
#endif
		if (incoming != NULL) {
			++(incoming->ref_count);
		}
		release();
		data = incoming;
#ifdef PH3_SCRIPT_INLINE_SCALARS
		scalar_type_=incoming_type;scalar_=incoming_scalar;
#endif
		return *this;
	}
	void swap(value& other) noexcept {
		std::swap(data, other.data);
#ifdef PH3_SCRIPT_INLINE_SCALARS
		std::swap(scalar_type_,other.scalar_type_);std::swap(scalar_,other.scalar_);
#endif
	}

	bool has_data() const
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		return data != NULL || scalar_type_ != NULL;
#else
		return data != NULL;
#endif
	}

	void set(type_data* t, gstd::script_real v)
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(t && t->get_kind()==type_data::tk_real){
			release();data=NULL;scalar_type_=t;scalar_.real_value=v;return;
		}
#endif
		unique();
		data->type = t;
		data->real_value = v;
	}

	void set(type_data* t, bool v)
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(t && t->get_kind()==type_data::tk_boolean){
			release();data=NULL;scalar_type_=t;scalar_.boolean_value=v;return;
		}
#endif
		unique();
		data->type = t;
		data->boolean_value = v;
	}

	void append(type_data* t, value const& x);
	void concatenate(value const& x);

	gstd::script_real as_real() const;
#if defined(PH3_SCRIPT_FAST_ARITHMETIC) || defined(PH3_SCRIPT_FAST_BUILTINS)
	// Numeric VM instructions must not use the coercing/string conversion API.
	// Keep this small read visible to the dispatch loop, including boxed writable
	// scalars. Type identity is still checked separately for comparison.
	bool try_real_operand(gstd::script_real& result) const {
		if (scalar_type_ != NULL) {
			if (scalar_type_->get_kind() != type_data::tk_real) return false;
			result = scalar_.real_value; return true;
		}
		if (data == NULL || data->type->get_kind() != type_data::tk_real) return false;
		result = data->real_value; return true;
	}
#endif
	wchar_t as_char() const;
	bool as_boolean() const;
	std::wstring as_string() const;
	unsigned length_as_array() const;
	value const& index_as_array(unsigned i) const;
	value& index_as_array(unsigned i);
	type_data* get_type() const;

	void unique() const
	{
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(scalar_type_ != NULL){
#ifdef PH3_VM_OPCODE_PROFILE
			profile_script_scalar_box();
#endif
			// index_writable shares a handle to an array element. Materialize
			// before returning that handle; ordinary scalar copies remain
			// allocation-free, writable references keep the original semantics.
			body* boxed=new body();boxed->ref_count=1;boxed->type=scalar_type_;
			switch(scalar_type_->get_kind()){
			case type_data::tk_real:boxed->real_value=scalar_.real_value;break;
			case type_data::tk_char:boxed->char_value=scalar_.char_value;break;
			case type_data::tk_boolean:boxed->boolean_value=scalar_.boolean_value;break;
			default:assert(false);
			}
			data=boxed;scalar_type_=NULL;return;
		}
#endif
		if (data == NULL) {
			data = new body();
			data->ref_count = 1;
			data->type = NULL;
		} else if (data->ref_count > 1) {
			--(data->ref_count);
			data = new body(*data);
			data->ref_count = 1;
		}
	}

	void overwrite(value const& source); //危険！外から呼ぶな

private:
	gstd::script_real read_real() const {
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(scalar_type_ != NULL)return scalar_.real_value;
#endif
		return data->real_value;
	}
	wchar_t read_char() const {
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(scalar_type_ != NULL)return scalar_.char_value;
#endif
		return data->char_value;
	}
	bool read_boolean() const {
#ifdef PH3_SCRIPT_INLINE_SCALARS
		if(scalar_type_ != NULL)return scalar_.boolean_value;
#endif
		return data->boolean_value;
	}
	inline void release()
	{
		if (data != NULL) {
			--(data->ref_count);
			if (data->ref_count == 0) {
				delete data;
			}
		}
	}
	struct body {
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_ALLOCATOR)
		static void* operator new(std::size_t bytes) {
#ifdef PH3_VM_OPCODE_PROFILE
			profile_script_body_request();
#endif
			assert(bytes == sizeof(body));
			return BoundedAllocationCache<sizeof(body), 4096>::Allocate();
		}
		static void operator delete(void* memory) noexcept {
			BoundedAllocationCache<sizeof(body), 4096>::Release(memory);
		}
#endif
		int ref_count;
		type_data* type;
		lightweight_vector<value> array_value;

		union {
			gstd::script_real real_value;
			wchar_t char_value;
			bool boolean_value;
		};
#ifdef PH3_SCRIPT_STRING_CACHE
		// Cache only unexposed short character arrays. Public element access
		// permanently disables caching because a writable handle can outlive it.
		std::shared_ptr<const std::wstring> string_cache;
		bool string_cacheable=true;
#endif
	};

	mutable body* data;
#ifdef PH3_SCRIPT_INLINE_SCALARS
	// Keep exact binary64 bits and original type identity, without NaN boxing
	// or narrowing. Arrays and writable element handles still use a body.
	union ScalarBits {
		gstd::script_real real_value;
		wchar_t char_value;
		bool boolean_value;
		ScalarBits():real_value(0){}
	};
	mutable type_data* scalar_type_=NULL;
	mutable ScalarBits scalar_;
#endif
};

class script_engine;
class script_machine;

typedef value (*callback)(script_machine* machine, int argc, value const* argv);

struct function {
	char const* name;
	callback func;
	unsigned arguments;
};

class script_type_manager {
public:
	script_type_manager();

	type_data* get_real_type()
	{
		return real_type;
	}

	type_data* get_char_type()
	{
		return char_type;
	}

	type_data* get_boolean_type()
	{
		return boolean_type;
	}

	type_data* get_string_type()
	{
		return string_type;
	}

	type_data* get_array_type(type_data* element);

private:
	script_type_manager(script_type_manager const&);
	script_type_manager& operator=(script_type_manager const& source);

	std::list<type_data> types; //中身のポインタを使うのでアドレスが変わらないようにlist
	type_data* real_type;
	type_data* char_type;
	type_data* boolean_type;
	type_data* string_type;
};

class script_engine {
public:
	script_engine(script_type_manager* a_type_manager, std::string const& source, int funcc, function const* funcv);
	script_engine(script_type_manager* a_type_manager, std::vector<char> const& source, int funcc, function const* funcv);
	virtual ~script_engine();

	void* data; //クライアント用空間

	bool get_error()
	{
		return error;
	}

	std::wstring& get_error_message()
	{
		return error_message;
	}

	int get_error_line()
	{
		return error_line;
	}

	script_type_manager* get_type_manager()
	{
		return type_manager;
	}

	//compatibility
	type_data* get_real_type()
	{
		return type_manager->get_real_type();
	}

	type_data* get_char_type()
	{
		return type_manager->get_char_type();
	}

	type_data* get_boolean_type()
	{
		return type_manager->get_boolean_type();
	}

	type_data* get_array_type(type_data* element)
	{
		return type_manager->get_array_type(element);
	}

	type_data* get_string_type()
	{
		return type_manager->get_string_type();
	}

#ifndef _MSC_VER
private:
#endif

	//コピー、代入演算子の自動生成を無効に
	script_engine(script_engine const& source);
	script_engine& operator=(script_engine const& source);

	//エラー
	bool error;
	std::wstring error_message;
	int error_line;

	//型
	script_type_manager* type_manager;

	//中間コード
	enum command_kind {
		pc_assign,
		pc_assign_writable,
		pc_break_loop,
		pc_break_routine,
		pc_call,
		pc_call_and_push_result,
		pc_case_begin,
		pc_case_end,
		pc_case_if,
		pc_case_if_not,
		pc_case_next,
		pc_compare_e,
		pc_compare_g,
		pc_compare_ge,
		pc_compare_l,
		pc_compare_le,
		pc_compare_ne,
		pc_dup,
		pc_dup2,
		pc_loop_ascent,
		pc_loop_back,
		pc_loop_count,
		pc_loop_descent,
		pc_loop_if,
		pc_pop,
		pc_push_value,
		pc_push_variable,
		pc_push_variable_writable,
		pc_swap,
		pc_yield
#ifdef PH3_SCRIPT_FAST_ARITHMETIC
		, pc_real_add, pc_real_subtract, pc_real_multiply, pc_real_divide,
		pc_real_negative, pc_real_predecessor, pc_real_successor, pc_real_compare
#endif
#ifdef PH3_SCRIPT_FAST_BUILTINS
		, pc_index_read, pc_array_length, pc_boolean_not, pc_boolean_true, pc_boolean_false
#endif
	};

	struct block;

	struct code {
		command_kind command;
		int line; //ソースコード上の行
#ifndef PH3_SCRIPT_COMPACT_CODE
#ifdef PH3_SCRIPT_LITERAL_POOL
		const value* data=nullptr;
#else
		value data; //pc_push_valueでpushするデータ
#endif
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
		// A compiled block is immutable. Branch targets depend on bytecode,
		// never on a script instance, argument values, RNG, or game frame.
		int cached_jump = -1;
#endif
#endif

		union {
#ifdef PH3_SCRIPT_COMPACT_CODE
			// An opcode uses either a literal, lexical slot, call target, or
			// branch target. These operands are never live simultaneously.
			// Keep line/opcode intact; do not narrow values, pointers or indices.
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
			int cached_jump = -1;
			const value* data;
#else
			const value* data = nullptr;
#endif
#endif
			struct
			{
				int level; //assign/push_variableの変数の環境位置
				unsigned variable; //assign/push_variableの変数のインデックス
			};
			struct
			{
				block* sub; //call/call_and_push_resultの飛び先
				unsigned arguments; //call/call_and_push_resultの引数の数
			};
			struct
			{
				int ip; //loop_backの戻り先
			};
		};

		code()
		{
		}

		code(int the_line, command_kind the_command)
			: line(the_line)
			, command(the_command)
		{
		}

		code(int the_line, command_kind the_command, int the_level, unsigned the_variable)
			: line(the_line)
			, command(the_command)
			, level(the_level)
			, variable(the_variable)
		{
		}

#if defined(PH3_SCRIPT_FAST_ARITHMETIC) || defined(PH3_SCRIPT_FAST_BUILTINS)
		code(int the_line, command_kind the_command, block* the_sub, int the_arguments);
#else
		code(int the_line, command_kind the_command, block* the_sub, int the_arguments)
			: line(the_line)
			, command(the_command)
			, sub(the_sub)
			, arguments(the_arguments)
		{
		}
#endif

		code(int the_line, command_kind the_command, int the_ip)
			: line(the_line)
			, command(the_command)
			, ip(the_ip)
		{
		}

#ifdef PH3_SCRIPT_LITERAL_POOL
		code(int the_line, command_kind the_command, const value* the_data)
#else
		code(int the_line, command_kind the_command, value const& the_data)
#endif
			: line(the_line)
			, command(the_command)
			, data(the_data)
		{
		}
		const value& literal() const {
#ifdef PH3_SCRIPT_LITERAL_POOL
			assert(data!=nullptr);return *data;
#else
			return data;
#endif
		}
	};
#ifdef PH3_SCRIPT_COMPACT_CODE
	static_assert(sizeof(void*)!=4 || sizeof(code)==16,
		"Web32 tagged bytecode operands must fit in 16 bytes");
#elif defined(PH3_SCRIPT_LITERAL_POOL)
	// A wide scalar cell must not enlarge EVERY bytecode instruction. Only
	// pc_push_value owns an entry in the engine's address-stable literal pool.
	static_assert(sizeof(void*)!=4 || sizeof(code)<=24,
		"Web32 bytecode should stay compact after inline scalar storage");
#endif

	enum block_kind {
		bk_normal,
		bk_loop,
		bk_sub,
		bk_function,
		bk_microthread
	};

	friend struct block;

	typedef lightweight_vector<code> codes_t;

	struct block {
		int level;
		int arguments;
		std::string name;
		callback func;
		codes_t codes;
		block_kind kind;
#ifdef PH3_SCRIPT_DIRECT_PARAMETERS
		// -2: unchecked, -1: not a contiguous parameter-assignment prefix.
		int direct_parameter_base=-2;
		unsigned direct_parameter_count=0;
#endif

		block(int the_level, block_kind the_kind)
			: level(the_level)
			, arguments(0)
			, name()
			, func(NULL)
			, codes()
			, kind(the_kind)
		{
		}
	};

#ifdef __EMSCRIPTEN__
	// push_back on deque keeps references/pointers to existing elements valid,
	// which is the property the VM needs here, without one heap allocation per
	// block as std::list requires in WebAssembly.
	std::deque<block> blocks;
#else
	std::list<block> blocks; //中身のポインタを使うのでアドレスが変わらないようにlist
#endif
	block* main_block;
	std::map<std::string, block*> events;
#ifdef PH3_SCRIPT_LITERAL_POOL
	std::deque<value> literals_;
#endif
	code make_literal(int line,const value& literal) {
#ifdef PH3_SCRIPT_LITERAL_POOL
		// The engine outlives its machines. deque preserves element addresses
		// while parsing further blocks; bytecode copies never own these pointers.
		literals_.push_back(literal);
		return code(line,pc_push_value,&literals_.back());
#else
		return code(line,pc_push_value,literal);
#endif
	}

	block* new_block(int level, block_kind kind)
	{
#ifdef __EMSCRIPTEN__
		blocks.emplace_back(level, kind);
		return &blocks.back();
#else
		block x(level, kind);
		return &*blocks.insert(blocks.end(), x);
#endif
	}

	friend class parser;
	friend class script_machine;
};

class script_machine {
public:
	script_machine(script_engine* the_engine);
	virtual ~script_machine();

	void* data; //クライアント用空間

	void run();
	void call(std::string event_name);
	void resume();

	void stop()
	{
		finished = true;
		stopped = true;
	}

	bool get_stopped()
	{
		return stopped;
	}

	bool get_resuming()
	{
		return resuming;
	}

	bool get_error()
	{
		return error;
	}

	std::wstring& get_error_message()
	{
		return error_message;
	}

	int get_error_line()
	{
		return error_line;
	}

	void raise_error(std::wstring const& message)
	{
		error = true;
		error_message = message;
		finished = true;
	}
	void terminate(std::wstring const& message)
	{
		bTerminate = true;
		error = true;
		error_message = message;
		finished = true;
	}

	script_engine* get_engine()
	{
		return engine;
	}

	bool has_event(std::string event_name);

	int get_current_line();

	int get_thread_count() { return threads.size(); }

private:
	script_machine();
	script_machine(script_machine const& source);
	script_machine& operator=(script_machine const& source);

	script_engine* engine;

	bool error;
	std::wstring error_message;
	int error_line;

	bool bTerminate;

	typedef lightweight_vector<value> variables_t;
	typedef lightweight_vector<value> stack_t;

	struct environment {
		environment *pred, *succ; //双方向リンクリスト
		environment* parent;
		int ref_count;
		script_engine::block* sub;
		unsigned ip;
		variables_t variables;
		stack_t stack;
		bool has_result;
	};

	std::list<environment*> call_start_parent_environment_list;
	environment* first_using_environment;
	environment* last_using_environment;
	environment* first_garbage_environment;
	environment* last_garbage_environment;
	environment* new_environment(environment* parent, script_engine::block* b);
#ifdef PH3_SCRIPT_DIRECT_PARAMETERS
	bool transfer_arguments(environment* target,stack_t& source,unsigned count);
#endif
	void dispose_environment(environment* object);

	typedef lightweight_vector<environment*> threads_t;

	threads_t threads;
	unsigned current_thread_index;
	bool finished;
	bool stopped;
	bool resuming;

	void yield()
	{
		if (current_thread_index > 0)
			--current_thread_index;
		else
			current_thread_index = threads.size() - 1;
	}

	void advance();
};

template <int num>
class constant {
public:
	static value func(script_machine* machine, int argc, value const* argv)
	{
		return value(machine->get_engine()->get_real_type(), (gstd::script_real)num);
	}
};

} // namespace gstd

#endif
