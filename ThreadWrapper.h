/*
	Thread Wrapper v.2.0

	Copyright (C) 2017 by Sergey A Kryukov
	http://www.SAKryukov.org
	http://www.codeproject.com/Members/SAKryukov	

	Original publications:
	https://www.codeproject.com/Articles/1177478/Thread-Wrapper-CPP
	https://www.codeproject.com/Articles/1177869/Conveyor-Thread-Wrapper-CPP
	https://www.codeproject.com/Articles/1170503/The-Impossibly-Fast-Cplusplus-Delegates-Fixed
	https://www.codeproject.com/Tips/149540/Simple-Blocking-Queue-for-Thread-Communication-and
*/

#pragma once
#include <thread>
#include <condition_variable>
#include <exception>
#include "Delegate.h"
#include <iostream>
#include <sstream>
#include <pthread.h>
#include "zf_log.h"

class ThreadWrapper {
public:
    ThreadWrapper() : running( false), started(false) {} 
    void Start(std::string threadName = "" , bool background = true) {
        
        m_threadName = threadName;
        running = true;
        done = false;
		thread = std::thread(&ThreadWrapper::BodyWrapper, this);
		// another way to do it:
		//std::thread tmp(&ThreadWrapper::BodyWrapper, this);
		//thread.swap(tmp);
		if (background)
			thread.detach();
        
        m_tid = thread.get_id();
        std::ostringstream sid;
        sid << m_tid;
        ZF_LOGD("Thread  started %s %s",sid.str().c_str(), m_threadName.c_str()  );
	} //Start
    virtual ~ThreadWrapper()
    {
        std::ostringstream sid;
        sid << m_tid;
        ZF_LOGD("Thread destroyed %s %s",sid.str().c_str(), m_threadName.c_str()  );
    }
	void Join() {
		if (thread.joinable())
			thread.join();
		else { //fallback for the case the thread was detached:
			std::unique_lock<std::mutex> ul(joinMutex);
			joinEvent.wait(ul, [=] { return !running || done; });
		} // if
	} //Join

	void PutToSleep() {
		std::lock_guard<std::mutex> lock(stateMutex);
		state = Action::sleep;
		stateEvent.notify_one();
	} //PutToSleep

	void WakeUp() {
		std::lock_guard<std::mutex> lock(stateMutex);
        if (state == Action::sleep)
            state = Action::wakeup;
		stateEvent.notify_one();
	} //WakeUp

	void Abort() { // works only at the point where thread Body calls SyncPoint
		SetAbort(true);
	} //Abort

private:

	enum class Action {
		sleep = 0, wakeup = 1,
		shallowAbort = wakeup | 2, deepAbort = wakeup | 4
	};

	class ThreadAbortException : std::exception {};

	void BodyWrapper() {
		try {
            if( m_threadName.size() > 0)
                pthread_setname_np(m_threadName.c_str());
            started = true;
			Body();
		} catch (ThreadAbortException&) {
			Aborted();
		} catch (std::exception& ex) {
			ExceptionCaught(ex);
		} catch (...) {
			UnknownExceptionCaught();
		} //exception
		std::lock_guard<std::mutex> lock(joinMutex);
		done = true;
		joinEvent.notify_all();
		Terminated();
	} //BodyWrapper
    std::thread::id  m_tid;
    std::string m_threadName;
	std::thread thread;
	std::condition_variable stateEvent, joinEvent;
	std::mutex stateMutex, joinMutex;
	Action state = Action::wakeup; // protected by statusMutex, statusEvent
	
protected:
    
    
    std::atomic<bool> running;
    std::atomic<bool> started;
    bool done = false; // protected by joinMutex, joinEvent
    
	void SyncPoint(bool autoReset = false) {
		std::unique_lock<std::mutex> ul(stateMutex);
		stateEvent.wait(ul, [=] {
			auto result = ((int)state & (int)Action::wakeup) > 0;
			if (state == Action::deepAbort)
				throw ThreadAbortException();
			else if (state == Action::shallowAbort)
				throw ShallowThreadAbortException();
			if (autoReset)
				state = Action::sleep;
			return result;
		});
	} //SyncPoint

	// exposed to ConveyorThreadWrapper, instead of exposing Action state:
	class ShallowThreadAbortException : ThreadAbortException {};

	void SetAbort(bool set, bool shallow = true) {
		std::lock_guard<std::mutex> lock(stateMutex);
		if (set) {
			if (shallow)
				state = Action::shallowAbort;
			else
				state = Action::deepAbort;
		} else //clear abort
			state = Action::wakeup;
		stateEvent.notify_one();
	} //SetAbort

	static void InitializeSyncPointDelegate(SA::delegate<void(bool)>& del, ThreadWrapper* instance) {
		del = SA::delegate<void(bool)>::create<ThreadWrapper, &ThreadWrapper::SyncPoint>(instance);
	} //InitializeSyncPointDelegate

	virtual void Body() = 0;
	virtual void Aborted() {}
	virtual void Terminated() {}
	virtual void ExceptionCaught(std::exception exception) {}
	virtual void UnknownExceptionCaught() {}

}; /* class ThreadWrapper */
