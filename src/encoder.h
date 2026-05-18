/**
* Casturria support layer
* Audio encoding include
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
#include "events.h"
#include <stdint.h>
#include <stddef.h>
#include <emscripten.h>

extern "C"
{
    /**
     * A handle to an audio encoder.
     * @note Not threadsafe.
     */
    struct AvCollection;
    typedef struct AvCollection Encoder;

    /**
     * A callback for receiving data directly from FFmpeg.
     * This is the same callback signature as scene in avio.h.
     * The supplied callback is passed directly to FFmpeg's IO context.
     * The pOpaque parameter is unused; a Javascript closure should be used here.
     */
    typedef int (*WriteCallback)(void *pOpaque, const uint8_t *pBuf, int bufSize);

    /**
     * Opens a file for encoding.
     * @param pURL any valid URL to an audio asset supported by FFmpeg.
     * @param pMessageCallback the callback to use for logging and error reporting.
     * @param inSampleRate the sample rate of the incoming audio.
     * @param inChannels the channel count of the incoming audio.
     * @param options a list of muxer and codec private options in JSON format.
     * @param pWriteCallback an optional custom write callback.
     * @note the write callback can only be used with streaming formats that can be written in a single pass. In other words, there is no seek callback for now.
     */
    EMSCRIPTEN_KEEPALIVE
    Encoder *casturria_newEncoder(const char *pURL, EventCallback pEventCallback, uint32_t inSampleRate, uint8_t inChannels, const char *pOptions, WriteCallback pWriteCallback);

    /**
     * Frees an encoder handle previously returned by casturria_newEncoder().
     * @param pEncoder the encoder handle to free.
     */
    EMSCRIPTEN_KEEPALIVE
    void casturria_freeEncoder(Encoder *pEncoder);

    /**
     * Submits a block of audio data to the encoder.
     * @param pEncoder the encoder handle to submit to.
     * @param input a memory region containing the audio data being submitted.
     * @param count the number of frames being submitted.
     * @note One frame equates to one sample per channel.
     * @note In case of failure, this function will dispatch events to the EventHandler previously supplied to casturria_newEncoder().
     */
    EMSCRIPTEN_KEEPALIVE
    void casturria_encode(Encoder *pEncoder, const float *pInput, size_t count);

    /**
     * Call this function after successful encoding but before casturria_freeEncoder().
     * @warning Failure to finalize an encoder results in a truncated file at best or an entirely unplayable one at worst.
     */
    EMSCRIPTEN_KEEPALIVE
    void casturria_finalizeEncoder(Encoder *pEncoder);
}
