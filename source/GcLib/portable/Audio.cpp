// Included by the original DirectSound.cpp owner on the Web platform.
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <vorbis/vorbisfile.h>
#include <array>
#include <cmath>
#include <limits>

namespace directx::portable {
namespace { bool gVorbisFullDecode=false; }
void SetVorbisFullDecode(bool enabled){gVorbisFullDecode=enabled;}
struct AudioClip {
    int rate=48000,channels=2;
    bool streamed=false;
    std::size_t frameCount=0;
    std::vector<float> samples;
    std::shared_ptr<std::vector<char>> encodedVorbis;
    std::size_t Frames() const { return frameCount?frameCount:samples.size()/channels; }
};
namespace {
struct EncodedAudio { const std::vector<char>* bytes; std::size_t offset=0; };
std::size_t VorbisRead(void* destination,std::size_t size,std::size_t count,void* source) {
    auto& input=*static_cast<EncodedAudio*>(source);
    if(!size)return 0;
    count=std::min(count,(input.bytes->size()-input.offset)/size);
    std::memcpy(destination,input.bytes->data()+input.offset,count*size);input.offset+=count*size;return count;
}
int VorbisSeek(void* source,ogg_int64_t offset,int origin) {
    auto& input=*static_cast<EncodedAudio*>(source);
    std::int64_t base=origin==SEEK_SET?0:origin==SEEK_CUR?input.offset:origin==SEEK_END?input.bytes->size():-1;
    if(base<0||offset < -base||offset>static_cast<std::int64_t>(input.bytes->size())-base)return -1;
    input.offset=static_cast<std::size_t>(base+offset);return 0;
}
long VorbisTell(void* source){return static_cast<long>(static_cast<EncodedAudio*>(source)->offset);}
constexpr std::size_t VorbisWindowFrames=4096;

std::shared_ptr<AudioClip> DecodeAudio(const std::vector<char>& bytes) {
    if(bytes.size()<12)throw std::runtime_error("Truncated audio file");
    auto clip=std::make_shared<AudioClip>();
    if(std::memcmp(bytes.data(),"OggS",4)==0){
        EncodedAudio input{&bytes};OggVorbis_File file{};
        ov_callbacks callbacks{VorbisRead,VorbisSeek,nullptr,VorbisTell};
        if(ov_open_callbacks(&input,&file,nullptr,0,callbacks)<0)throw std::runtime_error("Invalid Ogg Vorbis audio");
        try{
            auto* info=ov_info(&file,-1);
            if(!info||info->channels<1||info->channels>2||info->rate<8000||info->rate>192000)throw std::runtime_error("Unsupported Vorbis channel count or sample rate");
            clip->channels=info->channels;clip->rate=info->rate;clip->streamed=true;
            ogg_int64_t frames=ov_pcm_total(&file,-1);
            if(frames<0||frames>static_cast<ogg_int64_t>(clip->rate)*60*30)throw std::runtime_error("Vorbis duration exceeds the Web decode limit");
            clip->frameCount=static_cast<std::size_t>(frames);
            // Long music tracks must not be decoded synchronously on the game
            // thread. Keep the compressed bytes and give each voice a small
            // Vorbis decode window instead. Short OGG effects stay resident.
            if(!gVorbisFullDecode&&frames>static_cast<ogg_int64_t>(clip->rate)*20){
                clip->encodedVorbis=std::make_shared<std::vector<char>>(bytes);
                ov_clear(&file);
                return clip;
            }
            clip->samples.reserve(static_cast<std::size_t>(frames)*clip->channels);
            int section=0;
            for(;;){
                float** pcm=nullptr;long count=ov_read_float(&file,&pcm,4096,&section);
                if(count==0)break;if(count<0)throw std::runtime_error("Corrupt Ogg Vorbis packet");
                auto* current=ov_info(&file,section);
                if(current->rate!=clip->rate||current->channels!=clip->channels)throw std::runtime_error("Vorbis chained stream format change is unsupported");
                for(long frame=0;frame<count;++frame)for(int channel=0;channel<clip->channels;++channel)clip->samples.push_back(pcm[channel][frame]);
            }
            ov_clear(&file);
        }catch(...){ov_clear(&file);throw;}
    }else if(std::memcmp(bytes.data(),"RIFF",4)==0&&std::memcmp(bytes.data()+8,"WAVE",4)==0){
        SDL_AudioSpec source{};Uint8* pcm=nullptr;Uint32 length=0;
        if(!SDL_LoadWAV_IO(SDL_IOFromConstMem(bytes.data(),bytes.size()),true,&source,&pcm,&length))throw std::runtime_error(SDL_GetError());
        SDL_AudioSpec destination=source;destination.format=SDL_AUDIO_F32;
        Uint8* converted=nullptr;int convertedLength=0;
        bool ok=SDL_ConvertAudioSamples(&source,pcm,length,&destination,&converted,&convertedLength);
        SDL_free(pcm);if(!ok)throw std::runtime_error(SDL_GetError());
        if(source.channels<1||source.channels>2){SDL_free(converted);throw std::runtime_error("Only mono/stereo WAV is supported");}
        clip->rate=source.freq;clip->channels=source.channels;clip->streamed=bytes.size()>=1024*1024;
        clip->samples.resize(convertedLength/sizeof(float));std::memcpy(clip->samples.data(),converted,convertedLength);SDL_free(converted);
    }else throw std::runtime_error("Unsupported audio format (this build decodes WAV and Ogg Vorbis)");
    if(!clip->Frames())throw std::runtime_error("Audio file has no samples");
    return clip;
}
double VolumeGain(double rate){return rate<=0?0:rate>=1?1:std::pow(10.0,std::max(-10000,int(3320*std::log10(rate)))/2000.0);}
}

struct VorbisStream {
    explicit VorbisStream(const std::shared_ptr<AudioClip>& clip):clip_(clip),input_{clip->encodedVorbis.get()} {
        if(!clip_||!clip_->encodedVorbis)throw std::runtime_error("Streaming Vorbis clip has no encoded data");
        ov_callbacks callbacks{VorbisRead,VorbisSeek,nullptr,VorbisTell};
        if(ov_open_callbacks(&input_,&file_,nullptr,0,callbacks)<0)throw std::runtime_error("Cannot open streaming Vorbis audio");
        open_=true;
    }
    ~VorbisStream(){if(open_)ov_clear(&file_);}
    void Seek(std::size_t frame){
        if(ov_pcm_seek(&file_,static_cast<ogg_int64_t>(std::min(frame,clip_->Frames())))!=0)
            throw std::runtime_error("Cannot seek streaming Vorbis audio");
        start_=frame;frames_=0;buffer_.clear();
    }
    float Sample(std::size_t frame,int channel){
        if(frame>=clip_->Frames())return 0;
        if(frame<start_||frame>=start_+frames_)Fill(frame);
        if(frame<start_||frame>=start_+frames_)return 0;
        channel=std::clamp(channel,0,clip_->channels-1);
        return buffer_[(frame-start_)*clip_->channels+channel];
    }
private:
    void Fill(std::size_t frame){
        const auto current=ov_pcm_tell(&file_);
        if(current<0||static_cast<std::size_t>(current)!=frame){
            if(ov_pcm_seek(&file_,static_cast<ogg_int64_t>(frame))!=0)
                throw std::runtime_error("Cannot seek streaming Vorbis window");
        }
        start_=frame;frames_=0;buffer_.clear();
        buffer_.reserve(VorbisWindowFrames*clip_->channels);
        int section=0;
        while(frames_<VorbisWindowFrames){
            float** pcm=nullptr;
            const long wanted=static_cast<long>(VorbisWindowFrames-frames_);
            long count=ov_read_float(&file_,&pcm,wanted,&section);
            if(count==0)break;
            if(count<0)throw std::runtime_error("Corrupt streaming Vorbis packet");
            auto* info=ov_info(&file_,section);
            if(!info||info->rate!=clip_->rate||info->channels!=clip_->channels)
                throw std::runtime_error("Streaming Vorbis format changed");
            for(long i=0;i<count;++i)
                for(int channel=0;channel<clip_->channels;++channel)
                    buffer_.push_back(pcm[channel][i]);
            frames_+=static_cast<std::size_t>(count);
        }
    }
    std::shared_ptr<AudioClip> clip_;
    EncodedAudio input_;
    OggVorbis_File file_{};
    bool open_=false;
    std::size_t start_=0,frames_=0;
    std::vector<float> buffer_;
};
}

DirectSoundManager* DirectSoundManager::thisBase_=nullptr;
DirectSoundManager::DirectSoundManager(){for(int index:{SoundDivision::DIVISION_BGM,SoundDivision::DIVISION_SE,SoundDivision::DIVISION_VOICE})CreateSoundDivision(index);}
DirectSoundManager::~DirectSoundManager(){Clear();if(stream_)SDL_DestroyAudioStream(stream_);if(thisBase_==this)thisBase_=nullptr;}
bool DirectSoundManager::Initialize(HWND){
    if(thisBase_&&thisBase_!=this)return false;
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))throw std::runtime_error(SDL_GetError());
    SDL_AudioSpec spec{SDL_AUDIO_F32,2,48000};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!stream_)throw std::runtime_error(SDL_GetError());
    if(!SDL_ResumeAudioStreamDevice(stream_))throw std::runtime_error(SDL_GetError());
    lastPump_=emscripten_get_now();thisBase_=this;
    std::printf("[Woo audio] SDL3 stereo 48000 Hz mixer initialized\n");return true;
}
void DirectSoundManager::Clear(){for(auto& [path,players]:mapPlayer_)for(auto& player:players)player->Stop();mapPlayer_.clear();clips_.clear();if(stream_)SDL_ClearAudioStream(stream_);}
gstd::ref_count_ptr<SoundDivision> DirectSoundManager::CreateSoundDivision(int index){auto& division=mapDivision_[index];if(division==NULL)division=new SoundDivision();return division;}
gstd::ref_count_ptr<SoundDivision> DirectSoundManager::GetSoundDivision(int index){auto found=mapDivision_.find(index);return found==mapDivision_.end()?gstd::ref_count_ptr<SoundDivision>():found->second;}
gstd::ref_count_ptr<SoundInfo> DirectSoundManager::GetSoundInfo(std::wstring path){auto found=mapInfo_.find(PathProperty::GetFileName(path));return found==mapInfo_.end()?gstd::ref_count_ptr<SoundInfo>():found->second;}
gstd::ref_count_ptr<SoundPlayer> DirectSoundManager::GetPlayer(std::wstring path,bool createAlways){
    path=PathProperty::GetUnique(path);
    auto found=mapPlayer_.find(path);
    if(!createAlways&&found!=mapPlayer_.end())for(auto& player:found->second)if(!player->bDelete_)return player;
    auto clip=clips_[path].lock();
    if(!clip){
        const double decodeBegin=emscripten_get_now();
        auto reader=FileManager::GetBase()->GetFileReader(path);
        if(reader==NULL||!reader->Open()){Logger::WriteTop(ErrorUtility::GetFileNotFoundErrorMessage(path));return NULL;}
        int size=reader->GetFileSize();if(size<=0||size>256*1024*1024)throw std::runtime_error("Invalid encoded audio size");
        std::vector<char> bytes(size);if(reader->Read(bytes.data(),size)!=static_cast<DWORD>(size))throw std::runtime_error("Incomplete audio archive read");
        clip=portable::DecodeAudio(bytes);clips_[path]=clip;
        const double decodeMs=emscripten_get_now()-decodeBegin;
        auto pathUtf8=StringUtility::ConvertWideToMulti(path,CP_UTF8);
        std::printf("[Woo audio] %s %s (%zu frames, %.1f ms)\n",clip->encodedVorbis?"opened stream":"decoded",pathUtf8.c_str(),clip->Frames(),decodeMs);
#ifdef PH3_PERF_SCAN
        EM_ASM({
            const d=globalThis.ph3Diagnostics;if(!d)return;
            const list=d.audioDecodes||(d.audioDecodes=[]);
            list.push(({path:UTF8ToString($0),ms:$1,frames:$2,bytes:$3,lazy:!!$4}));
        },pathUtf8.c_str(),decodeMs,double(clip->Frames()),size,clip->encodedVorbis?1:0);
#endif
    }
    gstd::ref_count_ptr<SoundPlayer> player=new SoundPlayer();player->manager_=this;player->path_=path;player->clip_=clip;player->streaming_=clip->streamed;
    if(clip->encodedVorbis)player->vorbis_=std::make_unique<portable::VorbisStream>(clip);
    mapPlayer_[path].push_back(player);return player;
}
bool DirectSoundManager::AddSoundInfoFromFile(std::wstring path){
    auto reader=FileManager::GetBase()->GetFileReader(path);if(reader==NULL||!reader->Open())return false;
    std::vector<char> text(reader->GetFileSize()+1);reader->Read(text.data(),text.size()-1);Scanner scanner(text);
    while(scanner.HasNext()){
        Token token=scanner.Next();if(token.GetType()==Token::TK_EOF)break;
        if(token.GetType()!=Token::TK_ID||token.GetElement()!=L"SoundInfo")continue;
        token=scanner.Next();if(token.GetType()==Token::TK_NEWLINE)token=scanner.Next();scanner.CheckType(token,Token::TK_OPENC);
        gstd::ref_count_ptr<SoundInfo> info=new SoundInfo();
        while(scanner.HasNext()){
            token=scanner.Next();if(token.GetType()==Token::TK_CLOSEC)break;if(token.GetType()!=Token::TK_ID)continue;
            std::wstring key=token.GetElement();scanner.CheckType(scanner.Next(),Token::TK_EQUAL);token=scanner.Next();
            if(key==L"name")info->name_=token.GetString();else if(key==L"title")info->title_=token.GetString();
            else if(key==L"loop_start_time")info->timeLoopStart_=token.GetReal();else if(key==L"loop_end_time")info->timeLoopEnd_=token.GetReal();
            else throw wexception(L"Unknown SoundInfo field: "+key);
        }
        if(!info->name_.empty())mapInfo_[info->name_]=info;
    }
    return true;
}
std::vector<gstd::ref_count_ptr<SoundInfo>> DirectSoundManager::GetSoundInfoList(){std::vector<gstd::ref_count_ptr<SoundInfo>> result;for(auto& [name,info]:mapInfo_)result.push_back(info);return result;}
void DirectSoundManager::SetFadeDeleteAll(){for(auto& [path,players]:mapPlayer_)for(auto& player:players)player->SetFadeDelete(SoundPlayer::FADE_DEFAULT);}
bool SoundInfoPanel::_AddedLogger(HWND){name_=L"Sound";return true;}
void SoundInfoPanel::Update(DirectSoundManager*){info_=L"Audio diagnostics are available in the browser console.";}
SoundPlayer::SoundPlayer()=default;
SoundPlayer::~SoundPlayer(){Stop();}
void SoundPlayer::SetSoundDivision(int index){if(manager_)division_=manager_->GetSoundDivision(index);}
bool SoundPlayer::Play(){PlayStyle style;return Play(style);}
bool SoundPlayer::Play(PlayStyle& style){
    if(!clip_||bDelete_)return false;
    if(streaming_&&playing_)return true;
    if(bFadeDelete_)rateVolume_=100;
    bFadeDelete_=false;rateVolumeFadePerSec_=0;
    bLoop_=style.IsLoopEnable();timeLoopStart_=style.GetLoopStartTime();timeLoopEnd_=style.GetLoopEndTime();
    auto info=manager_?manager_->GetSoundInfo(path_):gstd::ref_count_ptr<SoundInfo>();
    if(info!=NULL){timeLoopStart_=info->GetLoopStartTime();timeLoopEnd_=info->GetLoopEndTime();}
    if(!(bPause_&&style.IsRestart()))Seek(style.GetStartTime());
    bPause_=false;playing_=true;return true;
}
bool SoundPlayer::Stop(){if(playing_)bPause_=true;playing_=false;return true;}
bool SoundPlayer::Seek(double seconds){if(!clip_||!std::isfinite(seconds))return false;position_=std::clamp(seconds*clip_->rate,0.0,double(clip_->Frames()));if(vorbis_)vorbis_->Seek(static_cast<std::size_t>(position_));return true;}
WAVEFORMATEX SoundPlayer::GetWaveFormat(){WAVEFORMATEX format;if(clip_){format.nChannels=clip_->channels;format.nSamplesPerSec=clip_->rate;format.nBlockAlign=2*clip_->channels;format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;}return format;}
bool SoundPlayer::SetVolumeRate(double rate){if(!std::isfinite(rate))return false;rateVolume_=std::clamp(rate,0.0,100.0);return true;}
bool SoundPlayer::SetPanRate(double rate){if(!std::isfinite(rate))return false;pan_=std::clamp(rate,-100.0,100.0);return true;}

void DirectSoundManager::Pump(){
    if(!stream_)return;
    double now=emscripten_get_now(),elapsed=std::max(0.0,(now-lastPump_)/1000);lastPump_=now;
    std::vector<SoundPlayer*> active;
    for(auto iterator=mapPlayer_.begin();iterator!=mapPlayer_.end();){
        auto& players=iterator->second;
        for(auto player=players.begin();player!=players.end();){
            auto* voice=player->GetPointer();
            if(voice->rateVolumeFadePerSec_!=0){voice->SetVolumeRate(voice->rateVolume_+voice->rateVolumeFadePerSec_*elapsed);if(voice->rateVolume_<=0&&voice->bFadeDelete_){voice->Stop();voice->Delete();}}
            if(voice->bDelete_||(voice->bAutoDelete_&&!voice->playing_)){voice->Stop();player=players.erase(player);}
            else{if(voice->playing_)active.push_back(voice);++player;}
        }
        if(players.empty())iterator=mapPlayer_.erase(iterator);else ++iterator;
    }
    constexpr int block=512,targetFrames=2048;
    int queued=SDL_GetAudioStreamQueued(stream_);if(queued<0)throw std::runtime_error(SDL_GetError());
    std::array<float,block*2> mix;
    while(queued<targetFrames*int(2*sizeof(float))){
        mix.fill(0);
        for(auto* voice:active){
            if(!voice->playing_)continue;
            const auto& clip=*voice->clip_;double step=double(clip.rate)/48000;
            double end=voice->bLoop_&&voice->timeLoopEnd_>0?std::min(double(clip.Frames()),voice->timeLoopEnd_*clip.rate):double(clip.Frames());
            double start=std::clamp(voice->timeLoopStart_*clip.rate,0.0,std::max(0.0,end-1));
            double rate=voice->rateVolume_/100*(voice->division_!=NULL?voice->division_->GetVolumeRate()/100:1);
            double gain=portable::VolumeGain(rate),panDb=5000*rate*voice->pan_/100;
            double left=gain*(panDb>0?std::pow(10.0,-panDb/2000):1),right=gain*(panDb<0?std::pow(10.0,panDb/2000):1);
            for(int frame=0;frame<block;++frame){
                if(voice->position_>=end){if(voice->bLoop_&&end>start)voice->position_=start+std::fmod(voice->position_-start,end-start);else{voice->playing_=false;voice->bPause_=false;break;}}
                std::size_t a=std::min(static_cast<std::size_t>(voice->position_),clip.Frames()-1),b=std::min(a+1,clip.Frames()-1);float fraction=voice->position_-a;
                if(voice->bLoop_&&b>=static_cast<std::size_t>(end))b=static_cast<std::size_t>(start);
                for(int channel=0;channel<2;++channel){
                    int source=std::min(channel,clip.channels-1);
                    float sa=voice->vorbis_?voice->vorbis_->Sample(a,source):clip.samples[a*clip.channels+source];
                    float sb=voice->vorbis_?voice->vorbis_->Sample(b,source):clip.samples[b*clip.channels+source];
                    float sample=sa*(1-fraction)+sb*fraction;mix[frame*2+channel]+=sample*float(channel?right:left);
                }
                voice->position_+=step;
            }
        }
        float peak=0;for(float& sample:mix){sample=std::clamp(sample,-1.f,1.f);peak=std::max(peak,std::abs(sample));}
        if(!SDL_PutAudioStreamData(stream_,mix.data(),sizeof(mix)))throw std::runtime_error(SDL_GetError());
        queued+=sizeof(mix);
        EM_ASM({const d=globalThis.ph3Diagnostics;if(d){d.audioPeak=$0;d.audioVoices=$1;d.audioSamples=(d.audioSamples||0)+$2;}},peak,active.size(),block);
    }
}
