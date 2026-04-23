/**
* Casturria support layer
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
#include <stdint.h>

extern "C"
{

    /**
     * List of event type identifiers.
     */
    enum event_t
    {

        /**
         * A milestone was reached during setup.
         */
        EVENTTYPE_SETUP_MILESTONE,

        /**
         * A failure occurred during setup.
         */
        EVENTTYPE_SETUP_FAILURE,

        /**
         * Demuxing of an audio asset is complete.
         */
        EVENTTYPE_DEMUX_COMPLETE,

        /**
         * Decoding of an audio asset is complete.
         */
        EVENTTYPE_DECODE_COMPLETE,

        /**
         * A filter graph has reached the end of its input.
         */
        EVENTTYPE_FILTER_COMPLETE,

        /**
         * A failure occurred during operation.
         */
        EVENTTYPE_OPERATION_FAILURE,

        /**
         * An error occurred while writing packets to an output file.
         * This is the key event to watch for while streaming to a network-based protocol such as Icecast.
         */
        EVENTTYPE_OUTPUT_ERROR,

    };

    /**
     * A callback which receives the details of an event.
     * @warning This callback does not inherit ownership of the message and must not attempt to free it.
     * @param eventType the type of the received event.
     * @param pMessage the message to be logged. Contains information such as the name of the file that was opened.
     */
    typedef void (*EventCallback)(uint32_t event, const char *pMessage);
}