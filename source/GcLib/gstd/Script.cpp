#include "Script.hpp"
#ifdef __EMSCRIPTEN__
#include <unordered_map>
#endif
#ifdef __EMSCRIPTEN__
#include "../portable/Utf16.hpp"
#endif
#include "GstdUtility.hpp"
#ifdef PH3_VM_OPCODE_PROFILE
#include <emscripten.h>
#include <cstdint>
#endif

#include <cassert>
#include <cctype>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <vector>
#ifndef __EMSCRIPTEN__
#include <windows.h>
#endif

#ifdef _MSC_VER
#define for if (0); else for
namespace std {
using ::atof;
using ::ceill;
using ::fabsl;
using ::floorl;
using ::fmodl;
using ::isalpha;
using ::isdigit;
using ::iswalpha;
using ::iswdigit;
using ::isxdigit;
using ::mbstowcs;
using ::powl;
using ::swprintf;
using ::wcstombs;
}

#endif

using namespace gstd;

#ifdef PH3_VM_OPCODE_PROFILE
namespace {
struct VmOpcodeProfile {
    bool active=false;
    std::uint64_t dispatch[64]{},fused[64]{};
    std::unordered_map<std::string,std::uint64_t> native;
    std::uint64_t scalarBoxes=0,bodyRequests=0,environments=0,reusedEnvironments=0,nativeOverflow=0;
} vmOpcodeProfile;
void countVmOpcode(unsigned command,bool fused=false){
    if(vmOpcodeProfile.active&&command<64)++(fused?vmOpcodeProfile.fused:vmOpcodeProfile.dispatch)[command];
}
void countVmNative(const std::string& name){
    if(!vmOpcodeProfile.active)return;
    auto found=vmOpcodeProfile.native.find(name);
    if(found!=vmOpcodeProfile.native.end())++found->second;
    else if(vmOpcodeProfile.native.size()<1024)vmOpcodeProfile.native.emplace(name,1);
    else ++vmOpcodeProfile.nativeOverflow;
}
}
void gstd::profile_script_scalar_box(){if(vmOpcodeProfile.active)++vmOpcodeProfile.scalarBoxes;}
void gstd::profile_script_body_request(){if(vmOpcodeProfile.active)++vmOpcodeProfile.bodyRequests;}
extern "C" EMSCRIPTEN_KEEPALIVE void hsob_vm_profile_reset(){vmOpcodeProfile=VmOpcodeProfile();vmOpcodeProfile.active=true;}
extern "C" EMSCRIPTEN_KEEPALIVE void hsob_vm_profile_dump(){
    static const char* names[]={"assign","assign-writable","break-loop","break-routine","call","call-result",
        "case-begin","case-end","case-if","case-if-not","case-next","compare-e","compare-g","compare-ge",
        "compare-l","compare-le","compare-ne","dup","dup2","loop-ascent","loop-back","loop-count",
        "loop-descent","loop-if","pop","push-value","push-variable","push-variable-writable","swap","yield"
#ifdef PH3_SCRIPT_FAST_ARITHMETIC
        ,"real-add","real-subtract","real-multiply","real-divide","real-negative","real-predecessor","real-successor","real-compare"
#endif
#ifdef PH3_SCRIPT_FAST_BUILTINS
        ,"index-read","array-length","boolean-not","boolean-true","boolean-false"
#endif
    };
    EM_ASM({globalThis.ph3Diagnostics.vmProfile=({dispatch:{},fused:{},native:{},
        scalarBoxes:$0,bodyRequests:$1,environments:$2,reusedEnvironments:$3,nativeOverflow:$4});},
        double(vmOpcodeProfile.scalarBoxes),double(vmOpcodeProfile.bodyRequests),double(vmOpcodeProfile.environments),
        double(vmOpcodeProfile.reusedEnvironments),double(vmOpcodeProfile.nativeOverflow));
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)
        EM_ASM({const p=globalThis.ph3Diagnostics.vmProfile;const n=UTF8ToString($0);p.dispatch[n]=$1;p.fused[n]=$2;},
            names[i],double(vmOpcodeProfile.dispatch[i]),double(vmOpcodeProfile.fused[i]));
    for(const auto& [name,count]:vmOpcodeProfile.native)
        EM_ASM({globalThis.ph3Diagnostics.vmProfile.native[UTF8ToString($0)]=$1;},name.c_str(),double(count));
}
#endif

std::string gstd::to_mbcs(std::wstring const& s)
{
	std::string result = StringUtility::ConvertWideToMulti(s);
	// int len = std::wcstombs(NULL, s.c_str(), s.size());
	// if (len < 0)
	// 	return "(BAD-DATA)";
	// char * buffer = new char[len + 1];
	// std::wcstombs(buffer, s.c_str(), len);
	// std::string result(buffer, len);
	// delete[] buffer;
	return result;
}

std::wstring gstd::to_wide(std::string const& s)
{
	std::wstring result = StringUtility::ConvertMultiToWide(s);
	// int len = std::mbstowcs(NULL, s.c_str(), s.size());
	// wchar_t * buffer = new wchar_t[len + 1];
	// std::mbstowcs(buffer, s.c_str(), len);
	// std::wstring result(buffer, len);
	// delete[] buffer;
	return result;
}

gstd::script_real fmodl2(gstd::script_real i, gstd::script_real j)
{
	if (j < 0) {
		// return (i < 0) ? -(-i % -j) : (i % -j) + j;
		return (i < 0) ? -std::fmod(-i, -j) : std::fmod(i, -j) + j;
	} else {
		// return (i < 0) ? -(-i % j) + j : i % j;
		return (i < 0) ? -std::fmod(-i, j) + j : std::fmod(i, j);
	}
}

/* value */

value::value(type_data* t, std::wstring v)
{
	data = new body();
	data->ref_count = 1;
	data->type = t;
	for (unsigned i = 0; i < v.size(); ++i)
		data->array_value.push_back(value(t->get_element(), v[i]));
}

void value::append(type_data* t, value const& x)
{
	unique();
#ifdef PH3_SCRIPT_STRING_CACHE
	data->string_cache.reset();
#endif
	data->type = t;
	data->array_value.push_back(x);
}

void value::concatenate(value const& x)
{
	unique();
#ifdef PH3_SCRIPT_STRING_CACHE
	data->string_cache.reset();
#endif
	unsigned l = data->array_value.length;
	unsigned r = x.data->array_value.length;
	unsigned t = l + r;
	if (l == 0)
		data->type = x.data->type;
	while (data->array_value.capacity < t)
		data->array_value.expand();
	for (unsigned i = 0; i < r; ++i)
		data->array_value[l + i] = x.data->array_value.at[i];
	data->array_value.length = t;
}

gstd::script_real value::as_real() const
{
	if (!has_data())
		return gstd::script_real(0);
	else {
		switch (get_type()->get_kind()) {
		case type_data::tk_real:
			return read_real();
		case type_data::tk_char:
			return static_cast<gstd::script_real>(read_char());
		case type_data::tk_boolean:
			return read_boolean() ? gstd::script_real(1) : gstd::script_real(0);
		case type_data::tk_array:
			if (data->type->get_element()->get_kind() == type_data::tk_char)
				return std::atof(to_mbcs(as_string()).c_str());
			else
				return 0.0L;
		default:
			assert(false);
			return 0.0L;
		}
	}
}

wchar_t value::as_char() const
{
	if (!has_data())
		return 0.0L;
	else {
		switch (get_type()->get_kind()) {
		case type_data::tk_real:
			return read_real();
		case type_data::tk_char:
			return read_char();
		case type_data::tk_boolean:
			return read_boolean() ? L'1' : L'0';
		case type_data::tk_array:
			return L'\0';
		default:
			assert(false);
			return L'\0';
		}
	}
}

bool value::as_boolean() const
{
	if (!has_data())
		return false;
	else {
		switch (get_type()->get_kind()) {
		case type_data::tk_real:
			return read_real() != 0.0L;
		case type_data::tk_char:
			return read_char() != L'\0';
		case type_data::tk_boolean:
			return read_boolean();
		case type_data::tk_array:
			return data->array_value.size() != 0;
		default:
			assert(false);
			return false;
		}
	}
}

std::wstring value::as_string() const
{
	if (!has_data())
		return L"(VOID)";

	else {
		switch (get_type()->get_kind()) {
		case type_data::tk_real: {
			wchar_t buffer[128];
			// MSVC's long double is binary64; the portable VM uses double to
			// preserve that contract. Match the vararg format to its actual type.
#ifdef __EMSCRIPTEN__
			std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%f", read_real());
#else
			std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%Lf", read_real());
#endif
			return std::wstring(buffer);
		}

		case type_data::tk_char: {
			std::wstring result;
			result += read_char();
			return result;
		}

		case type_data::tk_boolean:
			return read_boolean() ? L"true" : L"false";

		case type_data::tk_array: {
			if (data->type->get_element()->get_kind() == type_data::tk_char) {
#ifdef PH3_SCRIPT_STRING_CACHE
				if(data->string_cache)return *data->string_cache;
				bool cacheable=data->string_cacheable&&data->array_value.size()<=512;
#endif
				std::wstring result;
				for (unsigned i = 0; i < data->array_value.size(); ++i){
					result += data->array_value[i].as_char();
#ifdef PH3_SCRIPT_STRING_CACHE
					// A boxed element may already have a writable alias through
					// another array. Never cache such a parent, even after COW.
					if(data->array_value[i].data!=NULL)cacheable=false;
#endif
				}
#ifdef PH3_SCRIPT_STRING_CACHE
				if(cacheable)data->string_cache=std::make_shared<const std::wstring>(result);
#endif
				return result;
			} else {
				std::wstring result = L"[";
				for (unsigned i = 0; i < data->array_value.size(); ++i) {
					result += data->array_value[i].as_string();
					if (i != data->array_value.size() - 1)
						result += L",";
				}
				result += L"]";
				return result;
			}
		}
		default:
			assert(false);
			return L"(INTERNAL-ERROR)";
		}
	}
}

unsigned value::length_as_array() const
{
	assert(data != NULL && data->type->get_kind() == type_data::tk_array);
	return data->array_value.size();
}

value const& value::index_as_array(unsigned i) const
{
	assert(data != NULL && data->type->get_kind() == type_data::tk_array);
	assert(i < data->array_value.length);
#ifdef PH3_SCRIPT_STRING_CACHE
	data->string_cacheable=false;data->string_cache.reset();
#endif
	return data->array_value[i];
}

value& value::index_as_array(unsigned i)
{
	assert(data != NULL && data->type->get_kind() == type_data::tk_array);
	assert(i < data->array_value.length);
#ifdef PH3_SCRIPT_STRING_CACHE
	data->string_cacheable=false;data->string_cache.reset();
#endif
	return data->array_value[i];
}

type_data* value::get_type() const
{
#ifdef PH3_SCRIPT_INLINE_SCALARS
	if(scalar_type_ != NULL)return scalar_type_;
#endif
	assert(data != NULL);
	return data->type;
}

void value::overwrite(value const& source)
{
#ifdef PH3_SCRIPT_INLINE_SCALARS
	if(source.scalar_type_ != NULL)source.unique();
#endif
	assert(data != NULL);
	if (data == source.data)
		return;

	release();
	*data = *source.data;
	data->ref_count = 2;

	// * data = * source.data;
	// ++(data->ref_count);
}

//--------------------------------------

/* parser_error */

class parser_error : public gstd::wexception {
public:
	parser_error(std::wstring const& the_message)
		: gstd::wexception(the_message)
	{
	}
};

/* lexical analyzer */

enum token_kind {
	tk_end,
	tk_invalid,
	tk_word,
	tk_real,
	tk_char,
	tk_string,
	tk_open_par,
	tk_close_par,
	tk_open_bra,
	tk_close_bra,
	tk_open_cur,
	tk_close_cur,
	tk_open_abs,
	tk_close_abs,
	tk_comma,
	tk_semicolon,
	tk_tilde,
	tk_assign,
	tk_plus,
	tk_minus,
	tk_inc,
	tk_dec,
	tk_asterisk,
	tk_slash,
	tk_percent,
	tk_caret,
	tk_e,
	tk_g,
	tk_ge,
	tk_l,
	tk_le,
	tk_ne,
	tk_exclamation,
	tk_ampersand,
	tk_and_then,
	tk_vertical,
	tk_or_else,
	tk_at,
	tk_add_assign,
	tk_subtract_assign,
	tk_multiply_assign,
	tk_divide_assign,
	tk_remainder_assign,
	tk_power_assign,
	tk_range,
	tk_ALTERNATIVE,
	tk_ASCENT,
	tk_BREAK,
	tk_CASE,
	tk_DESCENT,
	tk_ELSE,
	tk_FUNCTION,
	tk_IF,
	tk_IN,
	tk_LET,
	tk_LOCAL,
	tk_LOOP,
	tk_OTHERS,
	tk_REAL,
	tk_RETURN,
	tk_SUB,
	tk_TASK,
	tk_TIMES,
	tk_WHILE,
	tk_YIELD,
};

class scanner {
public:
	token_kind next;
	std::string word;
	gstd::script_real real_value;
	wchar_t char_value;
	std::wstring string_value;
	int line;

	scanner(char const* source, char const* end)
		: current(source)
		, line(1)
	{
		endPoint = end;
		encoding = Encoding::SHIFT_JIS;
		if (Encoding::IsUtf16Le(source, 2)) {
			encoding = Encoding::UTF16LE;

			int bomSize = Encoding::GetBomSize(source, 2);
			current += bomSize;
		}

		advance();
	}

	scanner(scanner const& source)
		: encoding(source.encoding)
		, current(source.current)
		, endPoint(source.endPoint)
		, next(source.next)
		, word(source.word)
		, line(source.line)
	{
	}

	void skip();
	void advance();

	void AddLog(wchar_t* data);

private:
	int encoding;
	char const* current;
	char const* endPoint;

	inline wchar_t current_char();
	inline wchar_t index_from_current_char(int index);
	inline wchar_t next_char();
};

wchar_t scanner::current_char()
{
	if (current >= endPoint) return L'\0';
	wchar_t res = L'\0';
	if (encoding == Encoding::UTF16LE) {
#ifdef __EMSCRIPTEN__
		if (endPoint - current < 2) throw parser_error(L"Incomplete UTF-16LE code unit");
		res = gstd::portable::ReadUtf16Unit(current);
#else
		res = (wchar_t&)current[0];
#endif
	} else {
		res = *current;
	}
	return res;
}
wchar_t scanner::index_from_current_char(int index)
{
	wchar_t res = L'\0';
	if (encoding == Encoding::UTF16LE) {
		const char* pos = current + index * 2;
		if (pos >= endPoint)
			return L'\0';
#ifdef __EMSCRIPTEN__
		if (endPoint - pos < 2) return L'\0';
		res = gstd::portable::ReadUtf16Unit(pos);
#else
		res = (wchar_t&)current[index * 2];
#endif
	} else {
		const char* pos = current + index;
		if (pos >= endPoint)
			return L'\0';
		res = current[index];
	}

	return res;
}
wchar_t scanner::next_char()
{
	if (current >= endPoint) return L'\0';
	if (encoding == Encoding::UTF16LE) {
		current += 2;
	} else {
		++current;
	}

	wchar_t res = current_char();
	return res;
}

void scanner::skip()
{
	//空白を飛ばす
	wchar_t ch1 = current_char();
	wchar_t ch2 = index_from_current_char(1);
	while (ch1 == '\r' || ch1 == '\n' || ch1 == L'\t' || ch1 == L' '
		|| ch1 == L'#' || (ch1 == L'/' && (ch2 == L'/' || ch2 == L'*'))) {
		//コメントを飛ばす
		if (ch1 == L'#' || (ch1 == L'/' && (ch2 == L'/' || ch2 == L'*'))) {
			if (ch1 == L'#' || ch2 == L'/') {
				do {
					ch1 = next_char();
				} while (ch1 != L'\r' && ch1 != L'\n' && ch1 != L'\0');
			} else {
				next_char();
				ch1 = next_char();
				ch2 = index_from_current_char(1);
				while (ch1 != L'*' || ch2 != L'/') {
					if (ch1 == L'\0') throw parser_error(L"Unterminated block comment");
					if (ch1 == L'\n')
						++line;
					ch1 = next_char();
					ch2 = index_from_current_char(1);
				}
				ch1 = next_char();
				ch1 = next_char();
			}
		} else if (ch1 == L'\n') {
			++line;
			ch1 = next_char();
		} else
			ch1 = next_char();
		ch2 = index_from_current_char(1);
	}
}

void scanner::AddLog(wchar_t* data)
{
	wchar_t* pStart = (wchar_t*)current;
	wchar_t* pEnd = (wchar_t*)(current + std::min<std::ptrdiff_t>(16, endPoint - current));
	std::wstring wstr = std::wstring(pStart, pEnd);
	// Logger::WriteTop(StringUtility::Format(L"%s current=%d, endPoint=%d, val=%d, ch=%s", data, pStart, endPoint, (wchar_t)*current, wstr.c_str()));
}

void scanner::advance()
{
	skip();

	wchar_t ch = current_char();
	if (ch == L'\0' || current >= endPoint) {
		next = tk_end;
		return;
	}

	switch (ch) {
	case L'[':
		next = tk_open_bra;
		ch = next_char();
		break;
	case L']':
		next = tk_close_bra;
		ch = next_char();
		break;
	case L'(':
		next = tk_open_par;
		ch = next_char();
		if (ch == L'|') {
			next = tk_open_abs;
			ch = next_char();
		}
		break;
	case L')':
		next = tk_close_par;
		ch = next_char();
		break;
	case L'{':
		next = tk_open_cur;
		ch = next_char();
		break;
	case L'}':
		next = tk_close_cur;
		ch = next_char();
		break;
	case L'@':
		next = tk_at;
		ch = next_char();
		break;
	case L',':
		next = tk_comma;
		ch = next_char();
		break;
	case L';':
		next = tk_semicolon;
		ch = next_char();
		break;
	case L'~':
		next = tk_tilde;
		ch = next_char();
		break;
	case L'*':
		next = tk_asterisk;
		ch = next_char();
		if (ch == L'=') {
			next = tk_multiply_assign;
			ch = next_char();
		}
		break;
	case L'/':
		next = tk_slash;
		ch = next_char();
		if (ch == L'=') {
			next = tk_divide_assign;
			ch = next_char();
		}
		break;
	case L'%':
		next = tk_percent;
		ch = next_char();
		if (ch == L'=') {
			next = tk_remainder_assign;
			ch = next_char();
		}
		break;
	case L'^':
		next = tk_caret;
		ch = next_char();
		if (ch == L'=') {
			next = tk_power_assign;
			ch = next_char();
		}
		break;
	case L'=':
		next = tk_assign;
		ch = next_char();
		if (ch == L'=') {
			next = tk_e;
			ch = next_char();
		}
		break;
	case L'>':
		next = tk_g;
		ch = next_char();
		if (ch == L'=') {
			next = tk_ge;
			ch = next_char();
		}
		break;
	case L'<':
		next = tk_l;
		ch = next_char();
		if (ch == L'=') {
			next = tk_le;
			ch = next_char();
		}
		break;
	case L'!':
		next = tk_exclamation;
		ch = next_char();
		if (ch == L'=') {
			next = tk_ne;
			ch = next_char();
		}
		break;
	case L'+':
		next = tk_plus;
		ch = next_char();
		if (ch == L'+') {
			next = tk_inc;
			ch = next_char();
		} else if (ch == L'=') {
			next = tk_add_assign;
			ch = next_char();
		}
		break;
	case L'-':
		next = tk_minus;
		ch = next_char();
		if (ch == L'-') {
			next = tk_dec;
			ch = next_char();
		} else if (ch == L'=') {
			next = tk_subtract_assign;
			ch = next_char();
		}
		break;
	case L'&':
		next = tk_ampersand;
		ch = next_char();
		if (ch == L'&') {
			next = tk_and_then;
			ch = next_char();
		}
		break;
	case L'|':
		next = tk_vertical;
		ch = next_char();
		if (ch == L'|') {
			next = tk_or_else;
			ch = next_char();
		} else if (ch == L')') {
			next = tk_close_abs;
			ch = next_char();
		}
		break;
	case L'.':
		ch = next_char();
		if (ch == L'.') {
			next = tk_range;
			ch = next_char();
		} else {
			std::wstring error;
			error += L"It's script does not allow to alone period\r\n";
			error += L"(単独のピリオドはこのスクリプトでは使いません)";
			throw parser_error(error);
		}
		break;

	case L'\'':
	case L'\"': {
		wchar_t q = current_char();
		next = (q == L'\"') ? tk_string : tk_char;
		ch = next_char();
		wchar_t pre = next;
		if (encoding == Encoding::UTF16LE) {
			std::wstring s;
			while (true) {
				if (ch == L'\0') throw parser_error(L"Unterminated string literal");
				if (ch == q && pre != L'\\')
					break;

				if (ch == L'\\') {
					if (pre == L'\\')
						s += ch;
				} else {
					s += ch;
				}

				pre = ch;
				ch = next_char();
			}
			ch = next_char();
			string_value = s;
		} else {
			std::string s;
			while (true) {
				if (ch == L'\0') throw parser_error(L"Unterminated string literal");
				if (ch == q && pre != L'\\')
					break;

				if (ch == L'\\') {
					if (pre == L'\\')
						s += *current;
				} else {
					s += *current;
				}

				pre = ch;

				if (IsDBCSLeadByteEx(Encoding::CP_SHIFT_JIS, ch)) {
					ch = next_char();
					s += ch;
				}
				ch = next_char();
			}
			ch = next_char();
			string_value = to_wide(s);
		}

		if (q == L'\'') {
			if (string_value.size() == 1)
				char_value = string_value[0];
			else
				throw parser_error(L"文字型の値の長さは1だけです");
		}
	} break;
	case L'\\': {
		ch = next_char();
		next = tk_char;
		wchar_t c = ch;
		ch = next_char();
		switch (c) {
		case L'0':
			char_value = L'\0';
			break;
		case L'n':
			char_value = L'\n';
			break;
		case L'r':
			char_value = L'\r';
			break;
		case L't':
			char_value = L'\t';
			break;
		case L'x':
			char_value = 0;
			while (std::isxdigit(ch)) {
				char_value = char_value * 16 + (ch >= L'a') ? ch - L'a' + 10 : (ch >= L'A') ? ch - L'A' + 10 : ch - L'0';
				ch = next_char();
			}
			break;
		default: {
			std::wstring error;
			error += L"There is a strange character.\r\n";
			error += L"特殊文字が変です(「\"...\"」を忘れていませんか)";
			throw parser_error(error);
		}
		}
	} break;
	default:
		if (std::iswdigit(ch)) {
			next = tk_real;
			real_value = 0.0;
			do {
				real_value = real_value * 10. + (ch - L'0');
				ch = next_char();
			} while (std::iswdigit(ch));

			wchar_t ch2 = index_from_current_char(1);
			if (ch == L'.' && std::iswdigit(ch2)) {
				ch = next_char();
				gstd::script_real d = 1;
				while (std::iswdigit(ch)) {
					d = d / 10;
					real_value = real_value + d * (ch - L'0');
					ch = next_char();
				}
			}
		} else if (std::iswalpha(ch) || ch == L'_') {
			next = tk_word;
			if (encoding == Encoding::UTF16LE) {
				word = "";
				do {
					word += (char)ch;
					ch = next_char();
				} while (std::iswalpha(ch) || ch == '_' || std::iswdigit(ch));
			} else {
				char* pStart = (char*)current;
				char* pEnd = pStart;
				do {
					ch = next_char();
					pEnd = (char*)current;
				} while (std::iswalpha(ch) || ch == '_' || std::iswdigit(ch));
				word = std::string(pStart, pEnd);
			}

			if (word == "alternative")
				next = tk_ALTERNATIVE;
			else if (word == "ascent")
				next = tk_ASCENT;
			else if (word == "break")
				next = tk_BREAK;
			else if (word == "case")
				next = tk_CASE;
			else if (word == "descent")
				next = tk_DESCENT;
			else if (word == "else")
				next = tk_ELSE;
			else if (word == "function")
				next = tk_FUNCTION;
			else if (word == "if")
				next = tk_IF;
			else if (word == "in")
				next = tk_IN;
			else if (word == "let" || word == "var")
				next = tk_LET;
			else if (word == "local")
				next = tk_LOCAL;
			else if (word == "loop")
				next = tk_LOOP;
			else if (word == "others")
				next = tk_OTHERS;
			else if (word == "real")
				next = tk_REAL;
			else if (word == "return")
				next = tk_RETURN;
			else if (word == "sub")
				next = tk_SUB;
			else if (word == "task")
				next = tk_TASK;
			else if (word == "times")
				next = tk_TIMES;
			else if (word == "while")
				next = tk_WHILE;
			else if (word == "yield")
				next = tk_YIELD;
		} else {
			next = tk_invalid;
		}
	}
}

/* operations */

value add(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);
	if (argv[0].get_type()->get_kind() == type_data::tk_array) {
		if (argv[0].get_type() != argv[1].get_type()) {
			std::wstring error;
			error += L"variable type mismatch\r\n";
			error += L"(型が一致しません)";
			machine->raise_error(error);
			return value();
		}
		if (argv[0].length_as_array() != argv[1].length_as_array()) {
			std::wstring error;
			error += L"array length mismatch\r\n";
			error += L"(長さが一致しません)";
			machine->raise_error(error);
			return value();
		}
		value result;
		for (unsigned i = 0; i < argv[1].length_as_array(); ++i) {
			value v[2];
			v[0] = argv[0].index_as_array(i);
			v[1] = argv[1].index_as_array(i);
			result.append(argv[1].get_type(), add(machine, 2, v));
		}
		return result;
	} else
		return value(machine->get_engine()->get_real_type(), argv[0].as_real() + argv[1].as_real());
}

value subtract(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);
	if (argv[0].get_type()->get_kind() == type_data::tk_array) {
		if (argv[0].get_type() != argv[1].get_type()) {
			std::wstring error;
			error += L"variable type mismatch\r\n";
			error += L"(型が一致しません)";
			machine->raise_error(error);
			return value();
		}
		if (argv[0].length_as_array() != argv[1].length_as_array()) {
			std::wstring error;
			error += L"array length mismatch\r\n";
			error += L"(長さが一致しません)";
			machine->raise_error(error);
			return value();
		}
		value result;
		for (unsigned i = 0; i < argv[1].length_as_array(); ++i) {
			value v[2];
			v[0] = argv[0].index_as_array(i);
			v[1] = argv[1].index_as_array(i);
			result.append(argv[1].get_type(), subtract(machine, 2, v));
		}
		return result;
	} else
		return value(machine->get_engine()->get_real_type(), argv[0].as_real() - argv[1].as_real());
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value multiply(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), argv[0].as_real() * argv[1].as_real());
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value divide(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), argv[0].as_real() / argv[1].as_real());
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value remainder(script_machine* machine, int argc, value const* argv)
{
	gstd::script_real x = argv[0].as_real();
	gstd::script_real y = argv[1].as_real();
	return value(machine->get_engine()->get_real_type(), fmodl2(x, y));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value modc(script_machine* machine, int argc, value const* argv)
{
	gstd::script_real x = argv[0].as_real();
	gstd::script_real y = argv[1].as_real();
	return value(machine->get_engine()->get_real_type(), std::fmod(x, y));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value negative(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), -argv[0].as_real());
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value power(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), std::pow(argv[0].as_real(), argv[1].as_real()));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value compare(script_machine* machine, int argc, value const* argv)
{
	if (argv[0].get_type() == argv[1].get_type()) {
		int r = 0;

		switch (argv[0].get_type()->get_kind()) {
		case type_data::tk_real: {
			gstd::script_real a = argv[0].as_real();
			gstd::script_real b = argv[1].as_real();
			r = (a == b) ? 0 : (a < b) ? -1 : 1;
		} break;

		case type_data::tk_char: {
			wchar_t a = argv[0].as_char();
			wchar_t b = argv[1].as_char();
			r = (a == b) ? 0 : (a < b) ? -1 : 1;
		} break;

		case type_data::tk_boolean: {
			bool a = argv[0].as_boolean();
			bool b = argv[1].as_boolean();
			r = (a == b) ? 0 : (a < b) ? -1 : 1;
		} break;

		case type_data::tk_array: {
			for (unsigned i = 0; i < argv[0].length_as_array(); ++i) {
				if (i >= argv[1].length_as_array()) {
					r = +1; //"123" > "12"
					break;
				}

				value v[2];
				v[0] = argv[0].index_as_array(i);
				v[1] = argv[1].index_as_array(i);
				r = compare(machine, 2, v).as_real();
				if (r != 0)
					break;
			}
			if (r == 0 && argv[0].length_as_array() < argv[1].length_as_array()) {
				r = -1; //"12" < "123"
			}
		} break;

		default:
			assert(false);
		}
		return value(machine->get_engine()->get_real_type(), static_cast<gstd::script_real>(r));
	} else {
		std::wstring error;
		error += L"Variables of different types are being compared\r\n";
		error += L"(型が違う値同士を比較しようとしました)";
		machine->raise_error(error);
		return value();
	}
}

value predecessor(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 1);
	assert(argv[0].has_data());
	switch (argv[0].get_type()->get_kind()) {
	case type_data::tk_real:
		return value(argv[0].get_type(), argv[0].as_real() - 1);
	case type_data::tk_char: {
		wchar_t c = argv[0].as_char();
		--c;
		return value(argv[0].get_type(), c);
	}
	case type_data::tk_boolean:
		return value(argv[0].get_type(), false);
	default:
		std::wstring error;
		error += L"This variables does not allow predecessor\r\n";
		error += L"(この型の値にpredecessorは使えません)";
		machine->raise_error(error);
		return value();
	}
}

value successor(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 1);
	assert(argv[0].has_data());
	switch (argv[0].get_type()->get_kind()) {
	case type_data::tk_real:
		return value(argv[0].get_type(), argv[0].as_real() + 1);

	case type_data::tk_char: {
		wchar_t c = argv[0].as_char();
		++c;
		return value(argv[0].get_type(), c);
	}
	case type_data::tk_boolean:
		return value(argv[0].get_type(), true);
	default: {
		std::wstring error;
		error += L"This variables does not allow successor\r\n";
		error += L"(この型の値にpredecessorは使えません)";
		machine->raise_error(error);
		return value();
	}
	}
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value true_(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_boolean_type(), true);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value false_(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_boolean_type(), false);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value not_(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_boolean_type(), !argv[0].as_boolean());
}

value length(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 1);
#ifdef __EMSCRIPTEN__
	if (!argv[0].has_data() || argv[0].get_type()->get_kind() != type_data::tk_array) {
		machine->raise_error(L"length requires an array; received " + (argv[0].has_data() ? argv[0].as_string() : L"(nil)"));
		return value();
	}
#endif
	return value(machine->get_engine()->get_real_type(), static_cast<gstd::script_real>(argv[0].length_as_array()));
}

value index(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);

	if (argv[0].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array index operation.\r\n";
		error += L"(配列以外にindexを使いました)";
		machine->raise_error(error);
		return value();
	}

	gstd::script_real index = argv[1].as_real();

	if (index != static_cast<int>(index)) {
		std::wstring error;
		error += L"Array index access does not allow to period.\r\n";
		error += L"(小数点以下があります)";
		machine->raise_error(error);
		return value();
	}

	if (index < 0 || index >= argv[0].length_as_array()) {
		std::wstring error;
		error += L"Array index out of bounds.\r\n";
		error += L"(配列のサイズを超えています)";
		machine->raise_error(error);
		return value();
	}

	value const& result = argv[0].index_as_array(index);
	return result;
}

value index_writable(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);

	if (argv[0].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array index operation.\r\n";
		error += L"(配列以外にindex!を使いました)";
		machine->raise_error(error);
		return value();
	}

	gstd::script_real index = argv[1].as_real();

	if (index != static_cast<int>(index)) {
		std::wstring error;
		error += L"Array index access does not allow to period.\r\n";
		error += L"(小数点以下があります)";
		machine->raise_error(error);
		return value();
	}

	if (index < 0 || index >= argv[0].length_as_array()) {
		std::wstring error;
		error += L"Array index out of bounds.\r\n";
		error += L"(配列のサイズを超えています)";
		machine->raise_error(error);
		return value();
	}

	value const& result = argv[0].index_as_array(index);
	result.unique();
	return result;
}

value slice(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 3);

	if (argv[0].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array slice operation.\r\n";
		error += L"(配列以外にindexを使いました)";
		machine->raise_error(error);
		return value();
	}

	gstd::script_real index_1 = argv[1].as_real();

	if (index_1 != static_cast<int>(index_1)) {
		std::wstring error;
		error += L"Array slicing does not allow to period.\r\n";
		error += L"(開始位置に小数点以下があります)";
		machine->raise_error(error);
		return value();
	}

	gstd::script_real index_2 = argv[2].as_real();

	if (index_2 != static_cast<int>(index_2)) {
		std::wstring error;
		error += L"Array slicing does not allow to period.\r\n";
		error += L"(終端位置に小数点以下があります)";
		machine->raise_error(error);
		return value();
	}

	if (index_1 < 0 || index_1 > index_2 || index_2 > argv[0].length_as_array()) {
		std::wstring error;
		error += L"Array index out of bounds.\r\n";
		error += L"(配列のサイズを超えています)";
		machine->raise_error(error);
		return value();
	}

	value result(argv[0].get_type(), std::wstring());

	for (int i = index_1; i < index_2; ++i) {
		result.append(result.get_type(), argv[0].index_as_array(i));
	}

	return result;
}

value erase(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);

	if (argv[0].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array erase operation.\r\n";
		error += L"(配列以外にeraseを使いました)";
		machine->raise_error(error);
		return value();
	}

	gstd::script_real index_1 = argv[1].as_real();
	double length = argv[0].length_as_array();

	if (index_1 != static_cast<int>(index_1)) {
		std::wstring error;
		error += L"Array erasing does not allow to period.\r\n";
		error += L"(削除位置に小数点以下があります)";
		machine->raise_error(error);
		return value();
	}

	if (index_1 < 0 || index_1 >= argv[0].length_as_array()) {
		std::wstring error;
		error += L"Array index out of bounds.\r\n";
		error += L"(配列のサイズを超えています)";
		machine->raise_error(error);
		return value();
	}

	value result(argv[0].get_type(), std::wstring());

	for (int i = 0; i < index_1; ++i) {
		result.append(result.get_type(), argv[0].index_as_array(i));
	}
	for (int i = index_1 + 1; i < length; ++i) {
		result.append(result.get_type(), argv[0].index_as_array(i));
	}
	return result;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value append(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);

	if (argv[0].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array append operation.\r\n";
		error += L"(配列以外にappendを使いました)";
		machine->raise_error(error);
		return value();
	}

	if (argv[0].length_as_array() > 0 && argv[0].get_type()->get_element() != argv[1].get_type()) {
		std::wstring error;
		error += L"variable type mismatch\r\n";
		error += L"(型が一致しません)";
		machine->raise_error(error);
		return value();
	}

	value result = argv[0];
	result.append(machine->get_engine()->get_array_type(argv[1].get_type()), argv[1]);
	return result;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value concatenate(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);

	if (argv[0].get_type()->get_kind() != type_data::tk_array || argv[1].get_type()->get_kind() != type_data::tk_array) {
		std::wstring error;
		error += L"This variables does not allow to array concatenate operation.\r\n";
		error += L"(配列以外にconcatenateを使いました)";
		machine->raise_error(error);
		return value();
	}

	if (argv[0].length_as_array() > 0 && argv[1].length_as_array() > 0 && argv[0].get_type() != argv[1].get_type()) {
		std::wstring error;
		error += L"variable type mismatch\r\n";
		error += L"(型が一致しません)";
		machine->raise_error(error);
		return value();
	}

	value result = argv[0];
	result.concatenate(argv[1]);
	return result;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value round(script_machine* machine, int argc, value const* argv)
{
	gstd::script_real r = std::floor(argv[0].as_real() + gstd::script_real(0.5));
	return value(machine->get_engine()->get_real_type(), r);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value truncate(script_machine* machine, int argc, value const* argv)
{
	gstd::script_real r = argv[0].as_real();
	r = (r > 0) ? std::floor(r) : std::ceil(r);
	return value(machine->get_engine()->get_real_type(), r);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value ceil(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), std::ceil(argv[0].as_real()));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value floor(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), std::floor(argv[0].as_real()));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value absolute(script_machine* machine, int argc, value const* argv)
{
	gstd::script_real r = std::fabs(argv[0].as_real());
	return value(machine->get_engine()->get_real_type(), r);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value pi(script_machine* machine, int argc, value const* argv)
{
	return value(machine->get_engine()->get_real_type(), (gstd::script_real)3.14159265358979323846);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
value assert_(script_machine* machine, int argc, value const* argv)
{
	assert(argc == 2);
	if (!argv[0].as_boolean()) {
		machine->raise_error(argv[1].as_string());
	}
	return value();
}

function const operations[] = {
	{ "true", true_, 0 },
	{ "false", false_, 0 },
	{ "pi", pi, 0 },
	{ "length", length, 1 },
	{ "not", not_, 1 },
	{ "negative", negative, 1 },
	{ "predecessor", predecessor, 1 },
	{ "successor", successor, 1 },
	{ "round", round, 1 },
	{ "trunc", truncate, 1 },
	{ "truncate", truncate, 1 },
	{ "ceil", ceil, 1 },
	{ "floor", floor, 1 },
	{ "absolute", absolute, 1 },
	{ "add", add, 2 },
	{ "subtract", subtract, 2 },
	{ "multiply", multiply, 2 },
	{ "divide", divide, 2 },
	{ "remainder", remainder, 2 },
	{ "modc", modc, 2 },
	{ "power", power, 2 },
	{ "index_", index, 2 },
	{ "index!", index_writable, 2 },
	{ "slice", slice, 3 },
	{ "erase", erase, 2 },
	{ "append", append, 2 },
	{ "concatenate", concatenate, 2 },
	{ "compare", compare, 2 },
	{ "assert", assert_, 2 }
};

#if defined(PH3_SCRIPT_FAST_ARITHMETIC) || defined(PH3_SCRIPT_FAST_BUILTINS)
script_engine::code::code(int the_line, command_kind the_command, block* the_sub, int the_arguments)
	: command(the_command), line(the_line), sub(the_sub), arguments(the_arguments)
{
	// Match immutable native function identity, never a script/API function
	// name. Non-real operands still call the original implementation, including
	// recursive vector arithmetic, conversions and all original error paths.
	if(the_command!=pc_call_and_push_result || the_sub==NULL)return;
#ifdef PH3_SCRIPT_FAST_ARITHMETIC
	if(the_arguments==1 && the_sub->func==::negative)command=pc_real_negative;
	else if(the_arguments==1 && the_sub->func==::predecessor)command=pc_real_predecessor;
	else if(the_arguments==1 && the_sub->func==::successor)command=pc_real_successor;
	else if(the_arguments==2){
		if(the_sub->func==::add)command=pc_real_add;
		else if(the_sub->func==::subtract)command=pc_real_subtract;
		else if(the_sub->func==::multiply)command=pc_real_multiply;
		else if(the_sub->func==::divide)command=pc_real_divide;
		else if(the_sub->func==::compare)command=pc_real_compare;
	}
#endif
#ifdef PH3_SCRIPT_FAST_BUILTINS
	if(the_arguments==2&&the_sub->func==static_cast<callback>(::index))command=pc_index_read;
	else if(the_arguments==1&&the_sub->func==::length)command=pc_array_length;
	else if(the_arguments==1&&the_sub->func==::not_)command=pc_boolean_not;
	else if(the_arguments==0&&the_sub->func==::true_)command=pc_boolean_true;
	else if(the_arguments==0&&the_sub->func==::false_)command=pc_boolean_false;
#endif
}
#endif

/* parser */

class gstd::parser {
public:
	struct symbol {
		int level;
		script_engine::block* sub;
		int variable;
	};

	struct scope
#ifdef __EMSCRIPTEN__
		: public std::unordered_map<std::string, symbol>
#else
		: public std::map<std::string, symbol>
#endif
	{
		script_engine::block_kind kind;

		scope(script_engine::block_kind the_kind)
			: kind(the_kind)
		{
		}
	};

	std::vector<scope> frame;
	scanner* lex;
	script_engine* engine;
	bool error;
	std::wstring error_message;
	int error_line;
	std::map<std::string, script_engine::block*> events;

	parser(script_engine* e, scanner* s, int funcc, function const* funcv);

	virtual ~parser()
	{
	}

	void parse_parentheses(script_engine::block* block);
	void parse_clause(script_engine::block* block);
	void parse_prefix(script_engine::block* block);
	void parse_suffix(script_engine::block* block);
	void parse_product(script_engine::block* block);
	void parse_sum(script_engine::block* block);
	void parse_comparison(script_engine::block* block);
	void parse_logic(script_engine::block* block);
	void parse_expression(script_engine::block* block);
	int parse_arguments(script_engine::block* block);
	void parse_statements(script_engine::block* block);
	void parse_inline_block(script_engine::block* block, script_engine::block_kind kind);
	void parse_block(script_engine::block* block, std::vector<std::string> const* args, bool adding_result);

private:
	void register_function(function const& func);
	symbol* search(std::string const& name);
	symbol* search_result();
	void scan_current_scope(int level, std::vector<std::string> const* args, bool adding_result);
	void write_operation(script_engine::block* block, char const* name, int clauses);

	typedef script_engine::code code;
};

parser::parser(script_engine* e, scanner* s, int funcc, function const* funcv)
	: engine(e)
	, lex(s)
	, frame()
	, error(false)
{
	frame.push_back(scope(script_engine::bk_normal));
#ifdef __EMSCRIPTEN__
	// A ph3 root scope contains the built-ins plus hundreds of engine functions,
	// and every identifier parsed performs scope lookups. Avoid tree walks and
	// repeated rehashing in the Web compiler; scope ordering is never observed.
	frame[0].reserve(funcc + sizeof(operations) / sizeof(function) + 64);
#endif

	for (int i = 0; i < sizeof(operations) / sizeof(function); ++i)
		register_function(operations[i]);

	for (int i = 0; i < funcc; ++i)
		register_function(funcv[i]);

	try {
		scan_current_scope(0, NULL, false);
		parse_statements(engine->main_block);
		if (lex->next != tk_end) {
			std::wstring error;
			error += L"Unable to be interpreted (Don't forget \";\"s).\r\n";
			error += L"(解釈できないものがあります(「;」を忘れていませんか))";
			throw parser_error(error);
		}
	} catch (parser_error& e) {
		error = true;
		error_message = e.what();
		error_line = lex->line;
	}
}

void parser::register_function(function const& func)
{
	symbol s;
	s.level = 0;
	s.sub = engine->new_block(0, script_engine::bk_function);
	s.sub->arguments = func.arguments;
	s.sub->name = func.name;
	s.sub->func = func.func;
	s.variable = -1;
	frame[0][func.name] = s;
}

parser::symbol* parser::search(std::string const& name)
{
	for (int i = frame.size() - 1; i >= 0; --i) {
		if (frame[i].find(name) != frame[i].end())
			return &(frame[i][name]);
	}
	return NULL;
}

parser::symbol* parser::search_result()
{
	for (int i = frame.size() - 1; i >= 0; --i) {
		if (frame[i].find("result") != frame[i].end())
			return &(frame[i]["result"]);
		if (frame[i].kind == script_engine::bk_sub || frame[i].kind == script_engine::bk_microthread)
			return NULL;
	}
	return NULL;
}

void parser::scan_current_scope(int level, std::vector<std::string> const* args, bool adding_result)
{
	//先読みして識別子を登録する
	scanner lex2(*lex);
	try {
		scope* current_frame = &frame[frame.size() - 1];
		int cur = 0;
		int var = 0;

		if (adding_result) {
			symbol s;
			s.level = level;
			s.sub = NULL;
			s.variable = var;
			++var;
			(*current_frame)["result"] = s;
		}

		if (args != NULL) {
			for (unsigned i = 0; i < args->size(); ++i) {
				symbol s;
				s.level = level;
				s.sub = NULL;
				s.variable = var;
				++var;
				(*current_frame)[(*args)[i]] = s;
			}
		}

		while (cur >= 0 && lex2.next != tk_end && lex2.next != tk_invalid) {
			switch (lex2.next) {
			case tk_open_cur:
				++cur;
				lex2.advance();
				break;
			case tk_close_cur:
				--cur;
				lex2.advance();
				break;
			case tk_at:
			case tk_SUB:
			case tk_FUNCTION:
			case tk_TASK: {
				token_kind type = lex2.next;
				lex2.advance();
				if (cur == 0) {
					if ((*current_frame).find(lex2.word) != (*current_frame).end()) {
						std::wstring error;
						error += L"Functions and variables of the same name are declared in the same scope.\r\n";
						error += L"(同じスコープで同名のルーチンが複数宣言されています)";
						throw parser_error(error);
					}
					script_engine::block_kind kind = (type == tk_SUB || type == tk_at) ? script_engine::bk_sub : (type == tk_FUNCTION) ? script_engine::bk_function : script_engine::bk_microthread;

					symbol s;
					s.level = level;
					s.sub = engine->new_block(level + 1, kind);
					s.sub->name = lex2.word;
					s.sub->func = NULL;
					s.variable = -1;
					(*current_frame)[lex2.word] = s;
					lex2.advance();
					if (kind != script_engine::bk_sub && lex2.next == tk_open_par) {
						lex2.advance();
						while (lex2.next == tk_word || lex2.next == tk_LET || lex2.next == tk_REAL) {
							++(s.sub->arguments);
							if (lex2.next == tk_LET || lex2.next == tk_REAL)
								lex2.advance();
							if (lex2.next == tk_word)
								lex2.advance();
							if (lex2.next != tk_comma)
								break;
							lex2.advance();
						}
					}
				}
			} break;
			case tk_REAL:
			case tk_LET:
				lex2.advance();
				if (cur == 0) {
#ifdef __SCRIPT_H__NO_CHECK_DUPLICATED
					if (lex2.word == "result") {
#endif
						if ((*current_frame).find(lex2.word) != (*current_frame).end()) {
							std::wstring error;
							error += L"Variables of the same name are declared in the same scope.\r\n";
							error += L"(同じスコープで同名の変数が複数宣言されています)";
							throw parser_error(error);
						}
#ifdef __SCRIPT_H__NO_CHECK_DUPLICATED
					}
#endif
					symbol s;
					s.level = level;
					s.sub = NULL;
					s.variable = var;
					++var;
					(*current_frame)[lex2.word] = s;

					lex2.advance();
				}
				break;
			default:
				lex2.advance();
				break;
			}
		}
	} catch (...) {
		lex->line = lex2.line;
		throw;
	}
}

void parser::write_operation(script_engine::block* block, char const* name, int clauses)
{
	symbol* s = search(name);
	assert(s != NULL);
	if (s->sub->arguments != clauses) {
		std::wstring error;
		error += L"Overwriting function does not allow to different argument count.\r\n";
		error += L"(演算子に対応する関数が上書き定義されましたが引数の数が違います)";
		throw parser_error(error);
	}

	block->codes.push_back(script_engine::code(lex->line, script_engine::pc_call_and_push_result, s->sub, clauses));
}

void parser::parse_parentheses(script_engine::block* block)
{
	if (lex->next != tk_open_par) {
		std::wstring error;
		error += L"\"(\" is nessasary.\r\n";
		error += L"(\"(\"が必要です)";
		throw parser_error(error);
	}
	lex->advance();

	parse_expression(block);

	if (lex->next != tk_close_par) {
		std::wstring error;
		error += L"\")\" is nessasary.\r\n";
		error += L"(\")\"が必要です)";
		throw parser_error(error);
	}
	lex->advance();
}

void parser::parse_clause(script_engine::block* block)
{
	if (lex->next == tk_real) {
		block->codes.push_back(engine->make_literal(lex->line, value(engine->get_real_type(), lex->real_value)));
		lex->advance();
	} else if (lex->next == tk_char) {
		block->codes.push_back(engine->make_literal(lex->line, value(engine->get_char_type(), lex->char_value)));
		lex->advance();
	} else if (lex->next == tk_string) {
		std::wstring str = lex->string_value;
		lex->advance();
		while (lex->next == tk_string || lex->next == tk_char) {
			str += (lex->next == tk_string) ? lex->string_value : (std::wstring() + lex->char_value);
			lex->advance();
		}
		block->codes.push_back(engine->make_literal(lex->line, value(engine->get_string_type(), str)));
	} else if (lex->next == tk_word) {
		symbol* s = search(lex->word);
		if (s == NULL) {
			std::wstring error;
			error += StringUtility::FormatToWide("%s is not defined.\r\n", lex->word.c_str());
			error += StringUtility::FormatToWide("(%sは未定義の識別子です)", lex->word.c_str());
			throw parser_error(error);
		}

		lex->advance();

		if (s->sub != NULL) {
			if (s->sub->kind != script_engine::bk_function) {
				std::wstring error;
				error += L"sub and task cannot call in the statement.\r\n";
				error += L"(subやtaskは式中で呼べません)";
				throw parser_error(error);
			}

			int argc = parse_arguments(block);

			if (argc != s->sub->arguments) {
				std::wstring error;
				error += StringUtility::FormatToWide(
					"%s incorrect number of parameters. Check to make sure you have the correct number of parameters.\r\n",
					s->sub->name.c_str());
				error += StringUtility::FormatToWide("(%sの引数の数が違います)", s->sub->name.c_str());
				throw parser_error(error);
			}

			block->codes.push_back(code(lex->line, script_engine::pc_call_and_push_result, s->sub, argc));
		} else {
			//変数
			block->codes.push_back(code(lex->line, script_engine::pc_push_variable, s->level, s->variable));
		}
	} else if (lex->next == tk_open_bra) {
		lex->advance();
		block->codes.push_back(engine->make_literal(lex->line, value(engine->get_string_type(), std::wstring())));
		while (lex->next != tk_close_bra) {
			parse_expression(block);
			write_operation(block, "append", 2);
			if (lex->next != tk_comma)
				break;
			lex->advance();
		}
		if (lex->next != tk_close_bra) {
			std::wstring error;
			error += L"\"]\" is nessasary.\r\n";
			error += L"(\"]\"が必要です)";
			throw parser_error(error);
		}
		lex->advance();
	} else if (lex->next == tk_open_abs) {
		lex->advance();
		parse_expression(block);
		write_operation(block, "absolute", 1);
		if (lex->next != tk_close_abs) {
			std::wstring error;
			error += L"\"|\" is nessasary.\r\n";
			error += L"(\"|)\"が必要です)";
			throw parser_error(error);
		}
		lex->advance();
	} else if (lex->next == tk_open_par) {
		parse_parentheses(block);
	} else {
		std::wstring error;
		error += L"Invalid expression.\r\n";
		error += L"(項として無効な式があります)";
		throw parser_error(error);
	}
}

void parser::parse_suffix(script_engine::block* block)
{
	parse_clause(block);
	if (lex->next == tk_caret) {
		lex->advance();
		parse_suffix(block); //再帰
		write_operation(block, "power", 2);
	} else {
		while (lex->next == tk_open_bra) {
			lex->advance();
			parse_expression(block);

			if (lex->next == tk_range) {
				lex->advance();
				parse_expression(block);
				write_operation(block, "slice", 3);
			} else {
				write_operation(block, "index_", 2);
			}

			if (lex->next != tk_close_bra) {
				std::wstring error;
				error += L"\"]\" is nessasary.\r\n";
				error += L"(\"]\"が必要です)";
				throw parser_error(error);
			}
			lex->advance();
		}
	}
}

void parser::parse_prefix(script_engine::block* block)
{
	if (lex->next == tk_plus) {
		lex->advance();
		parse_prefix(block); //再帰
	} else if (lex->next == tk_minus) {
		lex->advance();
		parse_prefix(block); //再帰
		write_operation(block, "negative", 1);
	} else if (lex->next == tk_exclamation) {
		lex->advance();
		parse_prefix(block); //再帰
		write_operation(block, "not", 1);
	} else {
		parse_suffix(block);
	}
}

void parser::parse_product(script_engine::block* block)
{
	parse_prefix(block);
	while (lex->next == tk_asterisk || lex->next == tk_slash || lex->next == tk_percent) {
		char const* name = (lex->next == tk_asterisk) ? "multiply" : (lex->next == tk_slash) ? "divide" : "remainder";
		lex->advance();
		parse_prefix(block);
		write_operation(block, name, 2);
	}
}

void parser::parse_sum(script_engine::block* block)
{
	parse_product(block);
	while (lex->next == tk_tilde || lex->next == tk_plus || lex->next == tk_minus) {
		char const* name = (lex->next == tk_tilde) ? "concatenate" : (lex->next == tk_plus) ? "add" : "subtract";
		lex->advance();
		parse_product(block);
		write_operation(block, name, 2);
	}
}

void parser::parse_comparison(script_engine::block* block)
{
	parse_sum(block);
	switch (lex->next) {
	case tk_assign: {
		std::wstring error;
		error += L"Do you not mistake it for \"==\"?\r\n";
		error += L"(\"==\"と間違えてませんか？)";
		throw parser_error(error);
	}

	case tk_e:
	case tk_g:
	case tk_ge:
	case tk_l:
	case tk_le:
	case tk_ne:
		token_kind op = lex->next;
		lex->advance();
		parse_sum(block);
		write_operation(block, "compare", 2);
		switch (op) {
		case tk_e:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_e));
			break;
		case tk_g:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_g));
			break;
		case tk_ge:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_ge));
			break;
		case tk_l:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_l));
			break;
		case tk_le:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_le));
			break;
		case tk_ne:
			block->codes.push_back(code(lex->line, script_engine::pc_compare_ne));
			break;
		}
		break;
	}
}

void parser::parse_logic(script_engine::block* block)
{
	parse_comparison(block);
	while (lex->next == tk_and_then || lex->next == tk_or_else) {
		script_engine::command_kind cmd = (lex->next == tk_and_then) ? script_engine::pc_case_if_not : script_engine::pc_case_if;
		lex->advance();

		block->codes.push_back(code(lex->line, script_engine::pc_dup));
		block->codes.push_back(code(lex->line, script_engine::pc_case_begin));
		block->codes.push_back(code(lex->line, cmd));
		block->codes.push_back(code(lex->line, script_engine::pc_pop));

		parse_comparison(block);

		block->codes.push_back(code(lex->line, script_engine::pc_case_end));
	}
}

void parser::parse_expression(script_engine::block* block)
{
	parse_logic(block);
}

int parser::parse_arguments(script_engine::block* block)
{
	int result = 0;
	if (lex->next == tk_open_par) {
		lex->advance();
		while (lex->next != tk_close_par) {
			++result;
			parse_expression(block);
			if (lex->next != tk_comma)
				break;
			lex->advance();
		}
		if (lex->next != tk_close_par) {
			std::wstring error;
			error += L"\")\" is nessasary.\r\n";
			error += L"(\")\"が必要です)";
			throw parser_error(error);
		}
		lex->advance();
	}
	return result;
}

void parser::parse_statements(script_engine::block* block)
{
	while (true) {
		bool need_semicolon = true;

		if (lex->next == tk_word) {
			symbol* s = search(lex->word);
			if (s == NULL) {
				std::wstring error;
				error += StringUtility::FormatToWide("%s is not defined.\r\n", lex->word.c_str());
				error += StringUtility::FormatToWide("(%sは未定義の識別子です)", lex->word.c_str());
				throw parser_error(error);
			}
			lex->advance();
			switch (lex->next) {
			case tk_assign:
				lex->advance();
				parse_expression(block);
				block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
				break;

			case tk_open_bra:
				block->codes.push_back(code(lex->line, script_engine::pc_push_variable_writable, s->level, s->variable));
				lex->advance();
				parse_expression(block);
				if (lex->next != tk_close_bra) {
					std::wstring error;
					error += L"\"]\" is nessasary.\r\n";
					error += L"(\"]\"が必要です)";
					throw parser_error(error);
				}
				lex->advance();
				write_operation(block, "index!", 2);
				if (lex->next != tk_assign) {
					std::wstring error;
					error += L"\"=\" is nessasary.\r\n";
					error += L"(\"=\"が必要です)";
					throw parser_error(error);
				}
				lex->advance();
				parse_expression(block);
				block->codes.push_back(code(lex->line, script_engine::pc_assign_writable));
				break;

			case tk_add_assign:
			case tk_subtract_assign:
			case tk_multiply_assign:
			case tk_divide_assign:
			case tk_remainder_assign:
			case tk_power_assign: {
				char const* f;
				switch (lex->next) {
				case tk_add_assign:
					f = "add";
					break;
				case tk_subtract_assign:
					f = "subtract";
					break;
				case tk_multiply_assign:
					f = "multiply";
					break;
				case tk_divide_assign:
					f = "divide";
					break;
				case tk_remainder_assign:
					f = "remainder";
					break;
				case tk_power_assign:
					f = "power";
					break;
				}
				lex->advance();

				block->codes.push_back(code(lex->line, script_engine::pc_push_variable, s->level, s->variable));

				parse_expression(block);
				write_operation(block, f, 2);

				block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
			} break;

			case tk_inc:
			case tk_dec: {
				char const* f = (lex->next == tk_inc) ? "successor" : "predecessor";
				lex->advance();

				block->codes.push_back(code(lex->line, script_engine::pc_push_variable, s->level, s->variable));
				write_operation(block, f, 1);
				block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
			} break;
			default:
				//関数, sub呼出し
				if (s->sub == NULL) {
					std::wstring error;
					error += L"You cannot call a variable as if it were a function or a subroutine.\r\n";
					error += L"(変数は関数やsubのようには呼べません)";
					throw parser_error(error);
				}

				int argc = parse_arguments(block);

				if (argc != s->sub->arguments) {
					std::wstring error;
					error += StringUtility::FormatToWide(
						"%s incorrect number of parameters. Check to make sure you have the correct number of parameters.\r\n",
						s->sub->name.c_str());
					error += StringUtility::FormatToWide("(%sの引数の数が違います)", s->sub->name.c_str());
					throw parser_error(error);
				}

				block->codes.push_back(code(lex->line, script_engine::pc_call, s->sub, argc));
			}
		} else if (lex->next == tk_LET || lex->next == tk_REAL) {
			lex->advance();

			if (lex->next != tk_word) {
				std::wstring error;
				error += L"Symbol name is nessasary.\r\n";
				error += L"(識別子が必要です)";
				throw parser_error(error);
			}

			symbol* s = search(lex->word);

			lex->advance();
			if (lex->next == tk_assign) {
				lex->advance();
				parse_expression(block);
				block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
			}
		} else if (lex->next == tk_LOCAL) {
			lex->advance();
			parse_inline_block(block, script_engine::bk_normal);
			need_semicolon = false;
		} else if (lex->next == tk_LOOP) {
			lex->advance();
			if (lex->next == tk_open_par) {
				parse_parentheses(block);
				int ip = block->codes.length;
				block->codes.push_back(code(lex->line, script_engine::pc_loop_count));
				parse_inline_block(block, script_engine::bk_loop);
				block->codes.push_back(code(lex->line, script_engine::pc_loop_back, ip));
				block->codes.push_back(code(lex->line, script_engine::pc_pop));
			} else {
				int ip = block->codes.length;
				parse_inline_block(block, script_engine::bk_loop);
				block->codes.push_back(code(lex->line, script_engine::pc_loop_back, ip));
			}
			need_semicolon = false;
		} else if (lex->next == tk_TIMES) {
			lex->advance();
			parse_parentheses(block);
			int ip = block->codes.length;
			if (lex->next == tk_LOOP) {
				lex->advance();
			}
			block->codes.push_back(code(lex->line, script_engine::pc_loop_count));
			parse_inline_block(block, script_engine::bk_loop);
			block->codes.push_back(code(lex->line, script_engine::pc_loop_back, ip));
			block->codes.push_back(code(lex->line, script_engine::pc_pop));
			need_semicolon = false;
		} else if (lex->next == tk_WHILE) {
			lex->advance();
			int ip = block->codes.length;
			parse_parentheses(block);
			if (lex->next == tk_LOOP) {
				lex->advance();
			}
			block->codes.push_back(code(lex->line, script_engine::pc_loop_if));
			parse_inline_block(block, script_engine::bk_loop);
			block->codes.push_back(code(lex->line, script_engine::pc_loop_back, ip));
			need_semicolon = false;
		} else if (lex->next == tk_ASCENT || lex->next == tk_DESCENT) {
			bool back = lex->next == tk_DESCENT;
			lex->advance();

			if (lex->next != tk_open_par) {
				std::wstring error;
				error += L"\"(\" is nessasary.\r\n";
				error += L"(\"(\"が必要です)";
				throw parser_error(error);
			}
			lex->advance();

			if (lex->next == tk_LET || lex->next == tk_REAL) {
				lex->advance();
			}

			if (lex->next != tk_word) {
				std::wstring error;
				error += L"The symbol name is nessasary.\r\n";
				error += L"(識別子が必要です)";
				throw parser_error(error);
			}

			std::string s = lex->word;

			lex->advance();

			if (lex->next != tk_IN) {
				std::wstring error;
				error += L"\"in\" is nessasary.\r\n";
				error += L"(inが必要です)";
				throw parser_error(error);
			}
			lex->advance();

			parse_expression(block);

			if (lex->next != tk_range) {
				std::wstring error;
				error += L"\"..\" is nessasary.\r\n";
				error += L"(\"..\"が必要です)";
				throw parser_error(error);
			}
			lex->advance();

			parse_expression(block);

			if (lex->next != tk_close_par) {
				std::wstring error;
				error += L"\")\" is nessasary.\r\n";
				error += L"(\")\"が必要です)";
				throw parser_error(error);
			}
			lex->advance();

			if (lex->next == tk_LOOP) {
				lex->advance();
			}

			if (!back) {
				block->codes.push_back(code(lex->line, script_engine::pc_swap));
			}

			int ip = block->codes.length;

			block->codes.push_back(code(lex->line, script_engine::pc_dup2));
			write_operation(block, "compare", 2);

			block->codes.push_back(code(lex->line, back ? script_engine::pc_loop_descent : script_engine::pc_loop_ascent));

			if (back) {
				write_operation(block, "predecessor", 1);
			}

			script_engine::block* b = engine->new_block(block->level + 1, script_engine::bk_loop);
			std::vector<std::string> counter;
			counter.push_back(s);
			parse_block(b, &counter, false);
			block->codes.push_back(code(lex->line, script_engine::pc_dup));
			block->codes.push_back(code(lex->line, script_engine::pc_call, b, 1));

			if (!back) {
				write_operation(block, "successor", 1);
			}

			block->codes.push_back(code(lex->line, script_engine::pc_loop_back, ip));
			block->codes.push_back(code(lex->line, script_engine::pc_pop));
			block->codes.push_back(code(lex->line, script_engine::pc_pop));

			need_semicolon = false;
		} else if (lex->next == tk_IF) {
			lex->advance();
			block->codes.push_back(code(lex->line, script_engine::pc_case_begin));

			parse_parentheses(block);
			block->codes.push_back(code(lex->line, script_engine::pc_case_if_not));
			parse_inline_block(block, script_engine::bk_normal);
			while (lex->next == tk_ELSE) {
				lex->advance();
				block->codes.push_back(code(lex->line, script_engine::pc_case_next));
				if (lex->next == tk_IF) {
					lex->advance();
					parse_parentheses(block);
					block->codes.push_back(code(lex->line, script_engine::pc_case_if_not));
					parse_inline_block(block, script_engine::bk_normal);
				} else {
					parse_inline_block(block, script_engine::bk_normal);
					break;
				}
			}

			block->codes.push_back(code(lex->line, script_engine::pc_case_end));
			need_semicolon = false;
		} else if (lex->next == tk_ALTERNATIVE) {
			lex->advance();
			parse_parentheses(block);
			block->codes.push_back(code(lex->line, script_engine::pc_case_begin));
			while (lex->next == tk_CASE) {
				lex->advance();

				if (lex->next != tk_open_par) {
					std::wstring error;
					error += L"\"(\" is nessasary.\r\n";
					error += L"(\"(\"が必要です)";
					throw parser_error(error);
				}
				block->codes.push_back(code(lex->line, script_engine::pc_case_begin));
				do {
					lex->advance();

					block->codes.push_back(code(lex->line, script_engine::pc_dup));
					parse_expression(block);
					write_operation(block, "compare", 2);
					block->codes.push_back(code(lex->line, script_engine::pc_compare_e));
					block->codes.push_back(code(lex->line, script_engine::pc_dup));
					block->codes.push_back(code(lex->line, script_engine::pc_case_if));
					block->codes.push_back(code(lex->line, script_engine::pc_pop));

				} while (lex->next == tk_comma);
				block->codes.push_back(engine->make_literal(lex->line, value(engine->get_boolean_type(), false)));
				block->codes.push_back(code(lex->line, script_engine::pc_case_end));
				if (lex->next != tk_close_par) {
					std::wstring error;
					error += L"\")\" is nessasary.\r\n";
					error += L"(\")\"が必要です)";
					throw parser_error(error);
				}
				lex->advance();

				block->codes.push_back(code(lex->line, script_engine::pc_case_if_not));
				block->codes.push_back(code(lex->line, script_engine::pc_pop));
				parse_inline_block(block, script_engine::bk_normal);
				block->codes.push_back(code(lex->line, script_engine::pc_case_next));
			}
			if (lex->next == tk_OTHERS) {
				lex->advance();
				block->codes.push_back(code(lex->line, script_engine::pc_pop));
				parse_inline_block(block, script_engine::bk_normal);
			} else {
				block->codes.push_back(code(lex->line, script_engine::pc_pop));
			}
			block->codes.push_back(code(lex->line, script_engine::pc_case_end));
			need_semicolon = false;
		} else if (lex->next == tk_BREAK) {
			lex->advance();
			block->codes.push_back(code(lex->line, script_engine::pc_break_loop));
		} else if (lex->next == tk_RETURN) {
			lex->advance();
			switch (lex->next) {
			case tk_end:
			case tk_invalid:
			case tk_semicolon:
			case tk_close_cur:
				break;
			default:
				parse_expression(block);
				symbol* s = search_result();
				if (s == NULL) {
					std::wstring error;
					error += L"\"return\" can call in function only.\r\n";
					error += L"(ここはfunctionの中ではありません)";
					throw parser_error(error);
				}

				block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
			}
			block->codes.push_back(code(lex->line, script_engine::pc_break_routine));
		} else if (lex->next == tk_YIELD) {
			lex->advance();
			block->codes.push_back(code(lex->line, script_engine::pc_yield));
		} else if (lex->next == tk_at || lex->next == tk_SUB || lex->next == tk_FUNCTION || lex->next == tk_TASK) {
			bool is_event = lex->next == tk_at;

			lex->advance();
			if (lex->next != tk_word) {
				std::wstring error;
				error += L"Symbol name is nessasary.\r\n";
				error += L"(識別子が必要です)";
				throw parser_error(error);
			}

			symbol* s = search(lex->word);

			if (is_event) {
				if (s->sub->level > 1) {
					std::wstring error;
					error += L"\"@\" cannot use in inner function and task.\r\n";
					error += L"(イベントを深い階層に記述することはできません)";
					throw parser_error(error);
				}
				events[s->sub->name] = s->sub;
			}

			lex->advance();

			std::vector<std::string> args;
			if (s->sub->kind != script_engine::bk_sub) {
				if (lex->next == tk_open_par) {
					lex->advance();
					while (lex->next == tk_word || lex->next == tk_LET || lex->next == tk_REAL) {
						if (lex->next == tk_LET || lex->next == tk_REAL) {
							lex->advance();
							if (lex->next != tk_word) {
								std::wstring error;
								error += L"Function parameter is nessasary.\r\n";
								error += L"(仮引数が必要です)";
								throw parser_error(error);
							}
						}
						args.push_back(lex->word);
						lex->advance();
						if (lex->next != tk_comma)
							break;
						lex->advance();
					}
					if (lex->next != tk_close_par) {
						std::wstring error;
						error += L"\")\" is nessasary.\r\n";
						error += L"(\")\"が必要です)";
						throw parser_error(error);
					}
					lex->advance();
				}
			} else {
				//互換性のため空の括弧だけ許す
				if (lex->next == tk_open_par) {
					lex->advance();
					if (lex->next != tk_close_par) {
						std::wstring error;
						error += L"\")\" is nessasary.\r\n";
						error += L"(\")\"が必要…というか\"(\"要らんです)";
						throw parser_error(error);
					}
					lex->advance();
				}
			}
			parse_block(s->sub, &args, s->sub->kind == script_engine::bk_function);
			need_semicolon = false;
		}

		//セミコロンが無いと継続しない
		if (need_semicolon && lex->next != tk_semicolon)
			break;

		if (lex->next == tk_semicolon)
			lex->advance();
	}
}

void parser::parse_inline_block(script_engine::block* block, script_engine::block_kind kind)
{
	script_engine::block* b = engine->new_block(block->level + 1, kind);
	parse_block(b, NULL, false);
	block->codes.push_back(code(lex->line, script_engine::pc_call, b, 0));
}

void parser::parse_block(script_engine::block* block, std::vector<std::string> const* args, bool adding_result)
{
	if (lex->next != tk_open_cur) {
		std::wstring error;
		error += L"\"{\" is nessasary.\r\n";
		error += L"(\"{\"が必要です)";
		throw parser_error(error);
	}
	lex->advance();

	frame.push_back(scope(block->kind));

	scan_current_scope(block->level, args, adding_result);

	if (args != NULL) {
		for (unsigned i = 0; i < args->size(); ++i) {
			symbol* s = search((*args)[i]);
			block->codes.push_back(code(lex->line, script_engine::pc_assign, s->level, s->variable));
		}
	}
	parse_statements(block);

	frame.pop_back();

	if (lex->next != tk_close_cur) {
		std::wstring error;
		error += L"\"}\" is nessasary.\r\n";
		error += L"(\"}\"が必要です)";
		throw parser_error(error);
	}
	lex->advance();
}

/* script_type_manager */

script_type_manager::script_type_manager()
{
	real_type = &*types.insert(types.end(), type_data(type_data::tk_real));
	char_type = &*types.insert(types.end(), type_data(type_data::tk_char));
	boolean_type = &*types.insert(types.end(), type_data(type_data::tk_boolean));
	string_type = &*types.insert(types.end(), type_data(type_data::tk_array, char_type));
}

type_data* script_type_manager::get_array_type(type_data* element)
{
	for (std::list<type_data>::iterator i = types.begin(); i != types.end(); ++i) {
		if (i->get_kind() == type_data::tk_array && i->get_element() == element) {
			return &*i;
		}
	}
	return &*types.insert(types.end(), type_data(type_data::tk_array, element));
}

/* script_engine */

script_engine::script_engine(script_type_manager* a_type_manager, std::string const& source, int funcc, function const* funcv)
	: type_manager(a_type_manager)
{
	main_block = new_block(0, bk_normal);

	const char* end = &source[0] + source.size();
	scanner s(source.c_str(), end);
	parser p(this, &s, funcc, funcv);

	events = p.events;

	error = p.error;
	error_message = p.error_message;
	error_line = p.error_line;
}

script_engine::script_engine(script_type_manager* a_type_manager, std::vector<char> const& source, int funcc, function const* funcv)
	: type_manager(a_type_manager)
{
	main_block = new_block(0, bk_normal);

	if (false) {
		wchar_t* pStart = (wchar_t*)&source[0];
		wchar_t* pEnd = (wchar_t*)(&source[0] + std::min<std::size_t>(source.size(), 64));
		std::wstring str = std::wstring(pStart, pEnd);
		// Logger::WriteTop(str);
	}
	const char* end = &source[0] + source.size();
	scanner s(&source[0], end);
	parser p(this, &s, funcc, funcv);

	events = p.events;

	error = p.error;
	error_message = p.error_message;
	error_line = p.error_line;
}

script_engine::~script_engine()
{
	blocks.clear();
}

/* script_machine */

script_machine::script_machine(script_engine* the_engine)
{
	assert(!the_engine->get_error());
	engine = the_engine;

	first_using_environment = NULL;
	last_using_environment = NULL;
	first_garbage_environment = NULL;
	last_garbage_environment = NULL;

	error = false;
	bTerminate = false;
}

script_machine::~script_machine()
{
	while (first_using_environment != NULL) {
		environment* object = first_using_environment;
		first_using_environment = first_using_environment->succ;
		delete object;
	}

	while (first_garbage_environment != NULL) {
		environment* object = first_garbage_environment;
		first_garbage_environment = first_garbage_environment->succ;
		delete object;
	}
}

script_machine::environment* script_machine::new_environment(environment* parent, script_engine::block* b)
{
	environment* result = NULL;
#ifdef PH3_VM_OPCODE_PROFILE
	if(vmOpcodeProfile.active){++vmOpcodeProfile.environments;if(first_garbage_environment!=NULL)++vmOpcodeProfile.reusedEnvironments;}
#endif

	if (first_garbage_environment != NULL) {
		//ごみ回収
		result = first_garbage_environment;
		first_garbage_environment = result->succ;
		*((result->succ != NULL) ? &result->succ->pred : &last_garbage_environment) = result->pred;
	}

	if (result == NULL) {
		result = new environment;
	}

	result->parent = parent;
	result->ref_count = 1;
	result->sub = b;
	result->ip = 0;
#if !defined(__EMSCRIPTEN__) || defined(PH3_LEGACY_SCRIPT_ENVIRONMENT)
	result->variables.release();
#endif
	result->stack.length = 0;
	result->has_result = false;

	//使用中リストへの追加
	result->pred = last_using_environment;
	result->succ = NULL;
	*((result->pred != NULL) ? &result->pred->succ : &first_using_environment) = result;
	last_using_environment = result;

	return result;
}

void script_machine::dispose_environment(environment* object)
{
	assert(object->ref_count == 0);
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_ENVIRONMENT)
	// No thread or child can still reach this environment (ref_count == 0).
	// Drop dead value references now, but bound and reuse its small storage.
	object->variables.reset_for_reuse(64);
	object->stack.reset_for_reuse(64);
#endif

	//使用中リストからの削除
	*((object->pred != NULL) ? &object->pred->succ : &first_using_environment) = object->succ;
	*((object->succ != NULL) ? &object->succ->pred : &last_using_environment) = object->pred;

	//ごみリストへの追加
	object->pred = last_garbage_environment;
	object->succ = NULL;
	*((object->pred != NULL) ? &object->pred->succ : &first_garbage_environment) = object;
	last_garbage_environment = object;
}
#ifdef PH3_SCRIPT_DIRECT_PARAMETERS
bool script_machine::transfer_arguments(environment* target,stack_t& source,unsigned count)
{
	if(count==0)return false;
	auto* block=target->sub;
	if(block->direct_parameter_base==-2){
		block->direct_parameter_base=-1;
		// The bytecode remains unchanged. Only a bounded, exact, contiguous
		// assignment prologue is eligible, including loop and task parameters.
		if(count<=64&&block->codes.length>=count){
			const auto& first=block->codes.at[0];
			if(first.command==script_engine::pc_assign&&first.variable<=64-count){
				bool match=true;
				for(unsigned i=0;i<count;++i){
					const auto& code=block->codes.at[i];
					if(code.command!=script_engine::pc_assign||code.level!=block->level||
					   code.variable!=first.variable+i){match=false;break;}
				}
				if(match){block->direct_parameter_base=first.variable;block->direct_parameter_count=count;}
			}
		}
	}
	if(block->direct_parameter_base<0||block->direct_parameter_count!=count||
	   target->variables.length!=0||target->stack.length!=0)return false;
	assert(source.length>=count);
	auto& variables=target->variables;
	const unsigned base=unsigned(block->direct_parameter_base),length=base+count;
	while(variables.capacity<length)variables.expand();
	variables.length=length;
	// Consume the caller in the same right-to-left order as the original
	// stack transfer. The pure prologue introduces no native calls or yields.
	for(unsigned i=count;i>0;--i){
		variables.at[base+i-1]=std::move(source.back());source.pop_back();
	}
	target->ip=count;
	error_line=block->codes.at[count-1].line;
#ifdef PH3_VM_OPCODE_PROFILE
	for(unsigned i=0;i<count;++i)countVmOpcode(script_engine::pc_assign,true);
#endif
	return true;
}
#endif

void script_machine::run()
{
	if (bTerminate)
		return;

	assert(!error);
	if (first_using_environment == NULL) {
		error_line = -1;
		threads.clear();
		threads.push_back(new_environment(NULL, engine->main_block));
		current_thread_index = 0;
		finished = false;
		stopped = false;
		resuming = false;

		while (!finished && !bTerminate) {
			advance();
		}
	}
}

void script_machine::resume()
{
	if (bTerminate)
		return;

	assert(!error);
	assert(stopped);
	stopped = false;
	finished = false;
	resuming = true;
	while (!finished && !bTerminate) {
		advance();
	}
}

void script_machine::call(std::string event_name)
{
	if (bTerminate)
		return;

	assert(!error);
	assert(!stopped);
	if (engine->events.find(event_name) != engine->events.end()) {
		run(); //念のため

		int threadIndex = current_thread_index;
		current_thread_index = 0;

		script_engine::block* event = engine->events[event_name];
		++(threads[0]->ref_count);
		threads[0] = new_environment(threads[0], event);

		finished = false;
		environment* epp = threads[0]->parent->parent;
		call_start_parent_environment_list.push_back(epp);
		while (!finished && !bTerminate) {
			advance();
		}
		call_start_parent_environment_list.pop_back();
		finished = false;
		current_thread_index = threadIndex;
	}
}

bool script_machine::has_event(std::string event_name)
{
	assert(!error);
	return engine->events.find(event_name) != engine->events.end();
}

int script_machine::get_current_line()
{
	environment* current = threads.at[current_thread_index];
	script_engine::code* c = &(current->sub->codes.at[current->ip]);
	return c->line;
}

void script_machine::advance()
{
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_VM_DISPATCH)
dispatch_next:
#endif
	assert(current_thread_index < threads.length);
	environment* current = threads.at[current_thread_index];

	if (current->ip >= current->sub->codes.length) {
		environment* removing = current;
		current = current->parent;

		bool bFinish = false;
		if (current == NULL)
			bFinish = true;
		else {
			if (call_start_parent_environment_list.size() > 1) {
				environment* env = *call_start_parent_environment_list.rbegin();
				if (current == env)
					bFinish = true;
			}
		}

		if (bFinish) {
			finished = true;
		} else {
			threads[current_thread_index] = current;

			if (removing->has_result) {
				assert(current != NULL && removing->variables.length > 0);
				current->stack.push_back(removing->variables.at[0]);
			} else if (removing->sub->kind == script_engine::bk_microthread) {
				threads.erase(threads.begin() + current_thread_index);
				yield();
			}

			assert(removing->stack.length == 0);

			for (;;) {
				--(removing->ref_count);
				if (removing->ref_count > 0)
					break;
				environment* next = removing->parent;
				dispose_environment(removing);
				removing = next;
			}
		}
	} else {
		script_engine::code* c = &(current->sub->codes.at[current->ip]);
#ifdef PH3_VM_OPCODE_PROFILE
		countVmOpcode(c->command);
#endif
		error_line = c->line; //ぉ
		++(current->ip);
		const auto skipLoopBack = [current,c]() {
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
			if(c->cached_jump >= 0){current->ip=c->cached_jump;return;}
#endif
			do ++(current->ip);
			while(current->sub->codes.at[current->ip-1].command != script_engine::pc_loop_back);
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
			c->cached_jump=current->ip;
#endif
		};
#if defined(PH3_SCRIPT_FAST_ARITHMETIC) && !defined(PH3_LEGACY_SCRIPT_LOAD_ARITHMETIC)
		const auto loadedArithmetic = [&](const value& loaded) __attribute__((always_inline)) {
			// Consume only an adjacent, pure binary numeric instruction. Bytecode,
			// branch addresses and nonnumeric fallback paths remain unchanged.
			if(stopped || current->ip>=current->sub->codes.length)return false;
			const auto* next=&current->sub->codes.at[current->ip];
#ifndef PH3_LEGACY_SCRIPT_LOAD_GUARDS
			// Reject an unrelated call before looking up its second argument.
			// Pure loads still run through the original path on every fallback.
			const bool loadsSecond = next->command==script_engine::pc_push_value ||
				next->command==script_engine::pc_push_variable;
			if(loadsSecond && current->ip+1>=current->sub->codes.length)return false;
			const auto* operation = loadsSecond ? next+1 : next;
			if(operation->command<script_engine::pc_real_add ||
			   operation->command>script_engine::pc_real_compare ||
			   operation->command==script_engine::pc_real_negative || operation->arguments!=2)return false;
#endif
			const value* left=nullptr;
			const value* right=&loaded;
			bool paired=false;
			if((next->command==script_engine::pc_push_value||next->command==script_engine::pc_push_variable)&&
			   current->ip+1<current->sub->codes.length){
				left=&loaded;right=nullptr;paired=true;
				if(next->command==script_engine::pc_push_value)right=&next->literal();
				else for(environment* e=current;e!=NULL;e=e->parent){
					if(e->sub->level!=next->level)continue;
					if(next->variable<e->variables.length&&e->variables.at[next->variable].has_data())
						right=&e->variables.at[next->variable];
					break;
				}
				if(right==nullptr)return false;
				next=&current->sub->codes.at[current->ip+1];
			}else{
				if(current->stack.length==0)return false;
				left=&current->stack.back();
			}
#ifdef PH3_LEGACY_SCRIPT_LOAD_GUARDS
			if(next->command<script_engine::pc_real_add || next->command>script_engine::pc_real_compare ||
			   next->command==script_engine::pc_real_negative || next->arguments!=2)return false;
#endif
			gstd::script_real a,b;
			if(!left->try_real_operand(a)||!right->try_real_operand(b))return false;
			if(next->command==script_engine::pc_real_compare&&left->get_type()!=right->get_type())return false;
			gstd::script_real result;
			switch(next->command){
			case script_engine::pc_real_add:result=a+b;break;
			case script_engine::pc_real_subtract:result=a-b;break;
			case script_engine::pc_real_multiply:result=a*b;break;
			case script_engine::pc_real_divide:result=a/b;break;
			case script_engine::pc_real_compare:result=a==b?0:(a<b?-1:1);break;
			default:return false;
			}
			const unsigned resultIp=current->ip+(paired?2:1);
			if(resultIp<current->sub->codes.length){
				const auto* sink=&current->sub->codes.at[resultIp];
				if(sink->command==script_engine::pc_assign){
					for(environment* e=current;e!=NULL;e=e->parent){
						if(e->sub->level!=sink->level)continue;
						auto& vars=e->variables;
						if(sink->variable<vars.length&&vars.at[sink->variable].has_data()&&
						   vars.at[sink->variable].get_type()!=engine->get_real_type())break;
						// Retain assignment type identity and source-line semantics.
						// A type change still reaches the original error path below.
						error_line=sink->line;current->ip=resultIp+1;
#ifdef PH3_VM_OPCODE_PROFILE
						if(paired)countVmOpcode(current->sub->codes.at[resultIp-2].command,true);
						countVmOpcode(next->command,true);countVmOpcode(sink->command,true);
#endif
						if(vars.length<=sink->variable){
							while(vars.capacity<=sink->variable)vars.expand();
							vars.length=sink->variable+1;
						}
						vars.at[sink->variable].set(engine->get_real_type(),result);
						if(!paired)current->stack.pop_back();
						resuming=false;
						return true;
					}
				}
			}
			error_line=next->line;
#ifdef PH3_VM_OPCODE_PROFILE
			if(paired)countVmOpcode(current->sub->codes.at[current->ip].command,true);
			countVmOpcode(next->command,true);
#endif
			current->ip+=paired?2:1;
			if(paired)current->stack.push_back(value(engine->get_real_type(),result));
			else current->stack.back().set(engine->get_real_type(),result);
			resuming=false;
			return true;
		};
#endif

		switch (c->command) {
		case script_engine::pc_assign: {
			stack_t* stack = &current->stack;
			assert(stack->length > 0);
			for (environment* i = current; i != NULL; i = i->parent) {
				if (i->sub->level == c->level) {
					variables_t* vars = &i->variables;
					if (vars->length <= c->variable) {
						while (vars->capacity <= c->variable)
							vars->expand();
						vars->length = c->variable + 1;
					}
					value* dest = &(vars->at[c->variable]);
					value* src = &stack->at[stack->length - 1];
					if (dest->has_data() && dest->get_type() != src->get_type()
						&& !(dest->get_type()->get_kind() == type_data::tk_array
						&& src->get_type()->get_kind() == type_data::tk_array
						&& (dest->length_as_array() > 0 || src->length_as_array() > 0))) {
						std::wstring error;
						error += L"A variable was changing it's value type.\r\n";
						error += L"(代入によって型が変えられようとしました)";
						raise_error(error);
					}
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
					*dest = std::move(*src);
#else
					*dest = *src;
#endif
					stack->pop_back();
					break;
				}
			}
		} break;

		case script_engine::pc_assign_writable: {
			stack_t* stack = &current->stack;
			assert(stack->length >= 2);
			value* dest = &stack->at[stack->length - 2];
			value* src = &stack->at[stack->length - 1];
			if (dest->has_data() && dest->get_type() != src->get_type()
				&& !(dest->get_type()->get_kind() == type_data::tk_array && src->get_type()->get_kind() == type_data::tk_array
				&& (dest->length_as_array() > 0 || src->length_as_array() > 0))) {
				std::wstring error;
				error += L"A variable was changing it's value type.\r\n";
				error += L"(代入によって型が変えられようとしました)";
				raise_error(error);
			} else {
				dest->overwrite(*src);
				stack->pop_back();
				stack->pop_back();
				// stack->length -= 2;
			}
		} break;

		case script_engine::pc_break_loop:
		case script_engine::pc_break_routine:
			for (environment* i = current; i != NULL; i = i->parent) {
				i->ip = i->sub->codes.length;

				if (c->command == script_engine::pc_break_loop) {
					if (i->sub->kind == script_engine::bk_loop) {
						environment* e = i->parent;
						assert(e != NULL);
						do
							++(e->ip);
						while (e->sub->codes.at[e->ip - 1].command != script_engine::pc_loop_back);
						break;
					}
				} else {
					if (i->sub->kind == script_engine::bk_sub || i->sub->kind == script_engine::bk_function
						|| i->sub->kind == script_engine::bk_microthread)
						break;
					else if (i->sub->kind == script_engine::bk_loop)
						i->parent->stack.clear(); /*小細工もいいところ*/
				}
			}
			break;

#ifdef PH3_SCRIPT_FAST_BUILTINS
		case script_engine::pc_index_read:
		case script_engine::pc_array_length:
		case script_engine::pc_boolean_not:
		case script_engine::pc_boolean_true:
		case script_engine::pc_boolean_false: {
			if(stopped)goto native_call_dispatch;
			auto& stack=current->stack;
			assert(stack.length>=c->arguments);
			value* argv=&stack.at[stack.length-c->arguments];
			if(c->command==script_engine::pc_index_read){
				gstd::script_real indexValue;
				if(!argv[0].has_data()||argv[0].get_type()->get_kind()!=type_data::tk_array||
				   !argv[1].try_real_operand(indexValue)||!(indexValue>=0)||
				   !(indexValue<argv[0].length_as_array())||indexValue>2147483647.0||
				   indexValue!=static_cast<unsigned>(indexValue))goto native_call_dispatch;
				// Save the child before releasing an owning parent array.
				argv[0]=static_cast<const value&>(argv[0]).index_as_array(unsigned(indexValue));
				stack.pop_back();
			}else if(c->command==script_engine::pc_array_length){
				if(!argv[0].has_data()||argv[0].get_type()->get_kind()!=type_data::tk_array)
					goto native_call_dispatch;
				const auto length=argv[0].length_as_array();
				argv[0].set(engine->get_real_type(),gstd::script_real(length));
			}else if(c->command==script_engine::pc_boolean_not){
				const bool result=!argv[0].as_boolean();argv[0].set(engine->get_boolean_type(),result);
			}else{
				stack.push_back(value(engine->get_boolean_type(),c->command==script_engine::pc_boolean_true));
			}
			resuming=false;break;
		}
#endif
		case script_engine::pc_call:
#ifdef PH3_SCRIPT_FAST_ARITHMETIC
		case script_engine::pc_real_add:
		case script_engine::pc_real_subtract:
		case script_engine::pc_real_multiply:
		case script_engine::pc_real_divide:
		case script_engine::pc_real_negative:
		case script_engine::pc_real_predecessor:
		case script_engine::pc_real_successor:
		case script_engine::pc_real_compare:
#endif
		case script_engine::pc_call_and_push_result:
#ifdef PH3_SCRIPT_FAST_BUILTINS
		native_call_dispatch:
#endif
		{
			stack_t* current_stack = &current->stack;
			const bool wantsResult=c->command!=script_engine::pc_call;
			assert(current_stack->length >= c->arguments);
			if (c->sub->func != NULL) {
				//ネイティブ呼び出し
				value* argv = &((*current_stack).at[current_stack->length - c->arguments]);
				value ret;
#ifdef PH3_SCRIPT_FAST_ARITHMETIC
				const bool arithmetic=c->command>=script_engine::pc_real_add &&
					c->command<=script_engine::pc_real_compare;
				gstd::script_real a=0,b=0;
				if(arithmetic && argv[0].try_real_operand(a) &&
					(c->arguments==1 || argv[1].try_real_operand(b)) &&
					// compare requires identical type objects, not just equal kinds.
					// Arithmetic addition intentionally has no such restriction.
					(c->command!=script_engine::pc_real_compare || argv[0].get_type()==argv[1].get_type())){
					gstd::script_real result=0;
					switch(c->command){
					case script_engine::pc_real_add:result=a+b;break;
					case script_engine::pc_real_subtract:result=a-b;break;
					case script_engine::pc_real_multiply:result=a*b;break;
					case script_engine::pc_real_divide:result=a/b;break;
					case script_engine::pc_real_negative:result=-a;break;
					case script_engine::pc_real_predecessor:result=a-1;break;
					case script_engine::pc_real_successor:result=a+1;break;
					// This intentionally preserves ph3's existing NaN comparison,
					// rather than replacing it with ordinary ordered comparisons.
					case script_engine::pc_real_compare:result=a==b?0:(a<b?-1:1);break;
					default:assert(false);
					}
					// Counter operations preserve the operand's exact type object.
					// Ordinary arithmetic returns the engine's real type instead.
					type_data* resultType=(c->command==script_engine::pc_real_predecessor||
						c->command==script_engine::pc_real_successor)?argv[0].get_type():engine->get_real_type();
#if !defined(PH3_LEGACY_SCRIPT_ARITHMETIC_RESULT)
					if(!stopped){
						// Operands were read before any release. Reuse the first
						// argument cell directly, preserving boxed alias/COW behavior.
						argv[0].set(resultType,result);
						if(c->arguments==2)current_stack->pop_back();
						resuming=false;
						break;
					}
#endif
					ret.set(resultType,result);
				}else
#endif
				{
#ifdef PH3_VM_OPCODE_PROFILE
					countVmNative(c->sub->name);
#endif
					ret = c->sub->func(this, c->arguments, argv);
				}
				if (stopped) {
					--(current->ip);
				} else {
					resuming = false;
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
					// Preserve arguments on a suspended call. On a completed call,
					// consume them and reuse the first argument slot for its result
					// instead of pop-all/copy-push on every math and engine API call.
					if(wantsResult && c->arguments > 0) {
						for(unsigned i=1;i<c->arguments;++i)current_stack->pop_back();
						current_stack->at[current_stack->length-1] = std::move(ret);
					} else {
						for(unsigned i=0;i<c->arguments;++i)current_stack->pop_back();
						if(wantsResult)
							current_stack->push_back(std::move(ret));
					}
#else
					//詰まれた引数を削除
					for (int i = 0; i < c->arguments; ++i)
						current_stack->pop_back();
					// current_stack->length -= c->arguments;
					//戻り値
					if (wantsResult)
						current_stack->push_back(ret);
#endif
				}
			} else if (c->sub->kind == script_engine::bk_microthread) {
				//マイクロスレッド起動
				++(current->ref_count);
				environment* e = new_environment(current, c->sub);
				++current_thread_index;
				threads.insert(threads.begin() + current_thread_index, e);
				//引数の積み替え
#ifdef PH3_SCRIPT_DIRECT_PARAMETERS
				if(!transfer_arguments(e,*current_stack,c->arguments))
#endif
				for (unsigned i = 0; i < c->arguments; ++i) {
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
					e->stack.push_back(std::move(current_stack->at[current_stack->length - 1]));
#else
					e->stack.push_back(current_stack->at[current_stack->length - 1]);
#endif
					current_stack->pop_back();
				}
			} else {
				//スクリプト間の呼び出し
				++(current->ref_count);
				environment* e = new_environment(current, c->sub);
				e->has_result = wantsResult;
				threads[current_thread_index] = e;
				//引数の積み替え
#ifdef PH3_SCRIPT_DIRECT_PARAMETERS
				if(!transfer_arguments(e,*current_stack,c->arguments))
#endif
				for (unsigned i = 0; i < c->arguments; ++i) {
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
					e->stack.push_back(std::move(current_stack->at[current_stack->length - 1]));
#else
					e->stack.push_back(current_stack->at[current_stack->length - 1]);
#endif
					current_stack->pop_back();
				}
			}
		} break;

		case script_engine::pc_case_begin:
		case script_engine::pc_case_end:
			break;

		case script_engine::pc_case_if:
		case script_engine::pc_case_if_not:
		case script_engine::pc_case_next: {
			bool exit = true;
			if (c->command != script_engine::pc_case_next) {
				stack_t* current_stack = &current->stack;
				exit = current_stack->at[current_stack->length - 1].as_boolean();
				if (c->command == script_engine::pc_case_if_not)
					exit = !exit;
				current_stack->pop_back();
			}
			if (exit) {
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
				if(c->cached_jump >= 0){current->ip=c->cached_jump;break;}
#endif
				int nested = 0;
				for (;;) {
					switch (current->sub->codes.at[current->ip].command) {
					case script_engine::pc_case_begin:
						++nested;
						break;
					case script_engine::pc_case_end:
						--nested;
						if (nested < 0)
							goto next;
						break;
					case script_engine::pc_case_next:
						if (nested == 0 && c->command != script_engine::pc_case_next) {
							++(current->ip);
							goto next;
						}
						break;
					}
					++(current->ip);
				}
			next:;
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_JUMPS)
				c->cached_jump=current->ip;
#endif
			}
		} break;

		case script_engine::pc_compare_e:
		case script_engine::pc_compare_g:
		case script_engine::pc_compare_ge:
		case script_engine::pc_compare_l:
		case script_engine::pc_compare_le:
		case script_engine::pc_compare_ne: {
			stack_t* stack = &current->stack;
			value& t = stack->at[stack->length - 1];
			gstd::script_real r = t.as_real();
			bool b;
			switch (c->command) {
			case script_engine::pc_compare_e:
				b = r == 0;
				break;
			case script_engine::pc_compare_g:
				b = r > 0;
				break;
			case script_engine::pc_compare_ge:
				b = r >= 0;
				break;
			case script_engine::pc_compare_l:
				b = r < 0;
				break;
			case script_engine::pc_compare_le:
				b = r <= 0;
				break;
			case script_engine::pc_compare_ne:
				b = r != 0;
				break;
			}
			t.set(engine->get_boolean_type(), b);
		} break;

		case script_engine::pc_dup: {
			stack_t* stack = &current->stack;
			assert(stack->length > 0);
			stack->push_back(stack->at[stack->length - 1]);
		} break;

		case script_engine::pc_dup2: {
			stack_t* stack = &current->stack;
			int len = stack->length;
			assert(len >= 2);
			stack->push_back(stack->at[len - 2]);
			stack->push_back(stack->at[len - 1]);
		} break;

		case script_engine::pc_loop_back:
			current->ip = c->ip;
			break;

		case script_engine::pc_loop_ascent: {
			stack_t* stack = &current->stack;
			value* i = &stack->at[stack->length - 1];
			if (i->as_real() <= 0) {
				skipLoopBack();
			}
			current->stack.pop_back();
		} break;

		case script_engine::pc_loop_descent: {
			stack_t* stack = &current->stack;
			value* i = &stack->at[stack->length - 1];
			if (i->as_real() >= 0) {
				skipLoopBack();
			}
			current->stack.pop_back();
		} break;

		case script_engine::pc_loop_count: {
			stack_t* stack = &current->stack;
			value* i = &stack->at[stack->length - 1];
			assert(i->get_type()->get_kind() == type_data::tk_real);
			gstd::script_real r = i->as_real();
			if (r > 0)
				i->set(engine->get_real_type(), r - 1);
			else {
				skipLoopBack();
			}
		} break;

		case script_engine::pc_loop_if: {
			stack_t* stack = &current->stack;
			bool c = stack->at[stack->length - 1].as_boolean();
			current->stack.pop_back();
			if (!c) {
				skipLoopBack();
			}
		} break;

		case script_engine::pc_pop:
			assert(current->stack.length > 0);
			current->stack.pop_back();
			break;

		case script_engine::pc_push_value:
#if defined(PH3_SCRIPT_FAST_ARITHMETIC) && !defined(PH3_LEGACY_SCRIPT_LOAD_ARITHMETIC)
			if(loadedArithmetic(c->literal()))break;
#endif
			current->stack.push_back(c->literal());
			break;

		case script_engine::pc_push_variable:
		case script_engine::pc_push_variable_writable:
			for (environment* i = current; i != NULL; i = i->parent) {
				if (i->sub->level == c->level) {
					variables_t* vars = &i->variables;
					if (vars->length <= c->variable || !((*vars).at[c->variable].has_data())) {
						std::wstring error;
						error += L"you are using a variable that has not been set yet.\r\n";
						error += L"(一回も代入していない変数を使おうとしました)";
						raise_error(error);
					} else {
						value* var = &(*vars).at[c->variable];
						if (c->command == script_engine::pc_push_variable_writable)
							var->unique();
#if defined(PH3_SCRIPT_FAST_ARITHMETIC) && !defined(PH3_LEGACY_SCRIPT_LOAD_ARITHMETIC)
						if(c->command==script_engine::pc_push_variable&&loadedArithmetic(*var))break;
#endif
						current->stack.push_back(*var);
					}
					break;
				}
			}
			break;

		case script_engine::pc_swap: {
			int len = current->stack.length;
			assert(len >= 2);
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
			current->stack[len - 1].swap(current->stack[len - 2]);
#else
			value t = current->stack[len - 1];
			current->stack[len - 1] = current->stack[len - 2];
			current->stack[len - 2] = t;
#endif
		} break;

		case script_engine::pc_yield:
			yield();
			break;

		default:
			assert(false);
		}
	}
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_VM_DISPATCH)
	// All three callers run exactly this condition around advance(). Fuse that
	// dispatch loop, not game ticks: yield/event/termination semantics and the
	// selected thread are re-read at the same instruction boundaries as before.
	if (!finished && !bTerminate) goto dispatch_next;
#endif
}
