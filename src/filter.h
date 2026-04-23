/**
* Casturria support layer
* Audio filtering include
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
    struct FilterGraph;
    typedef struct FilterGraph FilterGraph;

    /**
     * Creates a new filter graph.
     * @param pDescription the FFmpeg filter graph description.
     * @param pCallback the callback to use for event handling.
     * @param inSampleRate the sample rate of the incoming audio.
     * @param inChannels the channel count of the incoming audio.
     * @param outSampleRate the desired output sample rate.
     * @param outChannels the desired output sample count.
     */
    FilterGraph *casturria_newFilterGraph(const char *pDescription, EventCallback pCallback, uint32_t inSampleRate, uint8_t inChannels, uint32_t outSampleRate, uint8_t outChannels);

    /**
     * Frees a filter graph.
     * @param pFilterGraph the filter graph to free.
     */
    void casturria_freeFilterGraph(FilterGraph *pFilterGraph);

    /**
     * Query the number of inputs held by the graph.
     * @param pFilterGraph the filter graph to query.
     */
    size_t casturria_getFilterGraphInputs(const FilterGraph *pFilterGraph);

    /**
     * Query the number of outputs held by a filter graph.
     * @param filterGraph the filter graph to query.
     */
    size_t casturria_getFilterGraphOutputs(const FilterGraph *pFilterGraph);

    /**
     * Send some audio to one of the graph's inputs.
     * @note input must be in interleaved floating point format.
     * @param pFilterGraph the filter graph to send input to.
     * @param pBuffer a memory region containing the audio to send.
     * @param count the number of samples sent per channel.
     * @param input the input to send to.
     * @note to obtain the final few samples once input ends, send a null pointer as pInput with a count of 0, then call casturria_receiveOutput until no more data comes out.
     */
    bool casturria_sendInput(FilterGraph *pFilterGraph, const float *pBuffer, size_t count, size_t input);

    /**
     * Obtain some audio from one of the graph's outputs.
     * @note if there is not enough available output to fulfill the request, this function will return zero samples instead of the requested amount.
     * @note the exception to the above is if the graph has been instructed to drain, in which case it will return up to the requested number of samples.
     * @note once draining begins, repeatedly call casturria_receiveOutput until no more samples come out.
     * @param pFilterGraph the filter graph to extract output from.
     * @param pBuffer a memory region to write the output to.
     * @param count the number of samples per channel to request.
     * @param output the output to extract samples from.
     * @returns number of samples per channel actually received.
     */
    size_t casturria_receiveOutput(FilterGraph *pFilterGraph, float *pBuffer, size_t count, size_t output);
}