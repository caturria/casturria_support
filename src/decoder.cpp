/**
* Casturria support layer
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
#include "internal/util.h"

Decoder *casturria_newDecoder(const char *pURL, EventCallback pCallback, uint32_t sampleRate, uint8_t channels)
{
    auto pDecoder = newAvCollection(pCallback);
    if (pDecoder == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
    }

    int result = avformat_open_input(&pDecoder->pFormatContext, pURL, nullptr, nullptr);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pDecoder);
    }
    dispatchEvent(pCallback, EVENTTYPE_SETUP_MILESTONE, std::format("Successfully opened an asset at the URL '{}'.", pURL));

    auto pFormatContext = pDecoder->pFormatContext;
    result = avformat_find_stream_info(pFormatContext, nullptr);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pDecoder);
    }
    // Report that valid streams were discovered.
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("The asset at '{}' contains {} {}.",
                    pURL,
                    pFormatContext->nb_streams,
                    pFormatContext->nb_streams == 1 ? "stream" : "streams"));

    const AVCodec *pCodec;
    pDecoder->stream = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pCodec, 0);
    auto stream = pDecoder->stream;
    if (stream < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pDecoder);
    }
    // Report details of the discovered audio stream.
    dispatchEvent(pCallback, EVENTTYPE_SETUP_MILESTONE, std::format("Successfully identified stream {} of asset '{}' as '{}'.", stream, pURL, pCodec->long_name));

    pDecoder->pCodecContext = avcodec_alloc_context3(pCodec);
    if (pDecoder->pCodecContext == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pDecoder);
    }

    auto pCodecContext = pDecoder->pCodecContext;
    result = avcodec_parameters_to_context(pCodecContext, pFormatContext->streams[stream]->codecpar);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pDecoder);
    }

    // At this point we finally know the details of the input file, needed for the upcoming filter configuration step.
    pDecoder->inSampleRate = pCodecContext->sample_rate;
    pDecoder->outSampleRate = sampleRate;
    pDecoder->inChannelLayout = pCodecContext->ch_layout;
    av_channel_layout_default(&pDecoder->outChannelLayout, channels);

    result = avcodec_open2(pCodecContext, pCodec, nullptr);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pDecoder);
    }
    // Finally report successful decoder initialization.
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("Successfully initialized codec '{}' for asset '{}'.",
                    pCodec->long_name,
                    pURL));

    if (!buildSystemFilterGraph(pDecoder))
    {
        return fail(pDecoder);
    }

    return pDecoder;
}

void casturria_freeDecoder(Decoder *pDecoder)
{
    freeAvCollection(pDecoder);
}

/**
 * Internal: move a packet from the demuxing layer to the codec.
 * @param pDecoder the decoder to process.
 */
static bool processPacket(Decoder *pDecoder)
{
    auto pCallback = pDecoder->pCallback;

    while (true)
    {
        int result = av_read_frame(pDecoder->pFormatContext, pDecoder->pPacket);
        if (result == AVERROR_EOF)
        {
            dispatchEvent(pCallback, EVENTTYPE_DEMUX_COMPLETE, "Demux complete!");
            avcodec_send_packet(pDecoder->pCodecContext, nullptr); // Flush.
            return true;                                           // Decoding layer should still try to render.
        }

        if (result != 0)
        {
            // Legitimate error here. Could be I/O related.
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
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
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
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
    auto pCallback = pDecoder->pCallback;
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
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
            return false;
        }

        // Got a frame. Deliver it to the filter layer.
        result = av_buffersrc_add_frame(pDecoder->pFilterGraphIn, pDecoder->pFrame);
        av_frame_unref(pDecoder->pFrame);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
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
    auto pCallback = pDecoder->pCallback;
    while (true)
    {
        int result = av_buffersink_get_frame(pDecoder->pFilterGraphOut, pDecoder->pFrame);
        if (result == AVERROR_EOF)
        {
            // Unlike earlier stages, EOF actually needs to stop here.
            dispatchEvent(pCallback, EVENTTYPE_DECODE_COMPLETE, "Decode complete!");
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
            dispatchEvent(
                pCallback,
                EVENTTYPE_OPERATION_FAILURE,
                std::format("Packet returned from filter has sample format {} instead of flt. This is likely a bug.",
                            av_get_sample_fmt_name((AVSampleFormat)pFrame->format)));
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