#include "Audio.h"
#include "arm_math.h"
#include "utility/dspinst.h"



/******************************************************************/


void AudioPrint::update(void)
{
	audio_block_t *block;
	uint32_t i;

	Serial.println("AudioPrint::update");
	Serial.println(name);
	block = receiveReadOnly();
	if (block) {
		for (i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
			Serial.print(block->data[i]);
			Serial.print(", ");
			if ((i % 12) == 11) Serial.println();
		}
		Serial.println();
		release(block);
	}
}


/******************************************************************/

//                A u d i o T o n e S w e e p
// Written by Pete (El Supremo) Feb 2014


boolean AudioToneSweep::begin(short t_amp,int t_lo,int t_hi,float t_time)
{
  double tone_tmp;
  
if(0) {
  Serial.print("AudioToneSweep.begin(tone_amp = ");
  Serial.print(t_amp);
  Serial.print(", tone_lo = ");
  Serial.print(t_lo);
  Serial.print(", tone_hi = ");
  Serial.print(t_hi);
  Serial.print(", tone_time = ");
  Serial.print(t_time,1);
  Serial.println(")");
}
  tone_amp = 0;
  if(t_amp < 0)return false;
  if(t_lo < 1)return false;
  if(t_hi < 1)return false;
  if(t_hi >= 44100/2)return false;
  if(t_lo >= 44100/2)return false;
  if(t_time < 0)return false;
  tone_lo = t_lo;
  tone_hi = t_hi;
  tone_phase = 0;

  tone_amp = t_amp;
  // Limit the output amplitude to prevent aliasing
  // until I can figure out why this "overtops"
  // above 29000.
  if(tone_amp > 29000)tone_amp = 29000;
  tone_tmp = tone_hi - tone_lo;
  tone_sign = 1;
  tone_freq = tone_lo*0x100000000LL;
  if(tone_tmp < 0) {
    tone_sign = -1;
    tone_tmp = -tone_tmp;
  }
  tone_tmp = tone_tmp/t_time/44100.;
  tone_incr = (tone_tmp * 0x100000000LL);
  sweep_busy = 1;
  return(true);
}



unsigned char AudioToneSweep::busy(void)
{
  return(sweep_busy);
}

int b_count = 0;
void AudioToneSweep::update(void)
{
  audio_block_t *block;
  short *bp;
  int i;
  
  if(!sweep_busy)return;

  //          L E F T  C H A N N E L  O N L Y
  block = allocate();
  if(block) {
    bp = block->data;
    // Generate the sweep
    for(i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
      *bp++ = (short)(( (short)(arm_sin_q31((uint32_t)((tone_phase >> 15)&0x7fffffff))>>16) *tone_amp) >> 16);
      uint64_t tone_tmp = (0x400000000000LL * (int)((tone_freq >> 32)&0x7fffffff))/44100;

      tone_phase +=  tone_tmp;
      if(tone_phase & 0x800000000000LL)tone_phase &= 0x7fffffffffffLL;

      if(tone_sign > 0) {
        if((tone_freq >> 32) > tone_hi) {
          sweep_busy = 0;
          break;
        }
        tone_freq += tone_incr;
      } else {
        if((tone_freq >> 32) < tone_hi) {
          sweep_busy = 0;

          break;
        }
        tone_freq -= tone_incr;        
      }
    }
    while(i < AUDIO_BLOCK_SAMPLES) {
      *bp++ = 0;
      i++;
    }
    b_count++;
    // send the samples to the left channel
    transmit(block,0);
    release(block);
  }
}
