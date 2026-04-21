/**
* Casturria support layer
* Event system internal include
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
#pragma once

#include "../events.h"

/**
 * EventDetails structure.
 * This is not a public API, but anything internal to casturria_support may use it to generate events for public consumption.
 * This is typically allocated on the stack because it does not need to live longer than a call to casturria_dispatchEvent().
 */
const int MAX_EVENT_DETAILS = 3; // Increase as needed.

struct EventDetails
{
    EventCallback pCallback;
    typedef union
    {
        const char *pStringDetail;
        int32_t intDetail;
        void *pArbitraryDetail;

    } Detail;
    Detail details[MAX_EVENT_DETAILS];
};
namespace Event
{

    /**
     * Dispatches the given event to a callback registered on the given EventHandler.
     * @param pHandler the EventHandler to dispatch the event to.
     * @param event the type of event being dispatched.
     * @param pDetails the specific details for this event.
     * @note out of range values for event result in a no-op.
     */
    void dispatch(EventHandler *pHandler, event_t event, EventDetails *pDetails);
}
