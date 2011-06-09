/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   FrameAPI.h
 *  @brief  Frame core API. Exposes framework's update tick.
 * 
 *  FrameAPI object can be used to:
 *  -retrieve signal every time frame has been processed
 *  -retrieve the wall clock time of Framework
 *  -trigger delayed signals when spesified amount of time has elapsed.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "FrameAPI.h"
#include "HighPerfClock.h"
#include "Profiler.h"
#include <QTimer>

#include "MemoryLeakCheck.h"

DelayedSignal::DelayedSignal(boost::uint64_t startTime)
:startTime_(startTime)
{
}

void DelayedSignal::Expire()
{
    emit Triggered((float)((GetCurrentClockTime() - startTime_) / GetCurrentClockFreq()));
}

FrameAPI::FrameAPI(Framework *framework)
:QObject(framework),
currentFrameNumber(0)
{
    startTime_ = GetCurrentClockTime();
}

FrameAPI::~FrameAPI()
{
    Reset();
}

void FrameAPI::Reset()
{
    qDeleteAll(delayedSignals_);
}

float FrameAPI::GetWallClockTime() const
{
    return (GetCurrentClockTime() - startTime_) / GetCurrentClockFreq();
}

DelayedSignal *FrameAPI::DelayedExecute(float time)
{
    DelayedSignal *delayed = new DelayedSignal(GetCurrentClockTime());
    QTimer::singleShot(time*1000, delayed, SLOT(Expire()));
    connect(delayed, SIGNAL(Triggered(float)), SLOT(DeleteDelayedSignal()));
    delayedSignals_.push_back(delayed);
    return delayed;
}

void FrameAPI::DelayedExecute(float time, const QObject *receiver, const char *member)
{
    DelayedSignal *delayed = new DelayedSignal(GetCurrentClockTime());
    QTimer::singleShot(time*1000, delayed, SLOT(Expire()));
    connect(delayed, SIGNAL(Triggered(float)), receiver, member);
    connect(delayed, SIGNAL(Triggered(float)), SLOT(DeleteDelayedSignal()));
    delayedSignals_.push_back(delayed);
}

void FrameAPI::Update(float frametime)
{
    PROFILE(FrameAPI_Update);

    emit Updated(frametime);

    ++currentFrameNumber;
    if (currentFrameNumber < 0)
        currentFrameNumber = 0;
}

void FrameAPI::DeleteDelayedSignal()
{
    DelayedSignal *expiredSignal = checked_static_cast<DelayedSignal *>(sender());
    assert(expiredSignal);
    delayedSignals_.removeOne(expiredSignal);
    SAFE_DELETE_LATER(expiredSignal);
}

int FrameAPI::FrameNumber() const
{
    return currentFrameNumber;
}