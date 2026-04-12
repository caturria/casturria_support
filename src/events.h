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
#include <stdint.h>

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

        /**
         * Successfully opened a URL for decoding.
         * Initialization may still fail if the file does not contain a decodable bitstream.
         * details:
         * decoder: arbitrary = the decoder that's being opened.
         */
        EVENTTYPE_OPENED_INPUT = 0,

        /**
         * Failed to open a URL for decoding.
         * This may follow an EVENTTYPE_OPENED_INPUT event if the URL turned out not to contain a decodable bitstream.
         * Details:
         * decoder: arbitrary = the decoder that was unable to be opened.
         * error: string = the error returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_FAILED_TO_OPEN_INPUT,

        /**
         * Found list of streams for the given file.
         * Details:
         * Decoder: arbitrary = the decoder being opened.
         * Streams: int = the number of streams in the file.
         */
        EVENTTYPE_FOUND_STREAMS,

        /**
         * Failed to find any valid streams in the file.
         * Details:
         * Decoder: arbitrary = the decoder being opened.
         */
        EVENTTYPE_FAILED_TO_FIND_STREAMS,

        /**
         * Successfully identified a decodable audio stream.
         * Details:
         * Decoder: arbitrary = the decoder being opened.
         * Stream: int = the stream index that has been selected.
         * Codec: string = the full name of the audio codec being used.
         */
        EVENTTYPE_FOUND_BEST_STREAM,

        /**
         * Failed to find a decodable audio stream.
         * Details:
         * Decoder: arbitrary = the decoder being opened.
         */
        EVENTTYPE_FAILED_TO_FIND_BEST_STREAM,

        /**
         * Successfully initialized a codec for decoding.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Codec: string = the name of the codec being initialized.
         */
        EVENTTYPE_INITIALIZED_CODEC_DECODING,

        /**
         * Failed to initialize the required codec for the given URL.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Error: string = the string returned from av_strerror().
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_CODEC_DECODING,

        /**
         * Successfully initialized the system (decoder-specific, non-configurable) filter graph.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         */
        EVENTTYPE_INITIALIZED_FILTER_DECODING,

        /**
         * Failed to initialize the system (decoder-specific, non-configurable) filter graph.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_FILTER_DECODING,

        /**
         * Out of memory.
         * details: none
         */
        EVENTTYPE_OUT_OF_MEMORY,

        /**
         * An unexpected problem occurred during decoder setup or decoding.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         */
        EVENTTYPE_BUG,

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
     * Gets an event detail as a string.
     * @param pDetails the EventDetails handle from which to fetch a detail.
     * @param detail the index of the detail to retrieve.
     * @warning Accessing an invalid detail (either the wrong type or out of range for the current event) is undefined behaviour.
     * @note See the documentation for event_t to learn which details are carried by which events.
     */
    const char *casturria_getStringDetail(EventDetails *pDetails, uint8_t detail);

    /**
     * Gets an event detail as an integer.
     * @param pDetails the EventDetails handle from which to fetch a detail.
     * @param detail the index of the detail to retrieve.
     * @warning Accessing an invalid detail (either the wrong type or out of range for the current event) is undefined behaviour.
     * @note See the documentation for event_t to learn which details are carried by which events.
     */
    int32_t casturria_getIntDetail(EventDetails *pDetails, uint8_t detail);
}