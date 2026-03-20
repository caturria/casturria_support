/**
* CVC support layer
* Event system include
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

extern "C"
{

    struct EventHandler;
    typedef struct EventHandler EventHandler;

    /**
     * A type that holds the details of a particular event.
     * Individual details can be extracted using functions such as casturria_getStringDetail() and casturria_getIntDetail().
     * @note The number and type of details vary based on the type of event being triggered.
     * @warning Event callbacks do not inherit ownership of this and should not attempt to free it.
     */
    struct EventDetails;
    typedef struct EventDetails EventDetails;

    /**
     * List of event type identifiers.
     */
    enum event_t
    {
        EVENTTYPE_OPENED_INPUT = 0,
        EVENTTYPE_FAILED_TO_OPEN_INPUT,
        EVENTTYPE_COUNT,
    };

    /**
     * A callback which receives the details of an event.
     * @warning This callback does not inherit ownership of the provided EventDetails handle and must not attempt to free it.
     * @param eventType the type of the received event.
     * @param pDetails the details of the event. Contains information such as the name of the file that was opened.
     */
    typedef void (*EventCallback)(event_t event, EventDetails *pDetails);

    /**
     * Creates a new EventHandler.
     * Can be shared amongst many event-emitting objects.
     * @note All events are handled with a default no-op callback by default.
     * @note Use casturria_setEventCallback to define behaviour for a given event.
     * @warning You must call casturria_freeEventHandler() after all objects which borrow this EventHandler instance have been freed.
     * @returns EventHandler*.
     */
    EventHandler *casturria_newEventHandler();

    /**
     * Frees an EventHandler previously allocated with casturria_newEventHandler().
     * @warning All objects which borrow this EventHandler instance, such as encoders and decoders, must be freed before calling this.
     * @param pEventHandler the EventHandler instance to free.
     */
    void casturria_freeEventHandler(EventHandler *pEventHandler);

    /**
     * Registers the given callback to be called when the given event is dispatched.
     * @param pHandler the EventHandler on which to register the callback.
     * @param event the event to register the callback to.
     * @param callback the callback to register.
     */
    void casturria_registerEventCallback(EventHandler *pHandler, event_t event, EventCallback callback);

    /**
     * Dispatches the given event to a callback registered on the given EventHandler.
     * @param pHandler the EventHandler to dispatch the event to.
     * @param event the type of event being dispatched.
     * @param pDetails the specific details for this event.
     * @note out of range values for event result in a no-op.
     */
    void casturria_dispatchEvent(EventHandler *pHandler, event_t event, EventDetails *pDetails);
}