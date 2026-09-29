#include "Thread.hpp"
#include "GstdUtility.hpp"

using namespace gstd;

#ifdef __EMSCRIPTEN__
// Browser jobs execute cooperatively on the calling main thread. Start does
// real bounded work; there is no invented OS handle or fake running worker.
Thread::Thread() : hThread_(NULL), idThread_(0), status_(STOP) {}
Thread::~Thread() { Stop(); }
unsigned int Thread::_StaticRun(LPVOID data) {
	auto* thread = static_cast<Thread*>(data);
	thread->status_ = RUN;
	try { thread->_Run(); thread->status_ = STOP; }
	catch (...) { thread->status_ = STOP; throw; }
	return 0;
}
void Thread::Start() {
	if (status_ != STOP) throw std::runtime_error("A cooperative job cannot restart itself");
	_StaticRun(this);
}
void Thread::Stop() { if (status_ == RUN) status_ = REQUEST_STOP; }
bool Thread::IsStop() { return status_ == STOP; }
DWORD Thread::Join(int) {
	if (status_ != STOP) throw std::runtime_error("Cannot block the Web main thread waiting for itself");
	return WAIT_OBJECT_0;
}
CriticalSection::CriticalSection() : idThread_(0), countLock_(0) {}
CriticalSection::~CriticalSection() = default;
void CriticalSection::Enter() { cs_.lock(); ++countLock_; }
void CriticalSection::Leave() {
	if (countLock_ <= 0) throw std::runtime_error("Unbalanced Woo critical section");
	--countLock_; cs_.unlock();
}
ThreadSignal::ThreadSignal(bool manual) : hEvent_(NULL), manualReset_(manual) {}
ThreadSignal::~ThreadSignal() = default;
DWORD ThreadSignal::Wait(int milliseconds) {
	if (signaled_) { if (!manualReset_) signaled_ = false; return WAIT_OBJECT_0; }
	if (milliseconds != 0) throw std::runtime_error("Blocking signal waits are not supported by the cooperative Web platform");
	return WAIT_TIMEOUT;
}
void ThreadSignal::SetSignal(bool enabled) { signaled_ = enabled; }
#else
//================================================================
//Thread
Thread::Thread()
{
	hThread_ = NULL;
	idThread_ = 0;
	status_ = STOP;
}
Thread::~Thread()
{
	this->Stop();
	this->Join();
	if (hThread_ != NULL) {
		::CloseHandle(hThread_);
		hThread_ = NULL;
		idThread_ = 0;
	}
}
unsigned int __stdcall Thread::_StaticRun(LPVOID data)
{
	try {
		Thread* thread = reinterpret_cast<Thread*>(data);
		thread->status_ = RUN;
		thread->_Run();
		thread->status_ = STOP;
	} catch (...) {
		//エラーは無視
	}
	return 0;
}
void Thread::Start()
{
	if (status_ != STOP) {
		this->Stop();
		this->Join();
	}

	hThread_ = (HANDLE)_beginthreadex(NULL, 0, _StaticRun, (void*)this, 0, &idThread_);
}
void Thread::Stop()
{
	if (status_ == RUN)
		status_ = REQUEST_STOP;
}
bool Thread::IsStop()
{
	return hThread_ == NULL || status_ == STOP;
}
DWORD Thread::Join(int mills)
{
	DWORD res = WAIT_OBJECT_0;

	if (hThread_ != NULL) {
		res = ::WaitForSingleObject(hThread_, mills);
	}

	if (hThread_ != NULL) {
		if (res != WAIT_TIMEOUT)
			::CloseHandle(hThread_); //タイムアウトの場合クローズできない
		hThread_ = NULL;
		idThread_ = 0;
		status_ = STOP;
	}
	return res;
}

//================================================================
//CriticalSection
CriticalSection::CriticalSection()
{
	idThread_ = NULL;
	countLock_ = 0;
	::InitializeCriticalSection(&cs_);
}
CriticalSection::~CriticalSection()
{
	::DeleteCriticalSection(&cs_);
}
void CriticalSection::Enter()
{
	if (::GetCurrentThreadId() == idThread_) { //カレントスレッド
		countLock_++;
		return;
	}

	::EnterCriticalSection(&cs_);
	countLock_ = 1;
	idThread_ = ::GetCurrentThreadId();
}
void CriticalSection::Leave()
{
	if (::GetCurrentThreadId() == idThread_) {
		countLock_--;
		if (countLock_ != 0)
			return;
		if (countLock_ < 0)
			throw std::runtime_error("CriticalSection：Lockしていません");
	} else {
		throw std::runtime_error("CriticalSection：LockしていないのにUnlockしようとしました");
	}
	idThread_ = NULL;
	::LeaveCriticalSection(&cs_);
}

//================================================================
//ThreadSignal
ThreadSignal::ThreadSignal(bool bManualReset)
{
	BOOL bManual = bManualReset ? TRUE : FALSE;
	hEvent_ = ::CreateEvent(NULL, bManual, FALSE, NULL);
}
ThreadSignal::~ThreadSignal()
{
	::CloseHandle(hEvent_);
}
DWORD ThreadSignal::Wait(int mills)
{
	DWORD res = WAIT_OBJECT_0;

	if (hEvent_ != NULL) {
		res = ::WaitForSingleObject(hEvent_, mills);
	}

	return res;
}
void ThreadSignal::SetSignal(bool bOn)
{
	if (bOn) {
		::SetEvent(hEvent_);
	} else {
		::ResetEvent(hEvent_);
	}
}
#endif
