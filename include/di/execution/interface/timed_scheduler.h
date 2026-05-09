#include "di/chrono/time_point/time_point.h"
#include "di/execution/concepts/scheduler.h"
#include "di/execution/concepts/sender.h"
#include "di/function/tag_invoke.h"
#include "di/util/declval.h"

namespace di::execution {
namespace timed_scheduler_ns {
    struct NowFunction {
        template<concepts::Scheduler Sched>
        requires(concepts::TagInvocable<NowFunction, Sched>)
        constexpr static auto operator()(Sched&& sched) -> concepts::InstanceOf<TimePoint> auto {
            return tag_invoke(NowFunction {}, di::forward<Sched>(sched));
        }
    };

    constexpr inline auto now = NowFunction {};

    template<typename Sched>
    concept SchedulerWithNow =
        concepts::Scheduler<Sched> && requires(Sched&& sched) { now(di::forward<Sched>(sched)); };

    template<SchedulerWithNow Sched>
    using SchedulerTimePoint = decltype(now(di::declval<Sched>()));

    template<SchedulerWithNow Sched>
    using SchedulerDuration = SchedulerTimePoint<Sched>::Duration;

    template<SchedulerWithNow Sched>
    using SchedulerClock = SchedulerTimePoint<Sched>::Clock;

    struct ScheduleAtFunction {
        template<typename Scheduler>
        requires(concepts::TagInvocable<ScheduleAtFunction, Scheduler, SchedulerTimePoint<Scheduler>>)
        constexpr static auto operator()(Scheduler&& scheduler, SchedulerTimePoint<Scheduler> time_point)
            -> concepts::Sender auto {
            return tag_invoke(ScheduleAtFunction {}, di::forward<Scheduler>(scheduler), time_point);
        }
    };

    constexpr inline auto schedule_at = ScheduleAtFunction {};

    struct ScheduleAfterFunction {
        template<SchedulerWithNow Scheduler>
        requires(concepts::TagInvocable<ScheduleAfterFunction, Scheduler, SchedulerDuration<Scheduler>> ||
                 concepts::TagInvocable<ScheduleAtFunction, Scheduler, SchedulerTimePoint<Scheduler>>)
        constexpr static auto operator()(Scheduler&& scheduler, SchedulerDuration<Scheduler> duration)
            -> concepts::Sender auto {
            if constexpr (concepts::TagInvocable<ScheduleAfterFunction, Scheduler, SchedulerDuration<Scheduler>>) {
                return tag_invoke(ScheduleAfterFunction {}, di::forward<Scheduler>(scheduler), duration);
            } else {
                return schedule_at(di::forward<Scheduler>(scheduler), now(scheduler) + duration);
            }
        }
    };

    constexpr inline auto schedule_after = ScheduleAfterFunction {};

    template<typename Sched>
    concept TimedScheduler = SchedulerWithNow<Sched> && requires(Sched&& scheduler, SchedulerDuration<Sched> duration,
                                                                 SchedulerTimePoint<Sched> time_point) {
        schedule_after(di::forward<Sched>(scheduler), duration);
        schedule_at(di::forward<Sched>(scheduler), time_point);
    };
}

using timed_scheduler_ns::now;
using timed_scheduler_ns::schedule_after;
using timed_scheduler_ns::schedule_at;
using timed_scheduler_ns::SchedulerClock;
using timed_scheduler_ns::SchedulerDuration;
using timed_scheduler_ns::SchedulerTimePoint;
using timed_scheduler_ns::TimedScheduler;
}
