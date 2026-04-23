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

extern "C"
{
    /**
     * A handle to an audio decoder.
     * @note Not threadsafe.
     */
    struct AvCollection;
    typedef struct AvCollection Encoder;

    /**
     * Opens a file for encoding.
     * @param pURL any valid URL to an audio asset supported by FFmpeg.
     * @param pCallback the callback to use for error reporting.
     * @param inSampleRate the sample rate of the incoming audio.
     * @param inChannels the channel count of the incoming audio.
     * @param options a list of muxer and codec parameters in JSON format.
     */
    Encoder *casturria_newEncoder(const char *pURL, EventCallback pCallback, uint32_t inSampleRate, uint8_t inChannels, const char *pOptions);

    /**
     * Frees an encoder handle previously returned by casturria_newEncoder().
     * @param pEncoder the encoder handle to free.
     */
    void casturria_freeEncoder(Encoder *pEncoder);

    /**
     * Submits a block of audio data to the encoder.
     * @param pEncoder the encoder handle to submit to.
     * @param input a memory region containing the audio data being submitted.
     * @param count the number of frames being submitted.
     * @note One frame equates to one sample per channel.
     * @note In case of failure, this function will dispatch events to the EventHandler previously supplied to casturria_newEncoder().
     */
    void casturria_encode(Encoder *pEncoder, const float *pInput, size_t count);

    /**
     * Call this function after successful encoding but before casturria_freeEncoder().
     * @warning Failure to finalize an encoder results in a truncated file at best or an entirely unplayable one at worst.
     */
    void casturria_finalizeEncoder(Encoder *pEncoder);
}
