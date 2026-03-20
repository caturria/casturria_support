/**
* CVC support layer
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

#include "events.h"
#include <stdlib.h>
const int MAX_EVENT_DETAILS = 3; // Increase as needed.

struct EventDetails
{
    typedef union
    {
        const char *pStringDetail;
        int intDetail;

    } Detail;
    Detail details[MAX_EVENT_DETAILS];
};

/**
 * An event callback which receives an event and does nothing with it.
 * All event callbacks are initialized to this by default.
 */
static void noOpEventCallback(event_t event, EventDetails *pDetails)
{
}

struct EventHandler
{
    EventCallback events[EVENTTYPE_COUNT];
};
EventHandler *casturria_newEventHandler()
{
    auto handler = (EventHandler *)malloc(sizeof(EventHandler));
    if (handler == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < EVENTTYPE_COUNT; i++)
    {
        handler->events[i] = noOpEventCallback;
    }
    return handler;
}

void casturria_freeEventHandler(EventHandler *pHandler)
{
    free(pHandler);
}
void casturria_registerEventCallback(EventHandler *pHandler, event_t event, EventCallback callback)
{
    pHandler->events[event] = callback == nullptr ? noOpEventCallback : callback;
}

void casturria_dispatchEvent(EventHandler *pHandler, event_t event, EventDetails *pDetails)
{
    if (event < 0 || event >= EVENTTYPE_COUNT)
    {
        return;
    }
    pHandler->events[event](event, pDetails);
}