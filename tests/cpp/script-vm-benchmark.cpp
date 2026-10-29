#include "../../source/GcLib/gstd/Script.hpp"
#include "../../source/GcLib/gstd/Logger.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>

gstd::Logger* gstd::Logger::top_=nullptr;
using namespace gstd;
struct State { double checksum=0; };
static value Record(script_machine* machine,int,const value* args){
    static_cast<State*>(machine->data)->checksum=args[0].as_real();return value();
}
static value KeyLength(script_machine* machine,int,const value* args){
    return value(machine->get_engine()->get_real_type(),script_real(args[0].as_string().size()));
}
int main(){
    const function functions[]={{"Record",Record,1},{"KeyLength",KeyLength,1}};
    const char* sources[]={
        "let x=0;ascent(i in 0 .. 100000){x=x+i*2-1;}Record(x);",
        "function Step(x){return(x*2-1);}let x=0;ascent(i in 0 .. 100000){x+=Step(i);}Record(x);",
        "let a=[0,1];ascent(i in 0 .. 50000){a[0]=a[0]+a[1];a[1]=a[1]+1;}Record(a[0]);",
        "let keyText=\"game/script/stage/bullet.dnh\";let sumTotal=0;ascent(i in 0 .. 100000){sumTotal+=KeyLength(keyText);}Record(sumTotal);"
    };
    const double expected[]={9999800000.0,9999800000.0,1250025000.0,2800000.0};
    script_type_manager types;
    for(int kernel=0;kernel<4;++kernel){
        script_engine engine(&types,std::string(sources[kernel]),2,functions);
        assert(!engine.get_error());
        for(int sample=0;sample<7;++sample){
            State state;
            const auto begin=std::chrono::steady_clock::now();
            {
                script_machine machine(&engine);machine.data=&state;machine.run();
                assert(!machine.get_error());
            }
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            assert(state.checksum==expected[kernel]);
            std::printf("%d %d %.6f %.0f\n",kernel,sample,ms,state.checksum);
        }
    }
}
