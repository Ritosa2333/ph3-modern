#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../source/GcLib/gstd/Script.hpp"
#include "../../source/GcLib/gstd/Logger.hpp"
#ifdef PH3_VM_OPCODE_PROFILE
#include <emscripten.h>
extern "C" void hsob_vm_profile_reset();
extern "C" void hsob_vm_profile_dump();
#endif

// This standalone VM executable has no application logger. Sanitized builds
// retain GstdUtility's conditional logging branches, unlike the optimized
// linker, so provide the normal null owner without linking the window logger.
gstd::Logger* gstd::Logger::top_ = nullptr;

using namespace gstd;
static std::string Printable(const std::wstring& text) {
    std::string result;for(wchar_t c:text)result+=c>=32&&c<127?char(c):'?';return result;
}
struct Results { std::vector<double> values; int resumes=0; int errorLine=-1; };
static value Record(script_machine* machine,int argc,const value* args) {
    assert(argc==1);static_cast<Results*>(machine->data)->values.push_back(args[0].as_real());return value();
}
static value Pause(script_machine* machine,int,const value*) {
    if(!machine->get_resuming())machine->stop();return value();
}
static value Nested(script_machine* machine,int,const value*) {machine->call("Nested");return value();}
static value Terminate(script_machine* machine,int,const value*) {machine->terminate(L"test termination");return value();}
static value Throw(script_machine*,int,const value*) {throw std::runtime_error("test unwind");}
static value DifferentReal(script_machine*,int,const value*) {
    static type_data distinctReal(type_data::tk_real);
    return value(&distinctReal,script_real(1));
}
static value EchoPause(script_machine* machine,int argc,const value* args) {
    assert(argc==1);
    if(!machine->get_resuming()){machine->stop();return value();}
    return args[0];
}
static const function functions[]={{"Record",Record,1},{"Pause",Pause,0},{"NestedCall",Nested,0},
    {"TerminateTest",Terminate,0},{"ThrowTest",Throw,0},{"EchoPause",EchoPause,1},
    {"DifferentReal",DifferentReal,0}};

struct EscapedValues { std::vector<value> values; };
static value KeepValue(script_machine* machine,int argc,const value* args) {
    assert(argc==1);
    static_cast<EscapedValues*>(machine->data)->values.push_back(args[0]);
    return value();
}
static value RecallValue(script_machine* machine,int,const value*) {
    return static_cast<EscapedValues*>(machine->data)->values.at(0);
}
static void TestValuesAfterEngineRetirement() {
    // Runtime clients share a process-lifetime type manager. Common data owns
    // its values independently of the engine/literal pool that created them.
    script_type_manager types;
    const function callbacks[]={{"Keep",KeepValue,1},{"Recall",RecallValue,0}};
    for(int cycle=0;cycle<100;++cycle) {
        EscapedValues saved;
        {
            script_engine engine(&types,std::string(
                "Keep([[1,2],[3,4]]);Keep(\"kept\");Keep(3.5);"),2,callbacks);
            assert(!engine.get_error());
            script_machine machine(&engine);machine.data=&saved;machine.run();
            assert(!machine.get_error());
        }
        assert(saved.values.size()==3);
        assert(saved.values[0].index_as_array(0).index_as_array(1).as_real()==2);
        assert(saved.values[1].as_string()==L"kept");
        assert(saved.values[2].as_real()==3.5);
        {
            script_engine engine(&types,std::string(
                "let a=Recall();let row=a[0];row[1]=9;a[0]=row;Keep(a);"),2,callbacks);
            assert(!engine.get_error());
            script_machine machine(&engine);machine.data=&saved;machine.run();
            assert(!machine.get_error());
        }
        assert(saved.values.size()==4);
        assert(saved.values[0].index_as_array(0).index_as_array(1).as_real()==2);
        assert(saved.values[3].index_as_array(0).index_as_array(1).as_real()==9);
        assert(saved.values[1].as_string()==L"kept");
        assert(saved.values[2].get_type()==types.get_real_type());
    }
    std::puts("engine retirement: escaped scalars, strings, nested arrays and cross-engine COW PASS");
}

static void TestStringViews() {
    type_data character(type_data::tk_char),stringType(type_data::tk_array,&character);
    value text(&stringType,std::wstring(L"a\u9b54z"));
    assert(text.as_string()==L"a\u9b54z");
    value saved=text;text.unique();
    const value& view=text;
    const value& letter=view.index_as_array(1);letter.unique();
    value handle=letter;
    assert(text.as_string()==L"a\u9b54z");
    handle.overwrite(value(&character,L'X'));
    assert(text.as_string()==L"aXz"&&saved.as_string()==L"a\u9b54z");

    value escaped;
    {
        value owner(&stringType,std::wstring(L"q"));
        owner.index_as_array(0).unique();escaped=owner.index_as_array(0);
    }
    value borrowed;borrowed.append(&stringType,escaped);
    assert(borrowed.as_string()==L"q");
    escaped.overwrite(value(&character,L'R'));
    assert(borrowed.as_string()==L"R");

    value empty(&stringType,std::wstring());assert(empty.as_string().empty());
    value emptySaved=empty;empty.append(&stringType,value(&character,L'A'));
    assert(empty.as_string()==L"A"&&emptySaved.as_string().empty());
    value tail(&stringType,std::wstring(L"BC"));assert(tail.as_string()==L"BC");
    empty.concatenate(tail);assert(empty.as_string()==L"ABC"&&tail.as_string()==L"BC");
    value whole(&stringType,std::wstring(L"old"));assert(whole.as_string()==L"old");
    value wholeHandle=whole;wholeHandle.overwrite(tail);
    assert(whole.as_string()==L"BC"&&wholeHandle.as_string()==L"BC");
    std::wstring longText(1024,L'W');value longValue(&stringType,longText);
    assert(longValue.as_string()==longText&&longValue.as_string()==longText);
    std::puts("short/long strings, COW, escaped const/writable handles, boxed children, append and overwrite PASS");
}

static Results Run(const char* name,const char* source,const char* event=nullptr,bool expectError=false,bool expectThrow=false) {
    script_type_manager types;
    script_engine engine(&types,std::string(source),sizeof(functions)/sizeof(functions[0]),functions);
    if(engine.get_error()) {
        std::fprintf(stderr,"parse failure %s at %d: %s\n",name,engine.get_error_line(),Printable(engine.get_error_message()).c_str());
        std::abort();
    }
    Results results; bool threw=false;
    {
        script_machine machine(&engine);machine.data=&results;
        try {
            machine.run();if(event)machine.call(event);
            while(machine.get_stopped()&&!machine.get_error()) {
                assert(results.resumes++<1000);machine.resume();
            }
        }catch(const std::runtime_error&){threw=true;}
        if(machine.get_error()!=expectError) {
            std::fprintf(stderr,"VM failure %s at %d: %s\n",name,machine.get_error_line(),Printable(machine.get_error_message()).c_str());
            std::abort();
        }
        if(expectError)results.errorLine=machine.get_error_line();
    }
    assert(threw==expectThrow);
    std::printf("%s %d",name,results.resumes);
    for(double value:results.values)std::printf(" %.17g",value);
    if(expectError)std::printf(" error-line=%d",results.errorLine);
    std::puts("");return results;
}
int main() {
#ifdef PH3_VM_OPCODE_PROFILE
    EM_ASM({globalThis.ph3Diagnostics={};});
    hsob_vm_profile_reset();
#endif
    TestValuesAfterEngineRetirement();
    TestStringViews();
    {
        type_data real(type_data::tk_real), anotherReal(type_data::tk_real);
        type_data character(type_data::tk_char), boolean(type_data::tk_boolean);
        type_data array(type_data::tk_array,&real), text(type_data::tk_array,&character);
        const std::uint64_t patterns[]={0,0x8000000000000000ULL,0x3ff0000000000000ULL,
            0x0000000000000001ULL,0x7ff0000000000000ULL,0xfff0000000000000ULL,
            0x7ff8000000000042ULL,0xbfd5555555555555ULL};
        for(const auto bits:patterns){
            double number;std::memcpy(&number,&bits,sizeof(number));
            value a(&real,number), b(a), c; c=b;
            for(value* item:{&a,&b,&c}){
                double actual=item->as_real();std::uint64_t actualBits;
                std::memcpy(&actualBits,&actual,sizeof(actual));
                assert(actualBits==bits&&item->get_type()==&real);
                item->unique();
                actual=item->as_real();std::memcpy(&actualBits,&actual,sizeof(actual));
                assert(actualBits==bits);
            }
        }
        value zero(&real,-0.0), other(&anotherReal,2.0);
        zero.swap(other);assert(zero.get_type()==&anotherReal&&zero.as_real()==2);
        assert(other.get_type()==&real&&std::signbit(other.as_real()));
        other=other;assert(std::signbit(other.as_real()));
        value truth(&boolean,true), letter(&character,L'\u9b54');
        assert(truth.as_real()==1&&truth.as_char()==L'1'&&truth.as_string()==L"true");
        assert(letter.as_real()==0x9b54&&letter.as_string()==L"\u9b54");
        value string(&text,std::wstring(L"a\u9b54z"));value stringCopy=string;
        string.unique();string.index_as_array(1).unique();
        {
            value writable=string.index_as_array(1);
            writable.overwrite(value(&character,L'X'));
        }
        assert(string.as_string()==L"aXz"&&stringCopy.as_string()==L"a\u9b54z");
        value parent;parent.append(&array,value(&real,42.0));
        value parentCopy=parent;parent.unique();parent.index_as_array(0).unique();
        {
            value writable=parent.index_as_array(0);
            value source(&real,17.0);writable.overwrite(source);
            assert(parent.index_as_array(0).as_real()==17);
        }
        assert(parentCopy.index_as_array(0).as_real()==42);
        value detached=parent.index_as_array(0);detached.set(&real,99.0);
        assert(detached.as_real()==99&&parent.index_as_array(0).as_real()==17);
        value assigned=truth;assigned.set(&boolean,false);
        assert(!assigned.as_boolean()&&truth.as_boolean());
        value nothing;assert(!nothing.has_data()&&nothing.as_real()==0&&nothing.as_string()==L"(VOID)");
        std::puts("scalar bits, type identity, writable handles and scalar/array ownership PASS");
    }
    {
        type_data real(type_data::tk_real), array(type_data::tk_array,&real);
        value parent;parent.append(&array,value(&real,42.0));
        parent=parent.index_as_array(0);
        assert(parent.as_real()==42);
        value movedParent;movedParent.append(&array,value(&real,43.0));
        movedParent=std::move(movedParent.index_as_array(0));
        assert(movedParent.as_real()==43);
        movedParent=std::move(movedParent);assert(movedParent.as_real()==43);
#if defined(__EMSCRIPTEN__) && !defined(PH3_LEGACY_SCRIPT_MOVES)
        value moved(std::move(movedParent));
        assert(!movedParent.has_data()&&moved.as_real()==43);
#endif
    }
    const auto arithmetic=Run("arithmetic",R"(
        function Square(x) { return(x*x); }
        let total=0; ascent(i in 0 .. 100) { total+=Square(i); }
        Record(total);
    )");
    assert(arithmetic.values==std::vector<double>{328350});
    const auto numeric=Run("numeric-opcode-fallbacks",R"(
        let number=(2+3)*7-9/3;Record(number);Record(-number);
        let a=[1,2]+[3,4];Record(a[0]);Record(a[1]);
        let b=[[1,2],[3,4]]+[[5,6],[7,8]];Record(b[1][0]);
        let c=[2,5]-[1,3];Record(c[0]);Record(c[1]);
        Record('A'+1);Record(1<2);Record(2>=2);
        let big=9007199254740992;Record((big+1)-big);
        Record(-0);Record(1/0);Record(-1/0);Record(0/0);
        Record((0/0)==(0/0));Record((0/0)>0);
    )");
    assert(numeric.values.size()==17);
    assert((std::vector<double>(numeric.values.begin(),numeric.values.begin()+11)==
        std::vector<double>{32,-32,4,6,10,1,2,66,1,1,0}));
    assert(numeric.values[11]==0&&std::signbit(numeric.values[11]));
    assert(std::isinf(numeric.values[12])&&!std::signbit(numeric.values[12]));
    assert(std::isinf(numeric.values[13])&&std::signbit(numeric.values[13]));
    assert(std::isnan(numeric.values[14]));
    assert(numeric.values[15]==0&&numeric.values[16]==1);
    const auto identities=Run("arithmetic-type-identity",R"(
        Record(DifferentReal()+1);Record(DifferentReal()==DifferentReal());
        Record([1,2]<[1,3]);
    )");
    assert((identities.values==std::vector<double>{2,1,1}));
    Run("mismatched-real-comparison","let wrong=DifferentReal()==1;",nullptr,true);
    Run("fused-assignment-type-error","let a=true;\na=1+2;\nRecord(a);",nullptr,true);
    Run("fused-assignment-identity-error","let a=DifferentReal();\na=a+1;\nRecord(a);",nullptr,true);
    const auto fused=Run("fused-parent-assignment",R"(
        let outer=2;
        function Update(x){outer=outer+x;let computed=outer*2-1;return(computed);}
        Record(Update(3));Record(outer);Record(Update(-2));Record(outer);
        let a=[7,8];let saved=a;
        let n=a[0]+a[0];a[0]=n*2-1;
        Record(n);Record(a[0]);Record(saved[0]);
    )");
    assert((fused.values==std::vector<double>{9,5,5,3,14,27,7}));
    const auto counters=Run("counter-types",R"(
        Record(successor(1));Record(predecessor(1));
        Record(successor('A'));Record(predecessor(true));
        Record(successor(9007199254740992)-9007199254740992);
        let a=DifferentReal();a=successor(a);Record(a);a=predecessor(a);Record(a);
        Record(successor(DifferentReal())==DifferentReal());
    )");
    assert((counters.values==std::vector<double>{2,0,66,0,0,2,1,0}));
    Run("counter-array-error","let a=successor([1,2]);",nullptr,true);
    const auto builtins=Run("builtin-opcode-fallbacks",R"(
        Record([11,22][true]);Record([11,22][false]);Record("AB"[false]);
        Record([[31,32]][0][1]);Record(length([]));Record(length([1,2,3]));
        Record(![]);Record(![1]);Record(!0);Record(!1);Record(true);Record(false);
        let a=[[5,6]];let saved=a;let child=a[0];child[1]=17;
        Record(child[1]);Record(saved[0][1]);
    )");
    assert((builtins.values==std::vector<double>{22,11,65,32,0,3,1,0,1,0,1,0,17,6}));
    Run("builtin-index-fraction","Record([1,2][0.5]);",nullptr,true);
    Run("builtin-index-negative","Record([1,2][-1]);",nullptr,true);
    Run("builtin-index-range","Record([1,2][2]);",nullptr,true);
    Run("builtin-index-type","Record(5[0]);",nullptr,true);
    Run("builtin-length-type","Record(length(5));",nullptr,true);
    {
        // Force repeated deque growth and bytecode reallocation. Literal
        // addresses must remain valid, including literals in a later block.
        std::string source="let total=0;";double expected=0;
        for(int i=0;i<3000;++i){source+="total+="+std::to_string(i)+";";expected+=i;}
        source+="Record(total);function Fresh(){return([5,6]);}"
            "let a=Fresh();a[0]=99;let b=Fresh();Record(b[0]);";
        const auto literals=Run("literal-pool-growth",source.c_str());
        assert((literals.values==std::vector<double>{expected,5}));
    }
    const auto arrays=Run("nested-cow",R"(
        let a=[[1,2],[3,4]]; let b=a;
        let row=a[0];row[1]=9;a[0]=row;
        Record(a[0][1]);Record(b[0][1]);
        function Make(x) {let localArray=[x,x+1];return(localArray);}
        let result=0;ascent(i in 0 .. 5000){let pair=Make(i);result+=pair[1];}
        Record(result);
    )");
    assert((arrays.values==std::vector<double>{9,2,12502500}));
    const auto writable=Run("repeated-writable-scalars",R"(
        let a=[1,2,3];let saved=a;
        ascent(i in 0 .. 2000){a[0]=a[0]+1;a[1]=a[0]*2;a[2]=a[1]-a[0];}
        Record(a[0]);Record(a[1]);Record(a[2]);Record(saved[0]);
        let nested=[[5,6],[7,8]];let old=nested;
        let row=nested[0];row[1]=nested[1][0]+10;nested[0]=row;
        Record(nested[0][1]);Record(old[0][1]);
    )");
    assert((writable.values==std::vector<double>{2001,4002,2001,1,17,6}));
    const auto pauses=Run("pause-resume",R"(
        ascent(i in 0 .. 12){Record(i);Pause();}
        Record(100);
    )");
    assert(pauses.resumes==12&&pauses.values.size()==13&&pauses.values.back()==100);
    const auto argumentPause=Run("pause-argument-result",R"(
        ascent(i in 0 .. 4){Record(EchoPause(i+10));}
    )");
    assert(argumentPause.resumes==4&&(argumentPause.values==std::vector<double>{10,11,12,13}));
    Run("nested-branch-targets",R"(
        function Branch(x){
            if(x<0){if(x<-2){return(1);}else{return(2);}}
            else if(x<3){return(3);}else{return(4);}
        }
        let total=0;
        loop(20){ascent(i in -5 .. 8){total+=Branch(i);}}
        let k=0;while(k<6){k++;if(k==3){loop(2){total+=10;}}}
        loop(0){total=-1;}
        Record(total);Record(k);
    )");
    const auto operands=Run("tagged-bytecode-operands",R"(
        function Pick(x){
            alternative(x)
            case(0,2){return(10+x);}
            case(1){return(20);}
            others{if(x<4){return(x+30);}else{return(x+40);}}
        }
        let total=0;
        loop(30){
            descent(i in 0 .. 6){total+=Pick(i);}
            let j=0;while(j<2){total+=j;j++;}
        }
        Record(total);Record(Pick(2));
    )");
    assert((operands.values==std::vector<double>{4950,12}));
    Run("microthread-arguments",R"(
        task Worker(base){ascent(i in 0 .. 3){Record(base+i);yield;}}
        Worker(10);Worker(20);loop(8){yield;}
    )");
    const auto params=Run("mixed-parameter-prologue",R"(
        function Mix(number,letters,flag,items){
            Record(number);Record(letters[1]);Record(flag);
            let copied=items;items[0]=number;Pause();
            Record(items[0]);Record(copied[0]);return(number+length(letters));
        }
        let source=[1,2];Record(Mix(7,"ab",true,source));Record(source[0]);
        function Missing(value){Record(value);}
        Record(Missing(42));
    )");
    assert(params.resumes==1&&(params.values==std::vector<double>{7,98,1,7,1,9,1,42,0}));
    const auto recursion=Run("recursive-parameter-prologue",R"(
        function Fibonacci(n){if(n<2){return(n);}return(Fibonacci(n-1)+Fibonacci(n-2));}
        Record(Fibonacci(12));
        task Worker(a,b,c){Record(a);yield;Record(b[0]);yield;Record(c[0]);}
        Worker(10,[20],"A");loop(8){yield;}
    )");
    assert((recursion.values==std::vector<double>{144,10,20,65}));
    {
        std::string source="function Wide(";
        for(int i=0;i<67;++i){if(i)source+=",";source+="p"+std::to_string(i);}
        source+="){return(p65+p66);}Record(Wide(";
        for(int i=0;i<67;++i){if(i)source+=",";source+=std::to_string(i);}
        source+="));";
        const auto wide=Run("wide-parameter-fallback",source.c_str());
        assert((wide.values==std::vector<double>{131}));
    }
    const auto nested=Run("nested-event",R"(
        @Nested {Record(42);}
        @Start {Record(1);NestedCall();Record(2);}
    )","Start");
    assert((nested.values==std::vector<double>{1,42,2}));
    Run("loop-break-return",R"(
        function Find(){ascent(i in 0 .. 30){if(i==7){return(i);}}return(-1);}
        loop(16){Record(Find());}
    )");
    const auto terminated=Run("termination","Record(1);TerminateTest();Record(2);",nullptr,true);
    assert(terminated.values==std::vector<double>{1});
    const auto unwind=Run("unwind",R"(
        function Work(){let held=[[1,2],[3,4]];ThrowTest();Record(9);}
        Record(1);Work();Record(2);
    )",nullptr,false,true);
    assert(unwind.values==std::vector<double>{1});
    std::puts("PASS VM arithmetic, COW, recycled environments, stop/resume, reentrant events, return, termination and unwinding");
#ifdef PH3_VM_OPCODE_PROFILE
    hsob_vm_profile_dump();
    assert(EM_ASM_INT({const p=globalThis.ph3Diagnostics.vmProfile;
        return p.scalarBoxes>0&&p.bodyRequests>0&&p.environments>0&&p.reusedEnvironments>0&&
            p.dispatch['push-variable']>0&&p.native.Record>0&&p.nativeOverflow===0?1:0;})==1);
#endif
}
