/**
* Casturria support layer
* Audio encoding and decoding utility include.
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
#include <stdlib.h>
#include <string>
#include <sstream>
#include <format>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
}

/**
 * A container for the many objects needed for FFmpeg encoding and decoding.
 * Aliased as Encoder and Decoder for the purposes of the public API.
 */

struct AvCollection
{
    EventCallback pCallback;         // The callback to send events to.
    AVFormatContext *pFormatContext; // Handles muxing/ demuxing.
    AVIOContext *pIOContext;         // I/O for encoding.
    AVCodecContext *pCodecContext;
    AVPacket *pPacket;
    AVFrame *pFrame;
    AVFilterGraph *pFilterGraph;
    AVFilterContext *pFilterGraphIn, *pFilterGraphOut; // abuffer and abuffersink. They belong to the filter graph.
    AVDictionary *pOptions;                            // Used during encoder setup.
    uint32_t inSampleRate;                             // Sample rate of the input.
    uint32_t outSampleRate;                            // Sample rate being encoded or decoded to.
    AVChannelLayout inChannelLayout;
    AVChannelLayout outChannelLayout;
    bool isEncoder;
    int stream;               // The audio stream chosen by av_find_best_stream(). Only used for decoding.
    size_t framesLeftInBatch; // Number of audio frames remaining in current FFmpeg output frame. Only used for decoding.
    uint64_t timestamp;       // Number of samples fed to the encoder so far. Only used for encoding.
};

/**
 * Internal: allocates a collection, as well as those members which are used for both encoding and decoding.
 * @param pCallback the callback to use for event handling.
 */
AvCollection *newAvCollection(EventCallback pCallback);

/**
 * Internal: gets a discription of a Libav* AVError as a C++ string.
 * @param err the Libav* error code to get a description of.
 */
std::string getAVErrorDescription(int err);

/**
 * Internal: frees an AvCollection (either an encoder or decoder).
 * @param pCollection the AvCollection to free.
 */
void freeAvCollection(AvCollection *pCollection);

/**
 * Internal: frees the given AvCollection and returns a null pointer.
 * Used to reduce verbosity of error handling in encoder/ decoder setup.
 * @param pCollection the AvCollection to free.
 */
AvCollection *fail(AvCollection *pCollection);

/**
 * Internal: get a channel layout description as a C++ string.
 * @param pLayout the channel layout to describe.
 */
std::string getChannelLayoutDescription(AVChannelLayout *pLayout);

/**
 * Overload that gets the default layout description for a given channel count.
 * @param channels the channel count to get a description of.
 */
std::string getChannelLayoutDescription(uint8_t channels);

/**
 * Looks up a filter and reports an error if not found.
 * @param pName the name of the filter to locate.
 * @param pCallback the EventCallback to use for error reporting.
 */
const AVFilter *findFilter(const char *pName, EventCallback pCallback);

/**
 * Internal: builds the system (non-configurable) filter graph for an encoder or decoder.
 * @param pCollection the AvCollection to process.
 */
bool buildSystemFilterGraph(AvCollection *pCollection);

/**
 * Dispatches the given event to the given EventCallback.
 * @param pCallback the callback to dispatch the event to.
 * @param event the type of event being dispatched.
 * @param message the message to send to the callback.
 */
void dispatchEvent(EventCallback pCallback, event_t type, const std::string &message);

/**
 * Same as above but for a constant cstring.
 * Caller retains ownership of string.
 */
void dispatchEvent(EventCallback pCallback, event_t type, const char *pMessage);

/**
 * Dispatches a message from av_strerror().
 * @param pCallback the callback to dispatch to.
 * @param type the type of event.
 * @param error the error code to translate and log.
 */
void dispatchEvent(EventCallback pCallback, event_t type, int error);

/**
 * Dispatches an "out of memory" event.
 * @param pCallback the event callback.
 * @param type the event type.
 */
void dispatchOutOfMemory(EventCallback pCallback, event_t type);
