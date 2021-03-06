/*
 *    Copyright (C) 2018
 *    Matthias P. Braendli (matthias.braendli@mpb.li)
 *
 *    Copyright (C) 2013 2015
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J (JSDR).
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Once the bits are "in", interpretation and manipulation
 *  should reconstruct the data blocks.
 *  Ofdm_decoder is called once every Ts samples, and
 *  its invocation results in 2 * Tu bits
 */

#include <iostream>
#include "ofdm/ofdm-decoder.h"

/**
 * \brief OfdmDecoder
 * The class OfdmDecoder is - when implemented in a separate thread -
 * taking the data from the ofdmProcessor class in, and
 * will extract the Tu samples, do an FFT and extract the
 * carriers and map them on (soft) bits
 */
OfdmDecoder::OfdmDecoder(
        const DABParams& p,
        RadioControllerInterface& mr,
        FicHandler& ficHandler,
        MscHandler& mscHandler) :
    params(p),
    radioInterface(mr),
    ficHandler(ficHandler),
    mscHandler(mscHandler),
    fft_handler(p.T_u),
    interleaver(p)
{
    ibits.resize(2 * params.K);

    T_g = params.T_s - params.T_u;
    fft_buffer = fft_handler.getVector();
    phaseReference.resize(params.T_u);

    snrCount    = 0;
    snr         = 0;

    /**
     * When implemented in a thread, the thread controls the
     * reading in of the data and processing the data through
     * functions for handling block 0, FIC blocks and MSC blocks.
     *
     * We just create a large buffer where index i refers to block i.
     *
     */
    command = new DSPCOMPLEX*[params.L];
    for (int16_t i = 0; i < params.L; i ++) {
        command[i] = new DSPCOMPLEX[params.T_u];
    }
    amount = 0;
    thread = std::thread(&OfdmDecoder::workerthread, this);
}

OfdmDecoder::~OfdmDecoder(void)
{
    stop();

    for (int16_t i = 0; i < params.L; i ++)
        delete[] command[i];
    delete[] command;
}

void OfdmDecoder::stop(void)
{
    running = false;
    commandHandler.notify_all();
    if (thread.joinable()) {
        thread.join();
    }
}

/**
 * The code in the thread executes a simple loop,
 * waiting for the next block and executing the interpretation
 * operation for that block.
 * In our original code the block count was 1 higher than
 * our count here.
 */
void OfdmDecoder::workerthread(void)
{
    int16_t currentBlock = 0;

    running = true;

    while (running) {
        std::unique_lock<std::mutex> lock(mutex);
        commandHandler.wait_for(lock, std::chrono::milliseconds(100));

        if (currentBlock == 0) {
            constellationPoints.clear();
            constellationPoints.reserve(
                    params.L * params.K / constellationDecimation);
        }

        while (amount > 0 && running) {
            if (currentBlock == 0)
                processPRS();
            else
                if (currentBlock < 4)
                    decodeFICblock(currentBlock);
                else
                    decodeMscblock(currentBlock);
            currentBlock = (currentBlock + 1) % (params.L);
            amount -= 1;

            if (currentBlock == 0) {
                radioInterface.onConstellationPoints(
                        std::move(constellationPoints));
                constellationPoints.clear();
            }
        }
    }

    std::clog << "OFDM-decoder:" <<  "closing down now" << std::endl;
}

/**
 * We need some functions to enter the ofdmProcessor data
 * in the buffer.
 */
void OfdmDecoder::processPRS (DSPCOMPLEX *vi)
{
    std::unique_lock<std::mutex> lock(mutex);

    memcpy(command[0], vi, sizeof (DSPCOMPLEX) * params.T_u);
    amount++;
    commandHandler.notify_one();
}

void OfdmDecoder::decodeFICblock (DSPCOMPLEX *vi, int32_t blkno)
{
    std::unique_lock<std::mutex> lock(mutex);

    memcpy (command[blkno], &vi[T_g], sizeof (DSPCOMPLEX) * params.T_u);
    amount++;
    commandHandler.notify_one();
}

void OfdmDecoder::decodeMscblock (DSPCOMPLEX *vi, int32_t blkno)
{
    std::unique_lock<std::mutex> lock(mutex);

    memcpy (command[blkno], &vi[T_g], sizeof (DSPCOMPLEX) * params.T_u);
    amount++;
    commandHandler.notify_one();
}

/**
 * Note that the distinction, made in the ofdmProcessor class
 * does not add much here, iff we decide to choose the multi core
 * option definitely, then code may be simplified there.
 */

/**
 * handle block 0 as collected from the buffer
 */
void OfdmDecoder::processPRS (void)
{
    memcpy (fft_buffer, command[0], params.T_u * sizeof (DSPCOMPLEX));
    fft_handler.do_FFT ();
    /**
     * The SNR is determined by looking at a segment of bins
     * within the signal region and bits outside.
     * It is just an indication
     */
    snr = 0.7 * snr + 0.3 * get_snr (fft_buffer);
    if (++snrCount > 10) {
        radioInterface.onSNR(snr);
        snrCount = 0;
    }
    /**
     * we are now in the frequency domain, and we keep the carriers
     * as coming from the FFT as phase reference.
     */
    memcpy (phaseReference.data(), fft_buffer, params.T_u * sizeof (DSPCOMPLEX));
}

/**
 * for the other blocks of data, the first step is to go from
 * time to frequency domain, to get the carriers.
 * we distinguish between FIC blocks and other blocks,
 * only to spare a test. Tthe mapping code is the same
 *
 * \brief decodeFICblock
 * do the transforms and hand over the result to the fichandler
 */
void OfdmDecoder::decodeFICblock (int32_t blkno)
{
    memcpy (fft_buffer, command[blkno], params.T_u * sizeof (DSPCOMPLEX));
    //fftlabel:
    /**
     * first step: do the FFT
     */
    fft_handler.do_FFT();
    /**
     * a little optimization: we do not interchange the
     * positive/negative frequencies to their right positions.
     * The de-interleaving understands this
     */
    //toBitsLabel:
    /**
     * Note that from here on, we are only interested in the
     * "carriers" useful carriers of the FFT output
     */
    for (int16_t i = 0; i < params.K; i ++) {
        int16_t index = interleaver.mapIn(i);
        if (index < 0)
            index += params.T_u;
        /**
         * decoding is computing the phase difference between
         * carriers with the same index in subsequent blocks.
         * The carrier of a block is the reference for the carrier
         * on the same position in the next block
         */
        DSPCOMPLEX r1 = fft_buffer[index] * conj (phaseReference[index]);
        phaseReference[index] = fft_buffer[index];
        DSPFLOAT ab1 = l1_norm(r1);
        /// split the real and the imaginary part and scale it

        ibits[i]            =  - real (r1) / ab1 * 127.0;
        ibits[params.K + i] =  - imag (r1) / ab1 * 127.0;

        if (i % constellationDecimation == 0) {
            constellationPoints.push_back(r1);
        }
    }
    //handlerLabel:
    ficHandler.process_ficBlock(ibits.data(), blkno);
}

/**
 * Msc block decoding is equal to FIC block decoding,
 */
void OfdmDecoder::decodeMscblock (int32_t blkno)
{
    memcpy (fft_buffer, command[blkno], params.T_u * sizeof (DSPCOMPLEX));
    //fftLabel:
    fft_handler.do_FFT ();

    //  Note that "mapIn" maps to -carriers / 2 .. carriers / 2
    //  we did not set the fft output to low .. high
    //toBitsLabel:
    for (int16_t i = 0; i < params.K; i ++) {
        int16_t index = interleaver.mapIn(i);
        if (index < 0)
            index += params.T_u;

        DSPCOMPLEX   r1 = fft_buffer[index] * conj (phaseReference[index]);
        phaseReference[index] = fft_buffer[index];
        DSPFLOAT ab1 = l1_norm(r1);
        //  Recall:  the viterbi decoder wants 127 max pos, - 127 max neg
        //  we make the bits into softbits in the range -127 .. 127
        ibits[i]            =  - real (r1) / ab1 * 127.0;
        ibits[params.K + i] =  - imag (r1) / ab1 * 127.0;

        if (i % constellationDecimation == 0) {
            constellationPoints.push_back(r1);
        }
    }
    //handlerLabel:
    mscHandler.processMscBlock(ibits.data(), blkno);
}

/**
 * for the snr we have a full T_u wide vector, with in the middle
 * K carriers.
 * Just get the strength from the selected carriers compared
 * to the strength of the carriers outside that region
 */
int16_t OfdmDecoder::get_snr (DSPCOMPLEX *v)
{
    int16_t i;
    DSPFLOAT    noise   = 0;
    DSPFLOAT    signal  = 0;
    const auto T_u = params.T_u;
    int16_t low = T_u / 2 -  params.K / 2;
    int16_t high    = low + params.K;

    for (i = 10; i < low - 20; i ++)
        noise += abs (v[(T_u / 2 + i) % T_u]);

    for (i = high + 20; i < T_u - 10; i ++)
        noise += abs (v[(T_u / 2 + i) % T_u]);

    noise   /= (low - 30 + T_u - high - 30);
    for (i = T_u / 2 - params.K / 4;  i < T_u / 2 + params.K / 4; i ++)
        signal += abs (v[(T_u / 2 + i) % T_u]);

    return get_db_over_256(signal / (params.K / 2)) - get_db_over_256(noise);
}

