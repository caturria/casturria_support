/**
* Casturria support layer
* Audio encoding and decoding utility implementation.
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

#include "internal/util.h"
/**
 * A common representation of an FFmpeg error event.
 */
struct AvErrorEvent
{
    EventDetails details;
    std::string description;
};

AvCollection *newAvCollection(EventHandler *pEventHandler, EventCallback pCallback)
{
    EventDetails details;
    details.pCallback = pCallback;

    auto pCollection = (AvCollection *)malloc(sizeof(AvCollection));
    if (pCollection == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, &details);
        return nullptr;
    }
    memset(pCollection, 0, sizeof(AvCollection));

    details.details[0].pArbitraryDetail = pCollection;

    pCollection->pEventHandler = pEventHandler;
    pCollection->pCallback = pCallback;

    pCollection->pPacket = av_packet_alloc();
    if (pCollection->pPacket == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, &details);
        return fail(pCollection);
    }

    pCollection->pFrame = av_frame_alloc();
    if (pCollection->pFrame == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, &details);
        return fail(pCollection);
    }

    return pCollection;
}

void handleAvError(void *pArbitrary, event_t event, int err)
{
    auto pEmitter = (EventEmitter *)pArbitrary;
    auto pEventHandler = pEmitter->pEventHandler;
    AvErrorEvent e;
    e.description.resize(AV_ERROR_MAX_STRING_SIZE);
    av_strerror(err, e.description.data(), AV_ERROR_MAX_STRING_SIZE);
    e.details.pCallback = pEmitter->pCallback;
    e.details.details[0].pArbitraryDetail = pArbitrary;
    e.details.details[1].pStringDetail = e.description.c_str();
    Event::dispatch(pEventHandler, event, &e.details);
}

void freeAvCollection(AvCollection *pCollection)
{
    if (pCollection == nullptr)
    {
        return;
    }
    av_dict_free(&pCollection->pOptions);
    if (pCollection->pFrame != nullptr)
    {
        av_frame_free(&pCollection->pFrame);
    }
    if (pCollection->pPacket != nullptr)
    {
        av_packet_free(&pCollection->pPacket);
    }
    if (pCollection->pFilterGraph != nullptr)
    {
        avfilter_graph_free(&pCollection->pFilterGraph);
    }
    if (pCollection->pCodecContext != nullptr)
    {
        avcodec_free_context(&pCollection->pCodecContext);
    }
    if (pCollection->pFormatContext != nullptr)
    {
        avformat_close_input(&pCollection->pFormatContext);
    }

    /*
    if (pCollection->pIOContext != nullptr)
        {
            avio_close(pCollection->pIOContext);
        }
            */
    free(pCollection);
}

AvCollection *fail(AvCollection *pCollection)
{
    freeAvCollection(pCollection);
    return nullptr;
}

std::string getChannelLayoutDescription(AVChannelLayout *pLayout)
{
    std::string description;
    // Did I just miss it? Is there really no way to know in advance what the max length of a channel layout description is?
    int size = 32; // Should be enough. If not this loops twice.
    while (true)
    {
        description.resize(size);
        int result = av_channel_layout_describe(pLayout, description.data(), size);
        if (result < 0)
        {
            return "";
        }
        if (result > size)
        {
            size = result;
            continue;
        }
        break;
    }
    return description;
}

std::string getChannelLayoutDescription(uint8_t channels)
{
    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    return getChannelLayoutDescription(&layout);
}

/**
 * Internal: generates abuffer arguments for the given encoder or decoder.
 * @param pCollection the AvCollection being configured.
 */
static std::string getAbufferArgs(AvCollection *pCollection)
{
    auto sampleFormat = AV_SAMPLE_FMT_FLT;
    // If we're a decoder, the filter graph takes in whatever the codec is giving out.
    // If we're an encoder, we expect the outside world to be dealing in float.
    if (!pCollection->isEncoder)
    {
        sampleFormat = pCollection->pCodecContext->sample_fmt;
    }
    return std::format("sample_rate={}:sample_fmt={}:channel_layout={}",
                       pCollection->inSampleRate,
                       av_get_sample_fmt_name(sampleFormat),
                       getChannelLayoutDescription(&pCollection->inChannelLayout));
}

/**
 * Internal: generates abuffersink args for the given output parameters.
 * @param pChannelLayout the output channel count.
 */
static std::string getAbuffersinkArgs(AVChannelLayout *pChannelLayout)
{
    return std::format("channel_layouts={}",
                       getChannelLayoutDescription(pChannelLayout));
}

/**
 * Generates a filtergraph description for the given encoder or decoder.
 * @param pCollection the AvCollection being configured.
 */
static std::string getSystemFiltergraphDescription(AvCollection *pCollection)
{
    auto pCodecContext = pCollection->pCodecContext;
    auto sampleFormat = pCollection->isEncoder ? pCodecContext->sample_fmt : AV_SAMPLE_FMT_FLT;
    // Note to self: remember that absolutely nothing can come after aformat! FFmpeg silently stops parsing!
    // Another note to self: as of April 2026 the Asetnsamples filter is broken! It yields frames that are around 5% smaller than they should be.
    return std::format("aformat=sample_fmts={}:sample_rates={}:channel_layouts={}",
                       av_get_sample_fmt_name(sampleFormat),
                       pCollection->outSampleRate,
                       getChannelLayoutDescription(&pCollection->outChannelLayout));
}

const AVFilter *findFilter(const char *pName, EventHandler *pEventHandler, EventCallback pCallback)
{
    auto pFilter = avfilter_get_by_name(pName);
    if (pFilter == nullptr)
    {
        EventDetails details;
        details.pCallback = pCallback;
        details.details[0].pStringDetail = pName;
        Event::dispatch(pEventHandler, EVENTTYPE_LACKING_REQUIRED_FILTER, &details);
    }
    return pFilter;
}

bool buildSystemFilterGraph(AvCollection *pCollection)
{
    EventDetails details;
    details.pCallback = pCollection->pCallback;
    details.details[0].pArbitraryDetail = pCollection;
    auto pEventHandler = pCollection->pEventHandler;
    pCollection->pFilterGraph = avfilter_graph_alloc();
    if (pCollection->pFilterGraph == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, &details);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_ALLOCATED_FILTER_GRAPH, &details);

    auto pFilterGraph = pCollection->pFilterGraph;
    auto pCodecContext = pCollection->pCodecContext;
    auto isEncoder = pCollection->isEncoder;
    // If we're decoding, then we promise float to the outside world. If we're encoding, we promise whatever the codec asks for.
    auto filters = getSystemFiltergraphDescription(pCollection);
    AVFilterInOut *pInputList, *pOutputList;
    int result = avfilter_graph_parse2(pFilterGraph, filters.c_str(), &pInputList, &pOutputList);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_PARSE_SYSTEM_FILTER_GRAPH, result);
        return false;
    }

    details.details[1].pStringDetail = filters.c_str();
    // There should be exactly one input and exactly one output.
    // Attempt to extract the filters from the lists, then free the lists, then make sure we have the filters we expect.
    auto pInput = pInputList == nullptr ? nullptr : pInputList->filter_ctx;
    auto pOutput = pOutputList == nullptr ? nullptr : pOutputList->filter_ctx;
    if (pInputList != nullptr)
    {
        avfilter_inout_free(&pInputList);
    }
    if (pOutputList != nullptr)
    {
        avfilter_inout_free(&pOutputList);
    }
    if (pInput == nullptr || pOutput == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_INCORRECT_SYSTEM_FILTER_CONFIG, &details);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_PARSED_SYSTEM_FILTER_GRAPH, &details);

    auto pFilter = findFilter("abuffer", pEventHandler, pCollection->pCallback);
    if (pFilter == nullptr)
    {
        return false;
    }
    // If we're encoding, we expect the outside world to be feeding float. If we're decoding, then the graph ingests whatever the decoder puts out.
    auto bufferArgs = getAbufferArgs(pCollection);
    result = avfilter_graph_create_filter(&pCollection->pFilterGraphIn, pFilter, nullptr, bufferArgs.c_str(), nullptr, pCollection->pFilterGraph);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_INITIALIZE_SYSTEM_ABUFFER, result);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_SYSTEM_ABUFFER, &details);

    auto pFilterGraphIn = pCollection->pFilterGraphIn;
    result = avfilter_link(pFilterGraphIn, 0, pInput, 0);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_LINK_SYSTEM_ABUFFER, result);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_LINKED_SYSTEM_ABUFFER, &details);

    pFilter = findFilter("abuffersink", pEventHandler, pCollection->pCallback);
    if (pFilter == nullptr)
    {
        return false; // Events already handled.
    }
    result = avfilter_graph_create_filter(&pCollection->pFilterGraphOut, pFilter, nullptr, getAbuffersinkArgs(&pCollection->outChannelLayout).c_str(), nullptr, pFilterGraph);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_INITIALIZE_SYSTEM_ABUFFERSINK, result);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_SYSTEM_ABUFFERSINK, &details);
    auto pFilterGraphOut = pCollection->pFilterGraphOut;
    result = avfilter_link(pOutput, 0, pFilterGraphOut, 0);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_LINK_SYSTEM_ABUFFERSINK, result);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_SYSTEM_ABUFFERSINK, &details);

    result = avfilter_graph_config(pFilterGraph, nullptr);
    if (result < 0)
    {
        handleAvError(pCollection, EVENTTYPE_FAILED_TO_CONFIGURE_SYSTEM_FILTER_GRAPH, result);
        return false;
    }
    Event::dispatch(pEventHandler, EVENTTYPE_CONFIGURED_SYSTEM_FILTER_GRAPH, &details);

    return true;
}
