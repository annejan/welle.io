/*
 *    Copyright (C) 2018
 *    Matthias P. Braendli (matthias.braendli@mpb.li)
 *
 *    Copyright (C) 2017
 *    Albrecht Lohofener (albrechtloh@gmx.de)
 *
 *    This file is based on SDR-J
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *
 *    The ringbuffer here is a rewrite of the ringbuffer used in the PA code
 *    All rights remain with their owners
 *
 *    This file is part of the welle.io.
 *    Many of the ideas as implemented in welle.io are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
 *
 *    welle.io is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "CFaadDecoder.h"
#include <string> // to_string

using namespace std;

CFaadDecoder::CFaadDecoder()
{
    aacCap = NeAACDecGetCapabilities();
    aacHandle = NeAACDecOpen();
    aacConf = NeAACDecGetCurrentConfiguration(aacHandle);
    aacInitialized  = false;
}

CFaadDecoder::~CFaadDecoder(void)
{
    NeAACDecClose(aacHandle);
}

int CFaadDecoder::get_aac_channel_configuration(
        int16_t m_mpeg_surround_config,
        uint8_t aacChannelMode) {

    switch(m_mpeg_surround_config) {
        case 0:     // no surround
            return aacChannelMode ? 2 : 1;
        case 1:     // 5.1
            return 6;
        case 2:     // 7.1
            return 7;
        default:
            return -1;
    }
}

vector<int16_t> CFaadDecoder::MP42PCM(
    uint8_t dacRate, uint8_t sbrFlag,
    int16_t mpegSurround,
    uint8_t aacChannelMode,
    uint8_t buffer[],
    int16_t bufferLength,
    uint32_t *sampleRate,
    bool *isParametricStereo)
{
    uint8_t channels;
    long unsigned int   sample_rate;
    NeAACDecFrameInfo hInfo;

    if (!aacInitialized) {
        /* AudioSpecificConfig structure (the only way to select 960 transform here!)
         *
         *  00010 = AudioObjectType 2 (AAC LC)
         *  xxxx  = (core) sample rate index
         *  xxxx  = (core) channel config
         *  100   = GASpecificConfig with 960 transform
         *
         * SBR: implicit signaling sufficient - libfaad2
         * automatically assumes SBR on sample rates <= 24 kHz
         * => explicit signaling works, too, but is not necessary here
         *
         * PS:  implicit signaling sufficient - libfaad2
         * therefore always uses stereo output (if PS support was enabled)
         * => explicit signaling not possible, as libfaad2 does not
         * support AudioObjectType 29 (PS)
         */
        int core_sr_index = dacRate ?
                (sbrFlag ? 6 : 3) :
                (sbrFlag ? 8 : 5);   // 24/48/16/32 kHz
        int core_ch_config = get_aac_channel_configuration(mpegSurround,
                aacChannelMode);

        if (core_ch_config == -1) {
            throw runtime_error("CFaadDecoder:"
                    " Unrecognized mpeg surround config (ignored): " +
                    to_string(mpegSurround));
        }

        uint8_t asc[2];
        asc[0] = 0b00010 << 3 | core_sr_index >> 1;
        asc[1] = (core_sr_index & 0x01) << 7 | core_ch_config << 3 | 0b100;
        long int init_result = NeAACDecInit2(aacHandle,
                asc,
                sizeof (asc),
                &sample_rate,
                &channels);
        if (init_result != 0) {
            /* If some error initializing occured, skip the file */
            const string errmsg = NeAACDecGetErrorMessage(-init_result);
            NeAACDecClose (aacHandle);
            throw runtime_error("CFaadDecoder:"
                    " Error initializing decoder library: " + errmsg);
        }
        aacInitialized = true;
    }

    int16_t *outBuffer;
    outBuffer = (int16_t *)NeAACDecDecode(aacHandle, &hInfo, buffer, bufferLength);

    if (isParametricStereo) {
        *isParametricStereo = (hInfo.ps == 1);
    }

    sample_rate = hInfo.samplerate;
    if (sampleRate) {
        *sampleRate = sample_rate;
    }

    size_t samples = hInfo.samples;

    //  fprintf (stderr, "bytes consumed %d\n", (int)(hInfo. bytesconsumed));
    //  fprintf (stderr, "samplerate = %d, samples = %d, channels = %d, error = %d, sbr = %d\n", sample_rate, samples,
    //           hInfo.channels,
    //           hInfo.error,
    //           hInfo.sbr);
    //  fprintf (stderr, "header = %d\n", hInfo.header_type);
    channels    = hInfo.channels;

    // Error check
    if (hInfo.error != 0)
    {
        fprintf(stderr, "CFaadDecoder: warning: %s\n",
                faacDecGetErrorMessage(hInfo.error));
        return {};
    }

    // Sometimes it can be 0 samples
    if (samples == 0)
        return {};

    if (channels == 2) {
        vector<int16_t> audio(outBuffer, outBuffer + samples);
        return audio;
    }
    else {
        if (channels == 1) {
            vector<int16_t> audio(2*samples);
            for (size_t i = 0; i < samples; i ++) {
                audio[2 * i]     = outBuffer[i];
                audio[2 * i + 1] = outBuffer[i];
            }
            return audio;
        }
        else {
            throw runtime_error("CFaadDecoder: Cannot handle these channels");
        }
    }
}
