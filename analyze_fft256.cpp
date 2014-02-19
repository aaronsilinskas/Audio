#include "Audio.h"
#include "arm_math.h"
#include "utility/dspinst.h"



static arm_cfft_radix4_instance_q15 fft_inst;

void AudioAnalyzeFFT256::init(void)
{
	// TODO: replace this with static const version
	arm_cfft_radix4_init_q15(&fft_inst, 256, 0, 1);

	//for (int i=0; i<2048; i++) {
		//buffer[i] = i * 3;
	//}
	//__disable_irq();
	//ARM_DEMCR |= ARM_DEMCR_TRCENA;
	//ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
	//uint32_t n = ARM_DWT_CYCCNT;
	//arm_cfft_radix2_q15(&fft_inst, buffer);
	//n = ARM_DWT_CYCCNT - n;
	//__enable_irq();
	//cycles = n;
	//arm_cmplx_mag_q15(buffer, buffer, 512);

	// each audio block is 278525 cycles @ 96 MHz
	// 256 point fft2 takes 65408 cycles
	// 256 point fft4 takes 49108 cycles
	// 128 point cmag takes 10999 cycles
	// 1024 point fft2 takes 125948 cycles
	// 1024 point fft4 takes 125840 cycles
	// 512 point cmag takes 43764 cycles

	//state = 0;
	//outputflag = false;
}

static void copy_to_fft_buffer(void *destination, const void *source)
{
	const int16_t *src = (const int16_t *)source;
	int16_t *dst = (int16_t *)destination;

	// TODO: optimize this
	for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
		*dst++ = *src++;  // real
		*dst++ = 0;       // imaginary
	}
}

static void apply_window_to_fft_buffer(void *buffer, const void *window)
{
	int16_t *buf = (int16_t *)buffer;
	const int16_t *win = (int16_t *)window;;

	for (int i=0; i < 256; i++) {
		int32_t val = *buf * *win++;
		//*buf = signed_saturate_rshift(val, 16, 15);
		*buf = val >> 15;
		buf += 2;
	}

}

void AudioAnalyzeFFT256::update(void)
{
	audio_block_t *block;

	block = receiveReadOnly();
	if (!block) return;
	if (!prevblock) {
		prevblock = block;
		return;
	}
	copy_to_fft_buffer(buffer, prevblock->data);
	copy_to_fft_buffer(buffer+256, block->data);
	//window = AudioWindowBlackmanNuttall256;
	//window = NULL;
	if (window) apply_window_to_fft_buffer(buffer, window);
	arm_cfft_radix4_q15(&fft_inst, buffer);
	// TODO: is this averaging correct?  G. Heinzel's paper says we're
	// supposed to average the magnitude squared, then do the square
	// root at the end after dividing by naverage.
	arm_cmplx_mag_q15(buffer, buffer, 128);
	if (count == 0) {
		for (int i=0; i < 128; i++) {
			output[i] = buffer[i];
		}
	} else {
		for (int i=0; i < 128; i++) {
			output[i] += buffer[i];
		}
	}
	if (++count == naverage) {
		count = 0;
		for (int i=0; i < 128; i++) {
			output[i] /= naverage;
		}
		outputflag = true;
	}

	release(prevblock);
	prevblock = block;

#if 0
	block = receiveReadOnly();
	if (state == 0) {
		//Serial.print("0");
		if (block == NULL) return;
		copy_to_fft_buffer(buffer, block->data);
		state = 1;
	} else if (state == 1) {
		//Serial.print("1");
		if (block == NULL) return;
		copy_to_fft_buffer(buffer+256, block->data);
		arm_cfft_radix4_q15(&fft_inst, buffer);
		state = 2;
	} else {
		//Serial.print("2");
		arm_cmplx_mag_q15(buffer, output, 128);
		outputflag = true;
		if (block == NULL) return;
		copy_to_fft_buffer(buffer, block->data);
		state = 1;
	}
	release(block);
#endif
}


