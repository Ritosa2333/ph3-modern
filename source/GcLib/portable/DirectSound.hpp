#pragma once
#include "../directx/DxConstant.hpp"
#include <memory>

struct WAVEFORMATEX {
    WORD wFormatTag=1,nChannels=2;
    DWORD nSamplesPerSec=48000,nAvgBytesPerSec=192000;
    WORD nBlockAlign=4,wBitsPerSample=16,cbSize=0;
};
struct SDL_AudioStream;
namespace directx {
class SoundInfo;
class SoundDivision;
class SoundPlayer;
class SoundInfoPanel;
namespace portable { struct AudioClip; struct VorbisStream; }
class DirectSoundManager {
public:
    friend SoundPlayer;
    enum { SD_VOLUME_MIN=-10000, SD_VOLUME_MAX=0 };
    enum FileFormat { SD_MIDI,SD_WAVE,SD_MP3,SD_OGG,SD_AWAVE,SD_UNKNOWN };
    DirectSoundManager();
    virtual ~DirectSoundManager();
    static DirectSoundManager* GetBase(){return thisBase_;}
    virtual bool Initialize(HWND);
    virtual void Clear();
    gstd::CriticalSection& GetLock(){return lock_;}
    gstd::ref_count_ptr<SoundPlayer> GetPlayer(std::wstring path,bool createAlways=false);
    gstd::ref_count_ptr<SoundDivision> CreateSoundDivision(int index);
    gstd::ref_count_ptr<SoundDivision> GetSoundDivision(int index);
    gstd::ref_count_ptr<SoundInfo> GetSoundInfo(std::wstring path);
    bool AddSoundInfoFromFile(std::wstring path);
    std::vector<gstd::ref_count_ptr<SoundInfo>> GetSoundInfoList();
    void SetFadeDeleteAll();
    void SetInfoPanel(gstd::ref_count_ptr<SoundInfoPanel> panel){panelInfo_=panel;}
    gstd::ref_count_ptr<SoundInfoPanel> GetInfoPanel(){return panelInfo_;}
    void Pump();
protected:
    static DirectSoundManager* thisBase_;
    gstd::CriticalSection lock_;
    std::map<int,gstd::ref_count_ptr<SoundDivision>> mapDivision_;
    std::map<std::wstring,std::list<gstd::ref_count_ptr<SoundPlayer>>> mapPlayer_;
    std::map<std::wstring,gstd::ref_count_ptr<SoundInfo>> mapInfo_;
    gstd::ref_count_ptr<SoundInfoPanel> panelInfo_;
    std::map<std::wstring,std::weak_ptr<portable::AudioClip>> clips_;
    SDL_AudioStream* stream_=nullptr;
    double lastPump_=0;
};
class SoundInfoPanel:public gstd::WindowLogger::Panel {
public:
    bool _AddedLogger(HWND) override;
    void Update(DirectSoundManager* manager);
private:
    std::wstring name_, info_;
};
class SoundInfo {
    friend DirectSoundManager;
public:
    std::wstring GetName(){return name_;}
    std::wstring GetTitle(){return title_;}
    double GetLoopStartTime(){return timeLoopStart_;}
    double GetLoopEndTime(){return timeLoopEnd_;}
protected:
    std::wstring name_,title_;
    double timeLoopStart_=0,timeLoopEnd_=0;
};
class SoundDivision {
public:
    enum { DIVISION_BGM=0,DIVISION_SE=1,DIVISION_VOICE=2 };
    double GetVolumeRate(){return volumeRate_;}
    void SetVolumeRate(double rate){volumeRate_=std::clamp(rate,0.0,100.0);}
private:
    double volumeRate_=100;
};
class SoundPlayer {
public:
    class PlayStyle;
    friend DirectSoundManager;
    enum { FADE_DEFAULT=-80 };
    SoundPlayer();
    virtual ~SoundPlayer();
    bool IsAutoDelete(){return bAutoDelete_;}
    void SetAutoDelete(bool enabled){bAutoDelete_=enabled;}
    bool IsLoop(){return bLoop_;}
    std::wstring GetPath(){return path_;}
    void SetSoundDivision(gstd::ref_count_ptr<SoundDivision> division){division_=division;}
    void SetSoundDivision(int index);
    gstd::ref_count_ptr<SoundDivision> GetSoundDivision(){return division_;}
    virtual bool Play();
    virtual bool Play(PlayStyle& style);
    virtual bool Stop();
    virtual bool IsPlaying(){return playing_;}
    virtual bool Seek(double seconds);
    virtual WAVEFORMATEX GetWaveFormat();
    virtual bool SetVolumeRate(double rate);
    virtual bool SetPanRate(double rate);
    virtual double GetVolumeRate(){return rateVolume_;}
    virtual void SetFade(double rate){rateVolumeFadePerSec_=rate;}
    virtual void SetFadeDelete(double rate){bFadeDelete_=true;SetFade(rate);}
    virtual double GetFadeVolumeRate(){return rateVolumeFadePerSec_;}
    virtual void Delete(){bDelete_=true;}
    void SetLoopEnable(bool enabled){bLoop_=enabled;}
    void SetLoopStartTime(double time){timeLoopStart_=time;}
    void SetLoopEndTime(double time){timeLoopEnd_=time;}
protected:
    std::wstring path_;
    DirectSoundManager* manager_=nullptr;
    gstd::ref_count_ptr<SoundDivision> division_;
    std::shared_ptr<portable::AudioClip> clip_;
    std::unique_ptr<portable::VorbisStream> vorbis_;
    bool bLoop_=false,bPause_=false,bDelete_=false,bFadeDelete_=false,bAutoDelete_=false,playing_=false;
    bool streaming_=false;
    double rateVolume_=100,rateVolumeFadePerSec_=0,pan_=0,position_=0,timeLoopStart_=0,timeLoopEnd_=0;
};
class SoundPlayer::PlayStyle {
public:
    virtual ~PlayStyle()=default;
    bool IsLoopEnable(){return bLoop_;}
    void SetLoopEnable(bool value){bLoop_=value;}
    double GetLoopStartTime(){return timeLoopStart_;}
    void SetLoopStartTime(double value){timeLoopStart_=value;}
    double GetLoopEndTime(){return timeLoopEnd_;}
    void SetLoopEndTime(double value){timeLoopEnd_=value;}
    double GetStartTime(){return timeStart_;}
    void SetStartTime(double value){timeStart_=value;}
    bool IsRestart(){return bRestart_;}
    void SetRestart(bool value){bRestart_=value;}
private:
    bool bLoop_=false,bRestart_=false;
    double timeLoopStart_=0,timeLoopEnd_=0,timeStart_=0;
};
}
