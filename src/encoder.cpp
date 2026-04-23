/**
* Casturria support layer
* Audio encoding implementation
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
#include "encoder.h"
#include "internal/util.h"
#include <simdjson.h>

/**
 * Internal: handle an invalid argument error.
 * @param pCallback the callback to dispatch the error to.
 * @param key the JSON key.
 * @param value the invalid value.
 */
static void handleInvalidArgument(EventCallback pCallback, const std::string &key, const std::string &value)
{
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_FAILURE,
        std::format("Invalid value '{}' for key '{}'.",
                    key,
                    value));
}

/**
 * Internal: negotiate an output sample format with an encoder.
 * @param pCodec the codec.
 */
AVSampleFormat negotiateSampleFormat(const AVCodec *pCodec)
{
    // We currently static link FFmpeg, but we'll take their advice not to use AV_SAMPLE_FMT_NB anyway just in case we ever switch to dynamic linking.
    const int length = AV_SAMPLE_FMT_S64P + 1;
    const AVSampleFormat *pSupportedFormats;
    int numSupportedFormats;
    bool table[length];

    // Build a lookup table of all possible formats, then rank them.
    memset(table, 0, sizeof(table));

    avcodec_get_supported_config(nullptr, pCodec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **)&pSupportedFormats, &numSupportedFormats);
    if (pSupportedFormats == nullptr)
    {
        // Codec supports all sample formats. No need to continue.
        return AV_SAMPLE_FMT_FLT;
    }

    for (int i = 0; i < numSupportedFormats; i++)
    {
        table[pSupportedFormats[i]] = true;
    }

    // first choice is floating point interleved because it's what everything else works with internally.
    if (table[AV_SAMPLE_FMT_FLT])
        return AV_SAMPLE_FMT_FLT;

    // Floating point planar is the next closest.
    if (table[AV_SAMPLE_FMT_FLTP])
        return AV_SAMPLE_FMT_FLTP;

    // If those aren't available, then the next best choice would be double since we can convert to this with no quality loss.
    // In practice it's pretty unlikely we negotiate anything before reaching for 16-bit at this point.
    if (table[AV_SAMPLE_FMT_DBL])
        return AV_SAMPLE_FMT_DBL;

    if (table[AV_SAMPLE_FMT_DBLP])
        return AV_SAMPLE_FMT_DBLP;

    // If we have to convert to integer, prefer 32 since this is the closest to what our input data is.
    if (table[AV_SAMPLE_FMT_S32])
        return AV_SAMPLE_FMT_S32;

    if (table[AV_SAMPLE_FMT_S32P])
        return AV_SAMPLE_FMT_S32P;

    // Try larger ints before negotiating down.
    if (table[AV_SAMPLE_FMT_S64])
        return AV_SAMPLE_FMT_S64;

    if (table[AV_SAMPLE_FMT_S64P])
        return AV_SAMPLE_FMT_S64P;

    if (table[AV_SAMPLE_FMT_S16])
        return AV_SAMPLE_FMT_S16;

    if (table[AV_SAMPLE_FMT_S16P])
        return AV_SAMPLE_FMT_S16P;

    // One more round just for kicks at this point. I doubt there's any interest in a codec that only supports 8-bit samples in a radio context.
    if (table[AV_SAMPLE_FMT_U8])
        return AV_SAMPLE_FMT_U8;

    if (table[AV_SAMPLE_FMT_U8P])
        return AV_SAMPLE_FMT_U8P;

    // If we get here it's most likely because a non-audio codec was chosen.
    return AV_SAMPLE_FMT_NONE;
}

/**
 * Negotiate a suitable output sample rate with an encoder.
 * @param pCodec the codec being used for encoding.
 * @param requestedSampleRate the desired sample rate.
 * @returns the supported sample rate that is closest to the desired sample rate.
 */
uint32_t negotiateSampleRate(const AVCodec *pCodec, uint32_t requestedSampleRate)
{
    const int *pSupportedRates;
    int numSupportedRates;
    avcodec_get_supported_config(nullptr, pCodec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void **)&pSupportedRates, &numSupportedRates);
    if (pSupportedRates == nullptr)
    {
        // Codec can encode to any rate.
        return requestedSampleRate;
    }

    uint32_t consideredSampleRate = 0;
    auto diff = std::numeric_limits<uint32_t>::max();

    for (int i = 0; i < numSupportedRates; i++)
    {
        auto currentDiff = std::abs((int)(pSupportedRates[i] - requestedSampleRate));
        if (currentDiff < diff)
        {
            diff = currentDiff;
            consideredSampleRate = pSupportedRates[i];
        }
    }

    return consideredSampleRate;
}

Encoder *casturria_newEncoder(const char *pURL, EventCallback pCallback, uint32_t inSampleRate, uint8_t inChannels, const char *pOptions)
{
    auto pEncoder = newAvCollection(pCallback);
    if (pEncoder == nullptr)
    {
        return nullptr; // Events are already handled.
    }

    pEncoder->isEncoder = true;
    pEncoder->inSampleRate = inSampleRate;
    av_channel_layout_default(&pEncoder->inChannelLayout, inChannels);
    pEncoder->outSampleRate = pEncoder->inSampleRate;       // JSON might override this later.
    pEncoder->outChannelLayout = pEncoder->inChannelLayout; // JSON might override this later.
    pEncoder->pFormatContext = avformat_alloc_context();
    if (pEncoder->pFormatContext == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pEncoder);
    }

    auto pFormatContext = pEncoder->pFormatContext;
    pFormatContext->oformat = av_guess_format(nullptr, pURL, nullptr); // Placeholder. The JSON might override this. Verify later.
    const AVCodec *pCodec = nullptr;                                   // JSON might set this. Otherwise we guess it later from the format.
    uint32_t bitRate = 0;                                              // If unchanged, use codec-specific default.
    int quality = -1;                                                  // If unchanged, use codec-specific default.
    int result;

    try
    {
        simdjson::ondemand::parser parser;
        // Make the JSON optional.
        std::string json(pOptions == nullptr ? "{}" : pOptions);
        auto doc = parser.iterate(simdjson::pad(json));
        auto obj = doc.get_object();

        for (auto i : obj)
        {
            // Strings are copied because most of the time they need to be passed as C-strings.
            auto key = (std::string)i.escaped_key().value();
            if (key == "format")
            {
                // Set the output container format.
                auto value = (std::string)i.value();
                pFormatContext->oformat = av_guess_format(value.c_str(), nullptr, nullptr);
                if (pFormatContext->oformat == nullptr)
                {
                    handleInvalidArgument(pCallback, key, value);
                    return fail(pEncoder);
                }
            }

            else if (key == "codec")
            {
                auto value = (std::string)i.value();
                pCodec = avcodec_find_encoder_by_name(value.c_str());
                if (pCodec == nullptr)
                {
                    handleInvalidArgument(pCallback, key, value);
                    return fail(pEncoder);
                }
            }

            else if (key == "sampleRate")
            {
                pEncoder->outSampleRate = (uint32_t)i.value();
            }

            else if (key == "channels")
            {
                av_channel_layout_default(&pEncoder->outChannelLayout, (uint8_t)i.value());
            }

            else if (key == "bitRate")
            {
                bitRate = (uint32_t)i.value();
            }

            else if (key == "quality")
            {
                quality = (int)i.value();
            }

            else
            {
                // Field is a muxer or codec private option. Support either a string or a number.
                auto value = i.value();
                if (value.is_integer())
                {
                    auto asString = std::format("{}", (int32_t)value);
                    result = av_dict_set(&pEncoder->pOptions, key.c_str(), asString.c_str(), 0);
                }

                else
                {
                    // Assume it's a string -- the only other supportable type -- and handle incorrect_type if it comes to that.
                    result = av_dict_set(&pEncoder->pOptions, key.c_str(), ((std::string)value).c_str(), 0);
                }

                if (result < 0)
                {
                    dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
                    return fail(pEncoder);
                }
            }
        }
    }

    catch (simdjson::simdjson_error &e)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, e.what());
        return fail(pEncoder);
    }
    // JSON parsing complete. We should have everything we need to build an encoder now.

    if (pFormatContext->oformat == nullptr)
    {
        // JSON lacks a preference, and the format couldn't be deduced from the URL.
        dispatchEvent(
            pCallback,
            EVENTTYPE_SETUP_FAILURE,
            std::format("Unable to determine an appropriate output format for '{}'.",
                        pURL));
        return fail(pEncoder);
    }
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("Selected output format '{}' for '{}'.",
                    pFormatContext->oformat->long_name,
                    pURL));

    if (pCodec == nullptr)
    {
        // We don't have a codec yet, but we do have a format, so we can try deduction.
        pCodec = avcodec_find_encoder(av_guess_codec(pFormatContext->oformat, nullptr, pURL, nullptr, AVMEDIA_TYPE_AUDIO));
    }

    if (pCodec == nullptr)
    {
        // No way to proceed if we still lack a codec here.
        dispatchEvent(
            pCallback,
            EVENTTYPE_SETUP_FAILURE,
            std::format("Unable to determine an appropriate output codec for '{}'.",
                        pURL));

        return fail(pEncoder);
    }
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("Selected output codec '{}' for '{}'.",
                    pCodec->long_name,
                    pURL));

    pEncoder->pCodecContext = avcodec_alloc_context3(pCodec);
    auto pCodecContext = pEncoder->pCodecContext;
    if (pCodecContext == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pEncoder);
    }

    // Determine the "best" sample format and rate that's supported by the encoder and as close as possible to what we ideally want.
    pCodecContext->sample_fmt = negotiateSampleFormat(pCodec);
    if (pCodecContext->sample_fmt == AV_SAMPLE_FMT_NONE)
    {
        dispatchEvent(
            pCallback,
            EVENTTYPE_SETUP_FAILURE,
            std::format("Failed to negotiate an appropriate output sample format for codec '{}'.",
                        pCodec->long_name));
        return fail(pEncoder);
    }

    pCodecContext->sample_rate = negotiateSampleRate(pCodec, pEncoder->outSampleRate);
    if (pCodecContext->sample_rate == 0)
    {
        dispatchEvent(
            pCallback,
            EVENTTYPE_SETUP_FAILURE,
            std::format("Failed to negotiate an appropriate output sampling rate for codec '{}'.",
                        pCodec->long_name));

        return fail(pEncoder);
    }

    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("The asset '{}' will be encoded to sample format {} at {} Hz.",
                    pURL,
                    av_get_sample_fmt_name(pCodecContext->sample_fmt),
                    pCodecContext->sample_rate));

    pEncoder->outSampleRate = pCodecContext->sample_rate;
    pCodecContext->bit_rate = bitRate;

    if (quality > 0)
    {
        pCodecContext->global_quality = (quality - 1) * FF_QP2LAMBDA;
        pCodecContext->flags |= AV_CODEC_FLAG_QSCALE; // Overrides bitRate.
    }

    pCodecContext->ch_layout = pEncoder->outChannelLayout;

    result = avcodec_open2(pCodecContext, pCodec, &pEncoder->pOptions);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pEncoder);
    }

    auto pStream = avformat_new_stream(pFormatContext, pCodec);
    if (pStream == nullptr)
    {
        dispatchOutOfMemory(pCallback, EVENTTYPE_SETUP_FAILURE);
        return fail(pEncoder);
    }

    result = avcodec_parameters_from_context(pStream->codecpar, pCodecContext);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pEncoder);
    }

    if (!buildSystemFilterGraph(pEncoder))
    {
        return fail(pEncoder);
    }

    result = avio_open2(&pEncoder->pIOContext, pURL, AVIO_FLAG_WRITE, nullptr, &pEncoder->pOptions);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pEncoder);
    }
    pFormatContext->pb = pEncoder->pIOContext;

    result = avformat_write_header(pFormatContext, &pEncoder->pOptions);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_SETUP_FAILURE, result);
        return fail(pEncoder);
    }
    dispatchEvent(
        pCallback,
        EVENTTYPE_SETUP_MILESTONE,
        std::format("Successfully opened and wrote header to asset '{}'.",
                    pURL));

    return pEncoder;
}

void casturria_freeEncoder(Encoder *pEncoder)
{
    freeAvCollection(pEncoder);
}

/**
 * Internal: ingest input and submit it to the filter layer.
 * @param pEncoder the encoder being encoded to.
 * @param pInput a buffer containing input audio.
 * @param count the number of frames being submitted.
 */
static bool ingest(Encoder *pEncoder, const float *pInput, size_t count)
{
    auto pCallback = pEncoder->pCallback;
    auto pFrame = pEncoder->pFrame;
    auto pCodecContext = pEncoder->pCodecContext;
    av_frame_unref(pFrame);
    // Submit the entire block to the filter as a single frame.
    pFrame->ch_layout = pEncoder->inChannelLayout;
    pFrame->format = AV_SAMPLE_FMT_FLT;
    pFrame->sample_rate = pEncoder->inSampleRate;
    pFrame->nb_samples = count;
    int result = av_frame_get_buffer(pFrame, 0);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        return false;
    }

    memcpy(pFrame->data[0], pInput, count * 4 * pCodecContext->ch_layout.nb_channels);
    result = av_buffersrc_add_frame(pEncoder->pFilterGraphIn, pFrame);
    av_frame_unref(pFrame);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        return false;
    }

    return true;
}

/**
 * Internal: encode and output as many frames as possible.
 * @param pEncoder the encoder being used.
 */
static bool encodeAndOutputFrames(Encoder *pEncoder)
{
    auto pCallback = pEncoder->pCallback;
    auto pPacket = pEncoder->pPacket;
    av_packet_unref(pPacket);
    while (true)
    {
        int result = avcodec_receive_packet(pEncoder->pCodecContext, pPacket);
        if (result == AVERROR(EAGAIN))
        {
            return true; // Ingest more input to continue.
        }

        if (result == AVERROR_EOF)
        {
            return true; // done.
        }

        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        }

        result = av_interleaved_write_frame(pEncoder->pFormatContext, pPacket);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OUTPUT_ERROR, result);
            return false;
        }
    }
}

/**
 * Internal: Move as many frames as possible from the filter layer to the encoder.
 * @param pEncoder the encoder being used.
 */
static bool sendFramesToEncoder(Encoder *pEncoder)
{
    auto pCallback = pEncoder->pCallback;

    auto pFrame = pEncoder->pFrame;
    auto pCodecContext = pEncoder->pCodecContext;
    av_frame_unref(pFrame);
    int result;

    while (true)
    {
        // Ideally we'd use the asetnsamples filter here for codecs that require specific frame sizes. But it's bugged as of April 2026 and produces frames that are smaller than they're supposed to be.
        if (pCodecContext->frame_size == 0)
        {
            result = av_buffersink_get_frame(pEncoder->pFilterGraphOut, pEncoder->pFrame);
        }

        else
        {
            result = av_buffersink_get_samples(pEncoder->pFilterGraphOut, pFrame, pCodecContext->frame_size);
        }

        if (result == AVERROR(EAGAIN))
        {
            return true; // Ingest more input to continue.
        }

        if (result == AVERROR_EOF)
        {
            // Tell the encoder to flush, then drain the last of it.
            avcodec_send_frame(pCodecContext, nullptr);
            encodeAndOutputFrames(pEncoder);
            return true;
        }
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        }

        pFrame->pts = pEncoder->timestamp;
        pEncoder->timestamp += pFrame->nb_samples;

        result = avcodec_send_frame(pEncoder->pCodecContext, pFrame);
        av_frame_unref(pFrame);
        if (result < 0)
        {
            dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
            return false;
        }

        encodeAndOutputFrames(pEncoder);
    }
}

void casturria_encode(Encoder *pEncoder, const float *pInput, size_t count)
{
    if (!ingest(pEncoder, pInput, count))
    {
        return;
    }

    if (!sendFramesToEncoder(pEncoder))
    {
        return;
    }
}

void casturria_finalizeEncoder(Encoder *pEncoder)
{
    auto pCallback = pEncoder->pCallback;
    // Drain the filter layer:
    int result = av_buffersrc_add_frame(pEncoder->pFilterGraphIn, nullptr);
    if (result < 0)
    {
        dispatchEvent(pCallback, EVENTTYPE_OPERATION_FAILURE, result);
        return;
    }

    // The encoder will detect the EOF state from the filter and perform its own draining stage.
    sendFramesToEncoder(pEncoder);
    // Finalize the file itself:
    av_write_trailer(pEncoder->pFormatContext);
}