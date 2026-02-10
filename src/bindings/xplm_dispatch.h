#pragma once

#include "../main_thread_dispatcher.h"

#include <utility>

template <typename Fn>
auto CallOnMainThread(Fn&& fn) -> decltype(fn())
{
    return MainThreadDispatcher::instance().CallSync(std::forward<Fn>(fn));
}
