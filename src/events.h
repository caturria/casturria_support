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
         * Details:
         * Decoder: arbitrary = the decoder that's being opened.
         */
        EVENTTYPE_DECODE_OPENED_INPUT = 0,

        /**
         * Failed to open a URL for decoding.
         * This may follow an EVENTTYPE_OPENED_INPUT event if the URL turned out not to contain a decodable bitstream.
         * Details:
         * Decoder: arbitrary = the decoder that was unable to be opened.
         * Error: string = the error returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_DECODE_FAILED_TO_OPEN_INPUT,

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
         * Successfully allocated a codec for decoding.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Codec: string = the name of the codec being initialized.
         */
        EVENTTYPE_DECODE_ALLOCATED_CODEC,

        /**
         * Successfully configured a codec for decoding.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Codec: string = the name of the codec being initialized.
         */
        EVENTTYPE_DECODE_CONFIGURED_CODEC,

        /**
         * Failed to configure the required codec for the given URL.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Error: string = the string returned from av_strerror().
         */
        EVENTTYPE_DECODE_FAILED_TO_CONFIGURE_CODEC,

        /**
         * Successfully initialized a codec for decoding.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Codec: string = the name of the codec being initialized.
         */
        EVENTTYPE_DECODE_INITIALIZED_CODEC,

        /**
         * Failed to initialize the required codec for the given URL.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         * Error: string = the string returned from av_strerror().
         */
        EVENTTYPE_DECODE_FAILED_TO_INITIALIZE_CODEC,

        /**
         * Successfully allocated the system (decoder-specific, non-configurable) filter graph.
         * Details:
         * Decoder: arbitrary = the decoder being initialized.
         */
        EVENTTYPE_ALLOCATED_FILTER_GRAPH,

        /**
         * Successfully parsed the system filter graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Filters: string = the filter graph that was parsed.
         */
        EVENTTYPE_PARSED_SYSTEM_FILTER_GRAPH,

        /**
         * Failed to parse the system filter graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned from FFmpeg's av_strerror API.
         */
        EVENTTYPE_FAILED_TO_PARSE_SYSTEM_FILTER_GRAPH,

        /**
         * The system filter graph produced an unexpected number of inputs and/or outputs.
         * Details:
         * Collection: string = the AvCollection being configured.
         * Filters: string = the filter graph description.
         */
        EVENTTYPE_INCORRECT_SYSTEM_FILTER_CONFIG,

        /**
         * A necessary filter was not found.
         * Details:
         * Filter: string = the name of the missing filter.
         */
        EVENTTYPE_LACKING_REQUIRED_FILTER,

        /**
         * Successfully created the system filter graph's input buffer.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_INITIALIZED_SYSTEM_ABUFFER,

        /**
         * Failed to create the system filter graph's input buffer.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned from FFmpeg's av_strerror api.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_SYSTEM_ABUFFER,

        /**
         * Successfully linked the system filter graph's input buffer to the graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_LINKED_SYSTEM_ABUFFER,

        /**
         * Failed to link the system filter graph's input buffer to the graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_FAILED_TO_LINK_SYSTEM_ABUFFER,

        /**
         * Successfully created the system filter graph's output buffer.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_INITIALIZED_SYSTEM_ABUFFERSINK,

        /**
         * Failed to create the system filter graph's output buffer.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned from FFmpeg's av_strerror api.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_SYSTEM_ABUFFERSINK,

        /**
         * Successfully linked the system filter graph's output buffer to the graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_LINKED_SYSTEM_ABUFFERSINK,

        /**
         * Failed to link the system filter graph's output buffer to the graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_FAILED_TO_LINK_SYSTEM_ABUFFERSINK,

        /**
         * Successfully configured the system filter graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_CONFIGURED_SYSTEM_FILTER_GRAPH,

        /**
         * Failed to configure the system filter graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         */
        EVENTTYPE_FAILED_TO_CONFIGURE_SYSTEM_FILTER_GRAPH,

        /**
         * Out of memory.
         * Details: none
         */
        EVENTTYPE_OUT_OF_MEMORY,

        /**
         * An unexpected problem occurred that shouldn't be possible.
         * Details:
         * Collection: arbitrary = the AvCollection being initialized.
         */
        EVENTTYPE_BUG,

        /**
         * An error occurred while demuxing.
         * Typical reasons include I/O errors and corrupted streams.
         * Details:
         * Decoder: arbitrary = the decoder being decoded from.
         * Details: string = the error returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_DEMUX_ERROR,

        /**
         * An error occurred while decoding.
         * Likely indicates a corrupted bitstream, otherwise a software bug.
         * Details:
         * Decoder: arbitrary = the decoder being decoded from.
         * Details: string = the error returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_DECODE_ERROR,

        /**
         * An error occurred while filtering input or output through the system filter graph.
         * Shouldn't happen short of a software bug as the system filter graph is non-configurable.
         * Details:
         * Collection: arbitrary = the AvCollection being encoded to or decoded from.
         * Details: string = the error returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_SYSTEM_FILTER_ERROR,

        /**
         * Demuxing complete.
         * Details:
         * Decoder: arbitrary = the decoder being decoded from.
         */
        EVENTTYPE_DEMUX_COMPLETE,

        /**
         * Decoding complete.
         * Details:
         * Decoder: arbitrary = the decoder being decoded from.
         */
        EVENTTYPE_DECODE_COMPLETE,

        /**
         * Successfully allocated output format context.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_ALLOCATED_FORMAT_CONTEXT,

        /**
         * Successfully found the JSON object. May still fail to parse it.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_FOUND_JSON_OBJECT,

        /**
         * An error occurred while parsing JSON.
         * Details:
         * Encoder: arbitrary = the encoder on which the problem occurred.
         * Details: string = the error details returned by the JSON parser.
         */
        EVENTTYPE_PARSE_ERROR,

        /**
         * An invalid option value was submitted.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Key: string = the JSON key.
         * Value: string = the invalid value.
         */
        EVENTTYPE_ENCODE_INVALID_ARGUMENT,

        /**
         * JSON was successfully parsed.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_PARSED_JSON,

        /**
         * An error occurred during encoder setup or encoding.
         * Details:
         * Encoder: arbitrary = the encoder being worked on.
         * Details: string = the string returned from FFmpeg's av_strerror API.
         */
        EVENTTYPE_ENCODE_ERROR,

        /**
         * Successfully configured an output format for the given URL.
         * Details:
         * Encoder: arbitrary = the encoder being set up.
         * Format: string = the chosen format.
         */
        EVENTTYPE_ENCODE_SELECTED_FORMAT,

        /**
         * Failed to select an output format for the given URL.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_LACKING_FORMAT,

        /**
         * Successfully selected an output codec for the given URL.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Codec: string = the chosen codec.
         */
        EVENTTYPE_ENCODE_SELECTED_CODEC,

        /**
         * Failed to select a valid encoder for the given URL.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_LACKING_CODEC,

        /**
         * Successfully allocated a codec context for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_ALLOCATED_CODEC_CONTEXT,

        /**
         * Successfully negotiated a suitable output sample format with the encoding codec.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Format: string = the chosen sample format.
         */
        EVENTTYPE_ENCODE_NEGOTIATED_SAMPLE_FORMAT,

        /**
         * Failed to negotiate a suitable output sample format with the encoding codec.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_FAILED_TO_NEGOTIATE_SAMPLE_FORMAT,

        /**
         * Successfully negotiated a suitable output sample rate with the encoding codec.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Rate: int = the chosen sample rate.
         */
        EVENTTYPE_ENCODE_NEGOTIATED_SAMPLE_RATE,

        /**
         * Failed to negotiate a suitable output sample rate with the encoding codec.
         * Details:
         * Encoder: arbitrary = the encoder being worked on.
         */
        EVENTTYPE_ENCODE_FAILED_TO_NEGOTIATE_SAMPLE_RATE,

        /**
         * Successfully initialized the codec for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Codec: string = the codec that was initialized.
         */
        EVENTTYPE_ENCODE_INITIALIZED_CODEC,

        /**
         * Failed to initialize the codec for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Codec: string = the codec that failed to be initialized.
         */
        EVENTTYPE_ENCODE_FAILED_TO_INITIALIZE_CODEC,

        /**
         * Successfully allocated the stream.
         * Details:
         * Encoder: arbitrary = the encoder being set up.
         */
        EVENTTYPE_ENCODE_ALLOCATED_STREAM,

        /**
         * Successfully configured a stream for a given encoder.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Codec: string = the codec being used.
         * Stream: int = the stream index.
         */
        EVENTTYPE_ENCODE_CONFIGURED_STREAM,

        /**
         * Failed to configure a stream for the given encoder.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         * Codec: string = the codec being used.
         */
        EVENTTYPE_ENCODE_FAILED_TO_CONFIGURE_STREAM,

        /**
         * Successfully opened the given URL for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_OPENED_URL,

        /**
         * Failed to open the given URL for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_FAILED_TO_OPEN_URL,

        /**
         * Successfully wrote the header for a given output file.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_WROTE_HEADER,

        /**
         * Failed to write the header for a given output file.
         * Details:
         * Encoder: arbitrary = the encoder being configured.
         */
        EVENTTYPE_ENCODE_FAILED_TO_WRITE_HEADER,

        /**
         * Failed to allocate a frame buffer for encoding.
         * Details:
         * Encoder: arbitrary = the encoder being used.
         * Details: string = the error message returned via FFmpeg's av_strerror api.
         */
        EVENTTYPE_ENCODE_FAILED_TO_ALLOCATE_FRAME_BUFFER,

        /**
         * An error occurred while writing an encoded packet.
         * This is the key event to watch for especially when writing to protocols like Icecast.
         * Details:
         * Encoder: arbitrary = the encoder being encoded to.
         * Details: string = the error message returned via FFmpeg's av_strerror API.
         */
        EVENTTYPE_OUTPUT_ERROR,

        /**
         * Successfully parsed a standalone (application-defined) filter graph.
         * Details:
         * Graph: arbitrary = the FilterGraph being configured.
         * Filters: string = the filter graph that was parsed.
         */
        EVENTTYPE_PARSED_APPLICATION_FILTER_GRAPH,

        /**
         * Failed to parse a standalone (application-defined) filter graph.
         * Details:
         * Collection: arbitrary = the AvCollection being configured.
         * Details: string = the error message returned from FFmpeg's av_strerror API.
         */
        EVENTTYPE_FAILED_TO_PARSE_APPLICATION_FILTER_GRAPH,

        /**
         * Successfully initialized an input buffer for a standalone(application-configured) filter graph.
         * @note One of these will fire per loose graph input.
         * Details:
         * Filter: arbitrary = the filter graph being configured.
         * Name: string = the name of the input whose buffer was initialized.
         * Pad: int = the input pad number it's being connected to.
         */
        EVENTTYPE_INITIALIZED_APPLICATION_ABUFFER,

        /**
         * Failed to initialize an input buffer for a standalone (application-configured) filter graph.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the string returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_APPLICATION_ABUFFER,

        /**
         * Successfully linked an input buffer to a standalone (application-created) filter.
         * @note One of these will fire per loose filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Name: string = the filter name.
         * Pad: int = the input pad.
         */
        EVENTTYPE_LINKED_APPLICATION_ABUFFER,

        /**
         * Failed to link an input buffer to a standalone (application-created) filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_LINK_APPLICATION_ABUFFER,

        /**
         * Successfully initialized a format converter for a standalone (application-created) filter.
         * @note One of these will fire per loose filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Name: string = the filter name.
         * Pad: int = the input pad.
         */
        EVENTTYPE_INITIALIZED_APPLICATION_AFORMAT,

        /**
         * Failed to initialize a format converter for a standalone (application-created) filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_APPLICATION_AFORMAT,

        /**
         * Successfully linked a format converter to a standalone (application-created) filter.
         * @note One of these will fire per loose filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Name: string = the filter name.
         * Pad: int = the input pad.
         */
        EVENTTYPE_LINKED_APPLICATION_AFORMAT,

        /**
         * Failed to link a format converter to a standalone (application-created) filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_LINK_APPLICATION_AFORMAT,

        /**
         * Successfully initialized an output buffer for a standalone (application-created) filter.
         * @note One of these will fire per loose filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Name: string = the filter name.
         * Pad: int = the input pad.
         */
        EVENTTYPE_INITIALIZED_APPLICATION_ABUFFERSINK,

        /**
         * Failed to initialize an output buffer for a standalone (application-created) filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_INITIALIZE_APPLICATION_ABUFFERSINK,

        /**
         * Successfully linked an output buffer to a standalone (application-created) filter.
         * @note One of these will fire per loose filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Name: string = the filter name.
         * Pad: int = the input pad.
         */
        EVENTTYPE_LINKED_APPLICATION_ABUFFERSINK,

        /**
         * Failed to link an output buffer to a standalone (application-created) filter.
         * Details:
         * Graph: arbitrary = the filter graph being configured.
         * Details: string = the error message returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_FAILED_TO_LINK_APPLICATION_ABUFFERSINK,

        /**
         * Successfully configured a standalone (application-created) filter graph.
         * Details:
         * Graph: arbitrary = the filter graph that was configured.
         */
        EVENTTYPE_CONFIGURED_APPLICATION_FILTER_GRAPH,

        /**
         * Failed to configure an application-created filter graph.
         * Details:
         * Graph: arbitrary = the filter graph that failed to be configured.
         */
        EVENTTYPE_FAILED_TO_CONFIGURE_APPLICATION_FILTER_GRAPH,

        /**
         * Failed to allocate frame buffers for an application-created filter graph.
         * Details:
         * Graph: arbitrary = the filter graph being used.
         */
        EVENTTYPE_FILTER_FAILED_TO_ALLOCATE_FRAME_BUFFER,

        /**
         * An error occurred while filtering input or output through an application-created filter graph.
         * Details:
         * Graph: arbitrary = the filter graph being used.
         * Details: string = the string returned via FFmpeg's av_strerror() API.
         */
        EVENTTYPE_APPLICATION_FILTER_ERROR,

        /**
         * Reached EOF on an application-created filter graph.
         * Details:
         * Graph: arbitrary = the filter graph being used.
         */
        EVENTTYPE_FILTER_GRAPH_EOF,

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