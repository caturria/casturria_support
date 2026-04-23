/**
* Casturria support layer
* Audio filtering implementation
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
#include "filter.h"
#include "internal/util.h"

struct FilterGraph
{
    EventCallback pCallback; // Must always be the second member.
    AVFilterGraph *pGraph;
    AVFilterContext **ppInputs;
    size_t inputCount;
    uint64_t *pTimestamps; // one timestamp per input. Required by filters such as afade.
    AVFilterContext **ppOutputs;
    size_t outputCount;
    AVFrame *pFrame; // shuttles frames back and forth between the filter graph and the application.
    uint32_t inSampleRate;
    AVChannelLayout inChannelLayout;
    uint32_t outSampleRate;
    AVChannelLayout outChannelLayout;
};

/**
 * Internal: frees a filter graph and returns a null pointer.
 * Used to reduce verbosity of error handling during setup.
 * @param pFilterGraph the filter graph to free.
 */
FilterGraph *fail(FilterGraph *pFilterGraph)
{
    casturria_freeFilterGraph(pFilterGraph);
    return nullptr;
}

/**
 * Internal: counts unlinked input or output pads in a given filter graph.
 * @param pItems the list of inputs or outputs to count.
 */
static size_t countInputsOrOutputs(AVFilterInOut *pItems)
{
    size_t count = 0;
    while (pItems != nullptr)
    {
        count++;
        pItems = pItems->next;
    }

    return count;
}

/**
 * Internal: sets up I/O buffers for all unlinked inputs.
 * @param pFilterGraph the filter graph to link up.
 * @param pInputs the list of unlinked inputs.
 */
static bool linkInputs(FilterGraph *pFilterGraph, AVFilterInOut *pInputs)
{
    auto pCallback = pFilterGraph->pCallback;
    pFilterGraph->inputCount = countInputsOrOutputs(pInputs);
    if (pFilterGraph->inputCount == 0)
    {
        return true; // Nothing to do here.
    }

    pFilterGraph->ppInputs = (AVFilterContext **)malloc(sizeof(AVFilterContext *) * pFilterGraph->inputCount);
    if (pFilterGraph->ppInputs == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return false;
    }

    pFilterGraph->pTimestamps = (uint64_t *)malloc(sizeof(uint64_t) * pFilterGraph->inputCount);
    if (pFilterGraph->pTimestamps == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_OPERATION_FAILURE);
        return false;
    }

    memset(pFilterGraph->pTimestamps, 0, sizeof(uint64_t) * pFilterGraph->inputCount);

    auto nextIndex = 0; // where to store the next AVFilterContext.

    auto pFilter = findFilter("abuffer", pFilterGraph->pCallback);
    if (pFilter == nullptr)
    {
        return false;
    }

    // Abuffer arguments: one size should fit all here.
    auto bufferArgs = std::format("sample_rate={}:sample_fmt=flt:channel_layout={}",
                                  pFilterGraph->inSampleRate,
                                  getChannelLayoutDescription(&pFilterGraph->inChannelLayout));

    while (pInputs != nullptr)
    {
        AVFilterContext *pBuffer;
        int result = avfilter_graph_create_filter(&pBuffer, pFilter, nullptr, bufferArgs.c_str(), nullptr, pFilterGraph->pGraph);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        result = avfilter_link(pBuffer, 0, pInputs->filter_ctx, pInputs->pad_idx);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        pFilterGraph->ppInputs[nextIndex] = pBuffer;
        nextIndex++;
        pInputs = pInputs->next;
    }

    return true;
}

/**
 * Internal: sets up I/O buffers for all unlinked outputs.
 * @param pFilterGraph the filter graph to link up.
 * @param pOutputs the list of unlinked outputs.
 */
static bool linkOutputs(FilterGraph *pFilterGraph, AVFilterInOut *pOutputs)
{
    auto pCallback = pFilterGraph->pCallback;

    pFilterGraph->outputCount = countInputsOrOutputs(pOutputs);
    if (pFilterGraph->outputCount == 0)
    {
        return true; // Nothing to do here.
    }

    pFilterGraph->ppOutputs = (AVFilterContext **)malloc(sizeof(AVFilterContext *) * pFilterGraph->outputCount);
    if (pFilterGraph->ppOutputs == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return false;
    }

    auto nextIndex = 0; // where to store the next AVFilterContext.
    // Man, aformat is dumb! Don't change the order of these arguments or they will be silently ignored!
    std::string formatArgs = std::format("sample_rates={}:sample_fmts=flt:channel_layouts={}",
                                         pFilterGraph->outSampleRate,
                                         getChannelLayoutDescription(&pFilterGraph->outChannelLayout));

    auto pFormatFilter = findFilter("aformat", pFilterGraph->pCallback);
    if (pFormatFilter == nullptr)
    {
        return false;
    }

    auto pBufferFilter = findFilter("abuffersink", pFilterGraph->pCallback);
    if (pBufferFilter == nullptr)
    {
        return false;
    }

    while (pOutputs != nullptr)
    {
        AVFilterContext *pFormat;
        // We need to attach an aformat filter before the abuffersink filter to make sure the graph outputs the correct sample format.
        int result = avfilter_graph_create_filter(&pFormat, pFormatFilter, nullptr, formatArgs.c_str(), nullptr, pFilterGraph->pGraph);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        result = avfilter_link(pOutputs->filter_ctx, pOutputs->pad_idx, pFormat, 0);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        // The abuffersink filter has no constraints because the graph is free to configure sample rates and/or channel layouts as it sees fit.
        AVFilterContext *pBuffer;
        result = avfilter_graph_create_filter(&pBuffer, pBufferFilter, nullptr, nullptr, nullptr, pFilterGraph->pGraph);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        result = avfilter_link(pFormat, 0, pBuffer, 0);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
            return false;
        }

        pFilterGraph->ppOutputs[nextIndex++] = pBuffer;
        pOutputs = pOutputs->next;
    }

    return true;
}

/**
 * Internal: sets up I/O buffers for all unlinked inputs and outputs.
 * @param pFilterGraph the filter graph to link up.
 * @param pInputs the list of unlinked inputs.
 * @pOutputs the list of unlinked outputs.
 * @note this will free both lists regardless of success or failure.
 */
static bool linkInputsAndOutputs(FilterGraph *pFilterGraph, AVFilterInOut *pInputs, AVFilterInOut *pOutputs)
{
    bool result = linkInputs(pFilterGraph, pInputs);
    avfilter_inout_free(&pInputs);
    if (result)
    {
        result = linkOutputs(pFilterGraph, pOutputs);
    }
    avfilter_inout_free(&pOutputs);
    return result;
}

FilterGraph *casturria_newFilterGraph(const char *pDescription, EventCallback pCallback, uint32_t inSampleRate, uint8_t inChannels, uint32_t outSampleRate, uint8_t outChannels)
{
    auto pFilterGraph = (FilterGraph *)malloc(sizeof(FilterGraph));
    if (pFilterGraph == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return nullptr;
    }

    memset(pFilterGraph, 0, sizeof(FilterGraph));

    pFilterGraph->pCallback = pCallback;

    pFilterGraph->pFrame = av_frame_alloc();
    if (pFilterGraph->pFrame == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pFilterGraph);
    }

    pFilterGraph->pGraph = avfilter_graph_alloc();
    if (pFilterGraph->pGraph == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pFilterGraph);
    }
    auto pGraph = pFilterGraph->pGraph;

    pFilterGraph->inSampleRate = inSampleRate;
    av_channel_layout_default(&pFilterGraph->inChannelLayout, inChannels);
    pFilterGraph->outSampleRate = outSampleRate;
    av_channel_layout_default(&pFilterGraph->outChannelLayout, outChannels);

    AVFilterInOut *pInputs, *pOutputs;
    int result = avfilter_graph_parse2(pGraph, pDescription, &pInputs, &pOutputs);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pFilterGraph);
    }

    if (!linkInputsAndOutputs(pFilterGraph, pInputs, pOutputs))
    {
        // Events and I/O list cleanup already handled.
        return fail(pFilterGraph);
    }

    result = avfilter_graph_config(pFilterGraph->pGraph, nullptr);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pFilterGraph);
    }

    return pFilterGraph;
}

void casturria_freeFilterGraph(FilterGraph *pFilterGraph)
{
    avfilter_graph_free(&pFilterGraph->pGraph);
    av_frame_free(&pFilterGraph->pFrame);
    if (pFilterGraph->pTimestamps != nullptr)
    {
        free(pFilterGraph->pTimestamps);
    }

    if (pFilterGraph->ppInputs != nullptr)
    {
        free(pFilterGraph->ppInputs);
    }

    if (pFilterGraph->ppOutputs != nullptr)
    {
        free(pFilterGraph->ppOutputs);
    }

    free(pFilterGraph);
}

size_t casturria_getFilterGraphInputs(const FilterGraph *pFilterGraph)
{
    return pFilterGraph->inputCount;
}

size_t casturria_getFilterGraphOutputs(const FilterGraph *pFilterGraph)
{
    return pFilterGraph->outputCount;
}

bool casturria_sendInput(FilterGraph *pFilterGraph, const float *pInput, size_t count, size_t input)
{
    auto pCallback = pFilterGraph->pCallback;
    if (input >= pFilterGraph->inputCount)
    {
        return false;
    }
    if (count == 0)
    {
        // Put the input into draining mode.
        int result = av_buffersrc_add_frame(pFilterGraph->ppInputs[input], nullptr);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
            return false;
        }
        return true;
    }

    auto pFrame = pFilterGraph->pFrame;
    pFrame->format = AV_SAMPLE_FMT_FLT;
    pFrame->ch_layout = pFilterGraph->inChannelLayout;
    pFrame->sample_rate = pFilterGraph->inSampleRate;
    pFrame->nb_samples = count;
    pFrame->pts = pFilterGraph->pTimestamps[input];
    pFilterGraph->pTimestamps[input] += pFrame->nb_samples;
    int result = av_frame_get_buffer(pFrame, 0);
    if (result == 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
    }

    memcpy(pFrame->data[0], pInput, count * 4 * pFrame->ch_layout.nb_channels);
    result = av_buffersrc_add_frame(pFilterGraph->ppInputs[input], pFrame);
    av_frame_unref(pFrame);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        return false;
    }

    return true;
}

size_t casturria_receiveOutput(FilterGraph *pFilterGraph, float *pOutput, size_t count, size_t output)
{
    auto pCallback = pFilterGraph->pCallback;
    if (output >= pFilterGraph->outputCount)
    {
        return 0;
    }

    auto pFrame = pFilterGraph->pFrame;
    int result = av_buffersink_get_samples(pFilterGraph->ppOutputs[output], pFrame, count);
    if (result == AVERROR(EAGAIN))
    {
        return 0;
    }

    if (result == AVERROR_EOF)
    {
        dispatchEvent(pCallback, EVENTTYPE_FILTER_COMPLETE, "Filtering complete!");
        return 0;
    }
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
    }

    memcpy(pOutput, pFrame->data[0], pFrame->nb_samples * 4 * pFrame->ch_layout.nb_channels);
    size_t samplesOut = pFrame->nb_samples;
    av_frame_unref(pFrame);
    return samplesOut;
}