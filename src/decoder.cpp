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
#include <format>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
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
    AVFilterContext *pFilterGraphIn, *pFilterGraphOut; // abuffer and abuffersink. They belong to the filter graph.
    int stream;                                        // The audio stream chosen by av_find_best_stream().
    size_t framesLeftInBatch;                          // Number of audio frames remaining in current FFmpeg output frame.
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
 * Internal: get a channel layout description as a C++ string.
 * @param pLayout the channel layout to describe.
 */
static std::string getChannelLayoutDescription(AVChannelLayout *pLayout)
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

/**
 * Overload that gets the default layout description for a given channel count.
 * @param channels the channel count to get a description of.
 */
static std::string getChannelLayoutDescription(uint8_t channels)
{
    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    return getChannelLayoutDescription(&layout);
}

/**
 * Internal: generates abuffer arguments for the given decoder.
 * @param pCodecContext a fully configured and opened AVCodecContext.
 * @returns std::string containing a filtergraph description on success or an empty string on failure.
 */
static std::string getAbufferArgs(AVCodecContext *pCodecContext)
{
    return std::format("sample_rate={}:sample_fmt={}:channel_layout={}",
                       pCodecContext->sample_rate,
                       av_get_sample_fmt_name(pCodecContext->sample_fmt),
                       getChannelLayoutDescription(&pCodecContext->ch_layout));
}

/**
 * Internal: generates abuffersink args for the given output parameters.
 * @param channels the output channel count.
 */
std::string getAbuffersinkArgs(uint8_t channels)
{
    return std::format("channel_layouts={}",
                       getChannelLayoutDescription(channels));
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
    return std::format("aformat=sample_fmts=flt:sample_rates={}:channel_layouts={}",
                       sampleRate,
                       getChannelLayoutDescription(channels));
}

/**
 * Internal: builds the system (non-configurable) filter graph for a decoder.
 * @param pDecoder the decoder to process.
 * @param sampleRate the sample rate to decode to.
 * @param channels the number of channels to decode to.
 */
static bool buildSystemFilterGraph(Decoder *pDecoder, uint32_t sampleRate, uint8_t channels)
{
    EventDetails details;
    details.details[0].pArbitraryDetail = pDecoder;
    auto pEventHandler = pDecoder->pEventHandler;
    pDecoder->pFilterGraph = avfilter_graph_alloc();
    if (pDecoder->pFilterGraph == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_OUT_OF_MEMORY, nullptr);
        return fail(pDecoder);
    }
    auto filters = getSystemFiltergraphDescription(pDecoder->pCodecContext, sampleRate, channels);
    AVFilterInOut *pInputList, *pOutputList;
    int result = avfilter_graph_parse2(pDecoder->pFilterGraph, filters.c_str(), &pInputList, &pOutputList);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
        return false;
    }
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
        Event::dispatch(pEventHandler, EVENTTYPE_DECODE_BUG, &details);
        return false;
    }

    auto pFilter = avfilter_get_by_name("abuffer");
    if (pFilter == nullptr)
    {
        // Only a broken FFmpeg build or wild bug can get here.
        Event::dispatch(pEventHandler, EVENTTYPE_DECODE_BUG, &details);
    }
    auto bufferArgs = getAbufferArgs(pDecoder->pCodecContext);
    result = avfilter_graph_create_filter(&pDecoder->pFilterGraphIn, pFilter, "in", bufferArgs.c_str(), nullptr, pDecoder->pFilterGraph);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
    }
    auto pFilterGraphIn = pDecoder->pFilterGraphIn;
    result = avfilter_link(pFilterGraphIn, 0, pInput, 0);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
        return false;
    }
    pFilter = avfilter_get_by_name("abuffersink");
    if (pFilter == nullptr)
    {
        Event::dispatch(pEventHandler, EVENTTYPE_DECODE_BUG, &details);
        return false;
    }
    result = avfilter_graph_create_filter(&pDecoder->pFilterGraphOut, pFilter, nullptr, getAbuffersinkArgs(channels).c_str(), nullptr, pDecoder->pFilterGraph);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
        return false;
    }
    auto pFilterGraphOut = pDecoder->pFilterGraphOut;
    result = avfilter_link(pOutput, 0, pDecoder->pFilterGraphOut, 0);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
        return false;
    }
    result = avfilter_graph_config(pDecoder->pFilterGraph, nullptr);
    if (result < 0)
    {
        handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
        return false;
    }

    return true;
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
    if (!buildSystemFilterGraph(pDecoder, sampleRate, channels))
    {
        fail(pDecoder);
    }

    Event::dispatch(pEventHandler, EVENTTYPE_INITIALIZED_FILTER_DECODING, &details);
    // abort();

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

/**
 * Internal: move a packet from the demuxing layer to the codec.
 * @param pDecoder the decoder to process.
 */
static bool processPacket(Decoder *pDecoder)
{
    while (true)
    {
        int result = av_read_frame(pDecoder->pFormatContext, pDecoder->pPacket);
        if (result == AVERROR_EOF)
        {
            EventDetails details;
            details.details[0].pArbitraryDetail = pDecoder;
            Event::dispatch(pDecoder->pEventHandler, EVENTTYPE_DEMUX_COMPLETE, &details);
            avcodec_send_packet(pDecoder->pCodecContext, nullptr); // Flush.
            return true;                                           // Decoding layer should still try to render.
        }
        if (result != 0)
        {
            // Legitimate error here. Could be I/O related.
            handleAvError(pDecoder, EVENTTYPE_DEMUX_ERROR, result);
            return false;
        }
        if (pDecoder->pPacket->stream_index != pDecoder->stream)
        {
            // Packet is metadata, video, or something else we don't want to deal with here.
            av_packet_unref(pDecoder->pPacket);
            continue;
        }
        result = avcodec_send_packet(pDecoder->pCodecContext, pDecoder->pPacket);
        av_packet_unref(pDecoder->pPacket);
        if (result != 0)
        {
            // A decoding error. Probably a corrupt or invalid stream.
            handleAvError(pDecoder, EVENTTYPE_DECODE_ERROR, result);
            return false;
        }
        return true;
    }
}

/**
 * Internal: attempt to move a frame from the codec layer to the filter layer.
 * @param pDecoder the decoder to process.
 */
static bool processFrame(Decoder *pDecoder)
{
    while (true)
    {
        int result = avcodec_receive_frame(pDecoder->pCodecContext, pDecoder->pFrame);
        if (result == AVERROR(EAGAIN))
        {
            // Need more input from the demuxer.
            if (!processPacket(pDecoder))
            {
                // Not EOF, some other demuxing failure.
                return false;
            }
            // Got another packet, so try decoding again.
            continue;
        }
        if (result == AVERROR_EOF)
        {
            // Flush the filter chain.
            (void)av_buffersrc_add_frame(pDecoder->pFilterGraphIn, nullptr);
            return true;
        }
        if (result < 0)
        {
            // Decoding error. Probably corrupted data.
            handleAvError(pDecoder, EVENTTYPE_DECODE_ERROR, result);
            return false;
        }
        // Got a frame. Deliver it to the filter layer.
        result = av_buffersrc_add_frame(pDecoder->pFilterGraphIn, pDecoder->pFrame);
        av_frame_unref(pDecoder->pFrame);
        if (result < 0)
        {
            handleAvError(pDecoder, EVENTTYPE_DECODE_FILTER_ERROR, result);
            return false;
        }
        return true;
    }
}

/**
 * Attempt to withdraw a frame from the filter and get it ready for output.
 * All output must be consumed before this is called again or data loss will occur.
 * @param pDecoder the decoder to work on.
 */
static bool prepareNextFrame(Decoder *pDecoder)
{
    av_frame_unref(pDecoder->pFrame);
    pDecoder->framesLeftInBatch = 0;
    EventDetails details;
    details.details[0].pArbitraryDetail = pDecoder;
    while (true)
    {
        int result = av_buffersink_get_frame(pDecoder->pFilterGraphOut, pDecoder->pFrame);
        if (result == AVERROR_EOF)
        {
            // Unlike earlier stages, EOF actually needs to stop here.
            Event::dispatch(pDecoder->pEventHandler, EVENTTYPE_DECODE_COMPLETE, &details);
            return false;
        }
        if (result == AVERROR(EAGAIN))
        {
            if (processFrame(pDecoder))
            {
                // A frame was successfully decoded. Try filtering again.
                continue;
            }
            return false; // Decoding error.
        }
        // Got a filtered frame.
        auto pFrame = pDecoder->pFrame;
        if (pFrame->format != AV_SAMPLE_FMT_FLT)
        {
            Event::dispatch(pDecoder->pEventHandler, EVENTTYPE_DECODE_BUG, &details);
        }
        pDecoder->framesLeftInBatch = pFrame->nb_samples;
        return true;
    }
}

/**
 * Output up to the requested number of frames from the currently prepared batch.
 * @param pDecoder the decoder to work on.
 * @param pBuffer where to write the output to.
 * @param count the maximum number of frames to render.
 * @returns number of frames rendered.
 */
static size_t render(Decoder *pDecoder, float *pBuffer, size_t count)
{
    count = std::min<size_t>(count, pDecoder->framesLeftInBatch);
    if (count <= 0)
    {
        return 0;
    }
    auto pFrame = pDecoder->pFrame;
    // Figure out how much of this frame has already been delivered and where the first fresh sample is.
    size_t delta = pFrame->nb_samples - pDecoder->framesLeftInBatch;
    auto pMark = (float *)pFrame->data[0] + (delta * pFrame->ch_layout.nb_channels);
    memcpy(pBuffer, pMark, count * 4 * pFrame->ch_layout.nb_channels);
    pDecoder->framesLeftInBatch -= count;
    return count;
}

size_t casturria_decode(Decoder *pDecoder, float *pOutput, size_t count)
{
    auto pMark = pOutput;
    size_t framesRendered = 0;
    while (count > 0)
    {
        if (pDecoder->framesLeftInBatch == 0)
        {
            if (!prepareNextFrame(pDecoder))
            {
                break; // EOF or error.
            }
        }
        auto framesRenderedThisTime = render(pDecoder, pMark, count);
        framesRendered += framesRenderedThisTime;
        count -= framesRenderedThisTime;
        pMark += (framesRenderedThisTime * pDecoder->pFrame->ch_layout.nb_channels);
    }
    return framesRendered;
}