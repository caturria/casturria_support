/**
* Casturria support layer
* Event system implementation
* Copyright (C) 2026  Jordan Verner and contributors

* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Affero General Public License for more details.

* You should have received a copy of the GNU Affero General Public License
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "internal/events.h"
#include <stdlib.h>
#include <bitset>
#include <stdexcept>

struct EventHandler
{
    std::bitset<EVENTTYPE_COUNT> events;
};

EventHandler *casturria_newEventHandler()
{
    try
    {
        auto handler = new EventHandler();
        if (handler == nullptr)
        {
            return nullptr;
        }
        return handler;
    }
    catch (std::exception &e)
    {
        // No way of reporting this.
        return nullptr;
    }
}

void casturria_freeEventHandler(EventHandler *pHandler)
{
    delete pHandler;
}

void casturria_subscribeToEvent(EventHandler *pHandler, uint32_t event, bool subscribed)
{
    if (event >= EVENTTYPE_COUNT)
    {
        return;
    }
    pHandler->events[event] = subscribed;
}

const char *casturria_getStringDetail(EventDetails *pDetails, uint8_t detail)
{
    return pDetails->details[detail].pStringDetail;
}

int32_t casturria_getIntDetail(EventDetails *pDetails, uint8_t detail)
{
    return pDetails->details[detail].intDetail;
}

namespace Event
{
    void dispatch(EventHandler *pHandler, event_t event, EventDetails *pDetails)
    {
        if (event < 0 || event >= EVENTTYPE_COUNT)
        {
            return;
        }
        if (pHandler->events[event])
        {
            pDetails->pCallback(event, pDetails);
        }
    }
}
