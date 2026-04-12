/**
* CVC support layer
* Audio decoding implementation
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
#include "decoder.h"
#include "internal/events.h"
#include <stdlib.h>
#include <string>
#include <sstream>

#include <iostream>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

/**
 * A common representation of an FFmpeg error event.
 */
struct AvErrorEvent
{
    EventDetails details;
    std::string description;
};

struct Decoder
{
    EventHandler *pEventHandler;     // Does not own.
    AVFormatContext *pFormatContext; // Handles demuxing.
    AVCodecContext *pCodecContext;
    AVPacket *pPacket;
    AVFrame *pFrame;
    AVFilterGraph *pFilterGraph;
    AVFilterInOut *pInputs, *pOutputs;
    int stream; // The audio stream chosen by av_find_best_stream().
};

/**
 * Internal: Handles a Libav* error via the event system.
 * @param pDecoder the decoder that produced the event.
 * @param event the event type being triggered.
 * @param err the Libav* error code to get a description of.
 */
static void handleAvError(Decoder *pDecoder, event_t event, int err)
{
    AvErrorEvent e;
    e.description.resize(AV_ERROR_MAX_STRING_SIZE);
    av_strerror(err, e.description.data(), AV_ERROR_MAX_STRING_SIZE);
    e.details.details[0].pArbitraryDetail = pDecoder;
    e.details.details[1].pStringDetail = e.description.c_str();
    Event::dispatch(pDecoder->pEventHandler, event, &e.details);
}

/**
 * Internal: frees the given decoder and returns a null pointer.
 * Used to reduce verbosity of error handling in decoder setup.
 * @param pDecoder the decoder to free.
 */
static Decoder *fail(Decoder *pDecoder)
{
    casturria_freeDecoder(pDecoder);
    return nullptr;
}

/**
 * Generates a filtergraph description for the given decoder.
 * @param pCodecContext a fully configured and opened AVCodecContext.
 * @param sampleRate the desired output sample rate.
 * @param channels the desired output channels.
 * @returns std::string containing a filtergraph description on success or an empty string on failure.
 */
static std::string getSystemFiltergraphDescription(AVCodecContext *pCodecContext, uint32_t sampleRate, uint8_t channels)
{
    AVChannelLayout channelLayout;
    av_channel_layout_default(&channelLayout, (int)channels);
    std::string description;
    // Did I just miss it? Is there really no way to know in advance what the max length of a channel layout description is?
    int size = 32; // Should be enough. If not this loops twice.
    while (true)
    {
        description.resize(32);
        int result = av_channel_layout_describe(&pCodecContext->ch_layout, description.data(), size);
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
    std::stringstream result;
    result
            << "abuffer@buffer="
            << "sample_rate="
            << pCodecContext->sample_rate
            << ":sample_fmt="
            << av_get_sample_fmt_name(pCodecContext->sample_fmt)
            << ":channel_layout="
            << description
            << "[0],"
            << "[0]aresample=sample_rate="
            << sampleRate
            << "[0],"
            << "[0]abuffersink@buffersink";
    return result.str();
}

Decoder *casturria_newDecoder(const char *pURL, EventHandler *pEventHandler, uint32_t sampleRate, uint8_t channels)
{
    auto pDecoder = (Decoder *)malloc(sizeof(Decoder));
    if (pDecoder == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return nullptr;
    }
    memset(pDecoder, 0, sizeof(Decoder));

    pDecoder->pPacket = av_packet_alloc();
    if (pDecoder->pPacket == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return fail(pDecoder);
    }

    pDecoder->pFrame = av_frame_alloc();
    if (pDecoder->pFrame == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return fail(pDecoder);
    }
    pDecoder->pFilterGraph = avfilter_graph_alloc();
    if (pDecoder->pFilterGraph == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return fail(pDecoder);
    }

    pDecoder->pEventHandler = pEventHandler;
    // The EventDetails can be reused usually by changing just one field.
    EventDetails details;
    details.details[0].pArbitraryDetail = pDecoder;

    int result = avformat_open_input(&pDecoder->pFormatContext, pURL, nullptr, nullptr);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_OPEN_INPUT, result);
        return fail(pDecoder);
    }

    auto pFormatContext = pDecoder->pFormatContext;
    result = avformat_find_stream_info(pFormatContext, nullptr);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_FIND_STREAMS, result);
        return fail(pDecoder);
    }
    // Report that valid streams were discovered.
    details.details[1].intDetail = pFormatContext->nb_streams;
    Event::dispatch(pEventHandler, EVENTTYPE_FOUND_STREAMS, &details);

    const AVCodec *pCodec;
    pDecoder->stream = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pCodec, 0);
    auto stream = pDecoder->stream;
    if (stream < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_FIND_BEST_STREAM, stream);
        return fail(pDecoder);
    }
    // Report details of the discovered audio stream.
    details.details[1].intDetail = stream;
    details.details[2].pStringDetail = pCodec->long_name;
    Event::dispatch(pEventHandler, EVENTTYPE_FOUND_BEST_STREAM, &details);

    pDecoder->pCodecContext = avcodec_alloc_context3(pCodec);
    if (pDecoder->pCodecContext == nullptr)
    {
        // Most likely OOM from what I can tell.
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return fail(pDecoder);
    }

    auto pCodecContext = pDecoder->pCodecContext;
    result = avcodec_parameters_to_context(pCodecContext, pFormatContext->streams[stream]->codecpar);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_INITIALIZE_CODEC_DECODING, result);
        return fail(pDecoder);
    }

    result = avcodec_open2(pCodecContext, pCodec, nullptr);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_INITIALIZE_CODEC_DECODING, result);
        return fail(pDecoder);
    }
    // Finally report successful decoder initialization.
    details.details[1].pStringDetail = pCodec->long_name;
    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_CODEC_DECODING, &details);
    auto pFilterGraph = pDecoder->pFilterGraph;
    auto filters = getSystemFiltergraphDescription(pCodecContext, sampleRate, 2);

    result = avfilter_graph_parse2(pFilterGraph, filters.c_str(), &pDecoder->pInputs, &pDecoder->pOutputs);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_FAILED_TO_INITIALIZE_FILTER_DECODING, result);
        return fail(pDecoder);
    }
    // Should be exactly one unlinked output and no unlinked inputs.
    if (pDecoder->pInputs != nullptr || pDecoder->pOutputs == nullptr || pDecoder->pOutputs->next != nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_FAILED_TO_INITIALIZE_FILTER_DECODING, &details);
        return fail(pDecoder);
    }
    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_FILTER_DECODING, &details);

    return pDecoder;
}
void casturria_freeDecoder(Decoder *pDecoder)
{
    if (pDecoder == nullptr)
    {
        return;
    }
    if (pDecoder->pFrame != nullptr)
    {
        av_frame_free(&pDecoder->pFrame);
    }
    if (pDecoder->pPacket != nullptr)
    {
        av_packet_free(&pDecoder->pPacket);
    }
    if (pDecoder->pInputs != nullptr)
    {
        avfilter_inout_free(&pDecoder->pInputs);
    }
    if (pDecoder->pOutputs != nullptr)
    {
        avfilter_inout_free(&pDecoder->pOutputs);
    }
    if (pDecoder->pFilterGraph != nullptr)
    {
        avfilter_graph_free(&pDecoder->pFilterGraph);
    }
    if (pDecoder->pCodecContext != nullptr)
    {
        avcodec_free_context(&pDecoder->pCodecContext);
    }
    if (pDecoder->pFormatContext != nullptr)
    {
        avformat_close_input(&pDecoder->pFormatContext);
    }
    free(pDecoder);
}