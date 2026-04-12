/**
* CVC support layer
* Audio decoding include
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
    struct Decoder;
    typedef struct Decoder Decoder;

    /**
     * Opens a file for decoding.
     * @param pURL any valid URL to an audio asset supported by FFmpeg.
     * @param pEventHandler a previously configured EventHandler instance.
     * @param sampleRate the desired output sample rate.
     * @param channels the desired output channels.
     * @note Several events will be dispatched to the provided EventHandler during the course of decoder initialization.
     * @returns Decoder* on success or nullptr on failure.
     */
    Decoder *casturria_newDecoder(const char *pURL, EventHandler *pEventHandler, uint32_t sampleRate, uint8_t channels);

    /**
     * Frees a Decoder handle previously returned by casturria_newDecoder().
     * @param pDecoder the decoder handle to free.
     */
    void casturria_freeDecoder(Decoder *pDecoder);

    /**
     * Requests a batch of audio frames from the decoder.
     * @param pDecoder the decoder handle to decode frames from.
     * @param pOutput a pointer to a meory region that will receive the output.
     * @param count the number of frames to decode.
     * @returns number of frames actually decoded.
     * @note One frame equates to one sample per decoded channel, so 1024 frames would yield 2048 samples if the output is stereo.
     * @note This function may dispatch events to the EventHandler previously supplied to casturria_NewDecoder().
     */
    size_t casturria_decode(Decoder *pDecoder, float *pOutput, size_t count);
}
