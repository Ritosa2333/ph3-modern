#pragma once
// Diagnostic-only, read-only fixed-tick observation. Never enable this while
// reporting production performance. Identical source is compiled by the MSVC
// source oracle and Emscripten verifier. Pointer/padding/graphics state is absent.
#ifdef WOO_REPLAY_VERIFY
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>

namespace woo { namespace verify {
struct Hash {
    std::uint64_t value = UINT64_C(14695981039346656037);
    void word(std::uint64_t v) {
        for (unsigned i=0;i<8;++i) { value^=(v>>(i*8))&255; value*=UINT64_C(1099511628211); }
    }
    void real(double v) { std::uint64_t bits;std::memcpy(&bits,&v,8);word(bits); }
    std::string text() const { char out[17];std::snprintf(out,sizeof(out),"%016llx",(unsigned long long)value);return out; }
};
inline std::string realBits(double value) {
    std::uint64_t bits;std::memcpy(&bits,&value,8);char text[17];
    std::snprintf(text,sizeof(text),"%016llx",(unsigned long long)bits);return text;
}

struct Access {
    static std::string commonInteger(StgStageController* stage,const char* area,const char* key) {
        auto manager=stage->GetSystemController()->GetCommonDataManager();
        if(manager==NULL||!manager->IsExists(area))return "null";
        auto data=manager->GetData(area);
        if(data==NULL||!data->IsExists(key))return "null";
        return std::to_string((long long)data->GetValue(key).as_real());
    }
    static std::string spellIndex(StgStageController* stage) {
        auto manager=stage->GetSystemController()->GetCommonDataManager();
        if(manager==NULL||!manager->IsExists("Spell"))return "null";
        auto data=manager->GetData("Spell");
        if(data==NULL||!data->IsExists("SPELL_INDEX"))return "null";
        auto wide=data->GetValue("SPELL_INDEX").as_string();
        std::ostringstream out;out<<'[';
        for(std::size_t i=0;i<wide.size();++i){if(i)out<<',';out<<unsigned(wide[i]);}
        out<<']';return out.str();
    }
    static int cursor(directx::KeyReplayManager* keys) { return keys->frame_; }
    static std::string inputs(directx::KeyReplayManager* keys) {
        std::ostringstream s;s<<'[';bool comma=false;
        for(int key:keys->listTarget_) {
            auto state=keys->mapLastKeyState_.find(key);
            if(comma)s<<',';comma=true;
            s<<'['<<key<<','<<(state==keys->mapLastKeyState_.end()?0:state->second)<<']';
        }
        s<<']';return s.str();
    }
    static std::string rng(gstd::MersenneTwister* random) {
        // A copy's next 624 words canonicalize implementations with different
        // internal index/stream serialization layouts without advancing the game.
        Hash hash;
#ifdef WOO_TRACE_MKM_RNG
        auto copy=*random;
        for(unsigned i=0;i<624;++i)hash.word(copy._GenrandInt32());
#else
        auto copy=random->actualTwister;
        for(unsigned i=0;i<624;++i)hash.word(copy());
#endif
        return hash.text();
    }
    template<class Object> static void common(Hash& h,Object* object) {
        h.word(object->GetObjectID());h.word(object->GetObjectType());
        h.word(object->IsDeleted());h.word(object->IsActive());
        auto* movement=static_cast<StgMoveObject*>(object);
        h.real(movement->GetPositionX());h.real(movement->GetPositionY());
        h.real(movement->GetSpeed());h.real(movement->GetDirectionAngle());
        auto pattern=movement->GetPattern();
        h.word(pattern==NULL?-1:pattern->GetType());
    }
    static std::string enemies(StgEnemyManager* manager) {
        Hash h;auto& list=manager->GetEnemyList();h.word(list.size());
        for(auto& owned:list) { auto* object=owned.GetPointer();common(h,object);
            h.real(object->GetLife());h.real(object->GetShotDamageRate());h.real(object->GetSpellDamageRate()); }
        return h.text();
    }
    static std::string enemyDetails(StgEnemyManager* manager) {
        std::ostringstream out;out<<'[';bool comma=false;
        for(auto& owned:manager->GetEnemyList()) {
            if(comma)out<<',';comma=true;auto* object=owned.GetPointer();
            auto* move=static_cast<StgMoveObject*>(object);auto pattern=move->GetPattern();
            out<<'['<<object->GetObjectID()<<','<<object->GetObjectType()<<','<<object->IsDeleted()<<','<<object->IsActive()
               <<",\""<<realBits(move->GetPositionX())<<"\",\""<<realBits(move->GetPositionY())
               <<"\",\""<<realBits(move->GetSpeed())<<"\",\""<<realBits(move->GetDirectionAngle())
               <<"\","<<(pattern==NULL?-1:pattern->GetType())<<",\""<<realBits(object->GetLife())
               <<"\",\""<<realBits(object->GetShotDamageRate())<<"\",\""
               <<realBits(object->GetSpellDamageRate())<<"\"]";
        }
        out<<']';return out.str();
    }
    static std::string shots(StgShotManager* manager) {
        Hash h;h.word(manager->listObj_.size());
        for(auto& owned:manager->listObj_) { auto* object=owned.GetPointer();common(h,object);
            h.word(object->GetShotDataID());h.word(object->GetOwnerType());h.word(object->GetDelay());
            h.real(object->GetDamage());h.real(object->GetLife());h.word(object->IsValidGraze());
            h.word(object->IsAutoDelete());h.word(object->IsEraseShot());h.word(object->IsSpellFactor()); }
        return h.text();
    }
    static std::string items(StgItemManager* manager) {
        Hash h;h.word(manager->listObj_.size());
        for(auto& owned:manager->listObj_) { auto* object=owned.GetPointer();common(h,object);
            h.word(object->GetItemType());h.word(object->GetScore());h.word(object->GetMoveType());
            h.word(object->IsMoveToPlayer());h.word(object->IsPermitMoveToPlayer()); }
        return h.text();
    }
    static std::string shotDetails(StgShotManager* manager) {
        std::ostringstream out;out<<'[';bool comma=false;
        for(auto& owned:manager->listObj_) {
            if(comma)out<<',';comma=true;auto* object=owned.GetPointer();
            auto* move=static_cast<StgMoveObject*>(object);auto pattern=move->GetPattern();
            out<<'['<<object->GetObjectID()<<','<<object->GetObjectType()<<','<<object->IsDeleted()<<','<<object->IsActive()
               <<",\""<<realBits(move->GetPositionX())<<"\",\""<<realBits(move->GetPositionY())
               <<"\",\""<<realBits(move->GetSpeed())<<"\",\""<<realBits(move->GetDirectionAngle())
               <<"\","<<(pattern==NULL?-1:pattern->GetType())<<','<<object->GetShotDataID()<<','<<object->GetOwnerType()
               <<','<<object->GetDelay()<<",\""<<realBits(object->GetDamage())<<"\",\""<<realBits(object->GetLife())
               <<"\","<<object->IsValidGraze()<<','<<object->IsAutoDelete()<<','<<object->IsEraseShot()<<','<<object->IsSpellFactor()<<']';
        }
        out<<']';return out.str();
    }
};

struct Context {
    bool configured=false,enabled=false,complete=false;
    int expectedStages=0,closedStages=0,ticks=0,detailTick=-1;
    StgStageController* stage=nullptr;
    std::FILE* file=nullptr;
    std::string buffer;
    void configure() {
        if(configured)return;configured=true;
#ifdef __EMSCRIPTEN__
        enabled=EM_ASM_INT({return Module['wooTraceEnabled']?1:0;})!=0;
        expectedStages=EM_ASM_INT({return Module['wooTraceStageCount']||0;});
        detailTick=EM_ASM_INT({return Module['wooTraceDetailTick']??-1;});
#else
        const char* path=std::getenv("WOO_TRACE_OUTPUT");
        if(path&&*path) {file=std::fopen(path,"wb");if(!file)throw std::runtime_error("Cannot open oracle trace output");enabled=true;}
        const char* count=std::getenv("WOO_TRACE_STAGE_COUNT");expectedStages=count?std::atoi(count):0;
        const char* detail=std::getenv("WOO_TRACE_DETAIL_TICK");detailTick=detail?std::atoi(detail):-1;
#endif
        if(enabled&&expectedStages<=0)throw std::runtime_error("A verifier run requires an explicit stage count");
    }
    void flush() {
        if(buffer.empty())return;
#ifdef __EMSCRIPTEN__
        const int ok=EM_ASM_INT({
            const value=UTF8ToString($0);const size=Module['wooTraceBytes']||0;
            if(size+value.length>16777216)return 0;
            (Module['wooTraceChunks']||(Module['wooTraceChunks']=[])).push(value);
            Module['wooTraceBytes']=size+value.length;return 1;
        },buffer.c_str());
        if(!ok)throw std::runtime_error("Verifier transport overflow: collector must drain trace chunks");
#else
        if(std::fwrite(buffer.data(),1,buffer.size(),file)!=buffer.size())throw std::runtime_error("Oracle trace write failed");
        std::fflush(file);
#endif
        buffer.clear();
    }
    void emit(const std::string& line,bool force=false) {
        buffer+=line;buffer+='\n';if(force||buffer.size()>=65536)flush();
    }
};
inline Context context;

inline void Begin(StgStageController* stage) {
    context.configure();if(!context.enabled||context.complete||!stage->GetStageInformation()->IsReplay())return;
    if(context.stage)throw std::runtime_error("Verifier stage began before previous stage closed");
    context.stage=stage;context.ticks=0;
    auto info=stage->GetStageInformation();auto recorded=info->GetReplayData();
    std::ostringstream out;
    out<<"{\"kind\":\"begin\",\"stage\":"<<info->GetStageIndex()
       <<",\"expectedFrame\":"<<recorded->GetEndFrame()<<",\"expectedScore\":\""<<recorded->GetLastScore()<<"\"}";
    context.emit(out.str(),true);
}

inline void Tick(StgStageController* stage,int inputFrame) {
    if(!context.enabled||context.stage!=stage)return;
    auto info=stage->GetStageInformation();auto player=stage->GetPlayerObject();
    std::ostringstream out;
    out<<"{\"kind\":\"tick\",\"stage\":"<<info->GetStageIndex()<<",\"tick\":"<<context.ticks++
       <<",\"frame\":"<<info->GetCurrentFrame()<<",\"inputFrame\":"<<inputFrame
       <<",\"input\":"<<Access::inputs(stage->GetKeyReplayManager().GetPointer())
       <<",\"score\":\""<<info->GetScore()<<"\",\"graze\":\""<<info->GetGraze()<<"\",\"point\":\""<<info->GetPoint()<<"\""
       <<",\"player\":";
    if(player==NULL)out<<"null";
    else out<<"[\""<<realBits(player->GetX())<<"\",\""<<realBits(player->GetY())<<"\","
            <<player->GetState()<<",\""<<realBits(player->GetLife())<<"\",\""<<realBits(player->GetSpell())
            <<"\",\""<<realBits(player->GetPower())<<"\","<<player->GetInvincibilityFrame()<<','<<player->GetRebirthFrame()<<']';
    out<<",\"counts\":["<<stage->GetEnemyManager()->GetEnemyCount()<<','<<stage->GetShotManager()->GetShotCountAll()
       <<','<<stage->GetItemManager()->GetItemCount()<<']'
       <<",\"maxScore\":"<<Access::commonInteger(stage,"System","MAX_SCORE")
       <<",\"spellBase\":"<<Access::commonInteger(stage,"Spell","SPELL_BONUS")
       <<",\"spellIndexUnits\":"<<Access::spellIndex(stage)
       <<",\"chapterAll\":"<<Access::commonInteger(stage,"Stg","STAGE_CHAPTER_ALL")
       <<",\"chapterSafe\":"<<Access::commonInteger(stage,"Stg","STAGE_CHAPTER_SAFE")
       <<",\"chapterDeath\":"<<Access::commonInteger(stage,"Stg","ChapterDeathCount")
       <<",\"rng\":\""<<Access::rng(info->GetMersenneTwister().GetPointer())
       <<"\",\"enemies\":\""<<Access::enemies(stage->GetEnemyManager())
       <<"\",\"shots\":\""<<Access::shots(stage->GetShotManager())
       <<"\",\"items\":\""<<Access::items(stage->GetItemManager())<<"\"";
    if(context.ticks-1==context.detailTick) {
        out<<",\"detailShots\":"<<Access::shotDetails(stage->GetShotManager());
        out<<",\"detailEnemies\":"<<Access::enemyDetails(stage->GetEnemyManager());
    }
    out<<'}';
    context.emit(out.str());
}

inline void End(StgStageController* stage) {
    if(!context.enabled||context.stage!=stage)return;
    auto info=stage->GetStageInformation();auto recorded=info->GetReplayData();
    std::ostringstream out;
    out<<"{\"kind\":\"end\",\"stage\":"<<info->GetStageIndex()<<",\"ticks\":"<<context.ticks
       <<",\"frame\":"<<info->GetCurrentFrame()<<",\"score\":\""<<info->GetScore()
       <<"\",\"expectedFrame\":"<<recorded->GetEndFrame()<<",\"expectedScore\":\""<<recorded->GetLastScore()<<"\"}";
    context.emit(out.str(),true);context.stage=nullptr;
    if(++context.closedStages==context.expectedStages) {
        context.emit("{\"kind\":\"run-end\"}",true);context.complete=true;
    }
}

struct Step {
    StgStageController* stage;int before;
    explicit Step(StgStageController* s):stage(s),before(Access::cursor(s->GetKeyReplayManager().GetPointer())) {}
    ~Step() noexcept(false) {
        if(std::uncaught_exceptions())return; // Never emit a partially failed step.
        if(!context.enabled||context.stage!=stage)return;
        const int after=Access::cursor(stage->GetKeyReplayManager().GetPointer());
        if(after==before)return; // Pauses do not consume replay input.
        if(after!=before+1)throw std::runtime_error("Verifier observed an invalid replay input step");
        Tick(stage,before);
    }
};
} }
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void woo_trace_flush() { woo::verify::context.flush(); }
#endif
#endif
