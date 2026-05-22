#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <wayland-server.h>
#include "../../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

#include "EventLoopTimer.hpp"

namespace Aquamarine {
    struct SPollFD;
};

struct SEventLoopDoLaterLock {
    SEventLoopDoLaterLock(uint64_t seq);
    ~SEventLoopDoLaterLock();

    uint64_t seq = 0;
};

class CEventLoopManager {
  public:
    CEventLoopManager(wl_display* display, wl_event_loop* wlEventLoop);
    ~CEventLoopManager();

    void enterLoop();

    // Note: will remove the timer if the ptr is lost.
    void addTimer(SP<CEventLoopTimer> timer);
    void removeTimer(SP<CEventLoopTimer> timer);

    void onTimerFire();

    // fired periodically by a CLOCK_BOOTTIME timerfd; detects a resume from
    // suspend and triggers session recovery if one is found.
    void onSuspendCheck();

    // returns how long the system was suspended (ms) given the time elapsed on
    // CLOCK_BOOTTIME and CLOCK_MONOTONIC across the same interval, or 0 if the
    // gap is below thresholdMs. Pure; separated out for testing.
    static uint64_t suspendGapMs(uint64_t bootElapsedMs, uint64_t monoElapsedMs, uint64_t thresholdMs);

    // schedules a recalc of the timers
    void scheduleRecalc();

    // schedules a function to run later, aka in a wayland idle event. Returns a sequence which can be used to remove it.
    uint64_t doLater(const std::function<void()>& fn);
    void     removeDoLater(uint64_t seq);

    // automatically cleaned up doLater instance
    [[nodiscard]] UP<SEventLoopDoLaterLock> doLaterLock(const std::function<void()>& fn);

    struct SIdleData {
        wl_event_source*                                        eventSource = nullptr;
        std::vector<std::pair<uint64_t, std::function<void()>>> fns;
    };

    struct SReadableWaiter {
        wl_event_source*               source;
        Hyprutils::OS::CFileDescriptor fd;
        std::function<void()>          fn;

        SReadableWaiter(wl_event_source* src, Hyprutils::OS::CFileDescriptor f, std::function<void()> func) : source(src), fd(std::move(f)), fn(std::move(func)) {}

        ~SReadableWaiter() {
            if (source) {
                wl_event_source_remove(source);
                source = nullptr;
            }
        }

        // copy
        SReadableWaiter(const SReadableWaiter&)            = delete;
        SReadableWaiter& operator=(const SReadableWaiter&) = delete;

        // move
        SReadableWaiter(SReadableWaiter&& other) noexcept            = default;
        SReadableWaiter& operator=(SReadableWaiter&& other) noexcept = default;
    };

    // schedule function to when fd is readable (WL_EVENT_READABLE / POLLIN),
    // takes ownership of fd
    void doOnReadable(Hyprutils::OS::CFileDescriptor fd, std::function<void()>&& fn);
    void onFdReadable(SReadableWaiter* waiter);
    void onFdReadableFail(SReadableWaiter* waiter);

  private:
    // Manages the event sources after AQ pollFDs change.
    void syncPollFDs();
    void nudgeTimers();

    struct SEventSourceData {
        SP<Aquamarine::SPollFD> pollFD;
        wl_event_source*        eventSource = nullptr;
    };

    struct {
        wl_event_loop*   loop        = nullptr;
        wl_display*      display     = nullptr;
        wl_event_source* eventSource = nullptr;
    } m_wayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        Hyprutils::OS::CFileDescriptor   timerfd;
        bool                             recalcScheduled = false;
    } m_timers;

    // detects a resume from suspend that the session layer never reported
    // (e.g. s2idle, which keeps the seat active). The timerfd is CLOCK_BOOTTIME
    // so it keeps counting through suspend; comparing its progress against
    // CLOCK_MONOTONIC (which freezes) on each fire reveals the suspended time.
    struct {
        Hyprutils::OS::CFileDescriptor timerfd;
        wl_event_source*               eventSource = nullptr;
        uint64_t                       lastBootMs  = 0;
        uint64_t                       lastMonoMs  = 0;
        // second recovery pass: the resume sequence can disable an output
        // again shortly after the first onResume(), so re-run it once the
        // sequence has settled.
        SP<CEventLoopTimer> recoveryTimer;
    } m_suspendDetect;

    SIdleData                        m_idle;
    std::map<int, SEventSourceData>  m_aqEventSources;
    std::vector<UP<SReadableWaiter>> m_readableWaiters;

    struct {
        CHyprSignalListener pollFDsChanged;
    } m_listeners;

    wl_event_source* m_configWatcherInotifySource = nullptr;

    friend class CAsyncDialogBox;
    friend class CMainLoopExecutor;
};

inline UP<CEventLoopManager> g_pEventLoopManager;
