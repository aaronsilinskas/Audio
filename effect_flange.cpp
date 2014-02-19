#include "Audio.h"
#include "arm_math.h"
#include "utility/dspinst.h"




/******************************************************************/
//                A u d i o E f f e c t F l a n g e
// Written by Pete (El Supremo) Jan 2014
// 140207 - fix calculation of delay_rate_incr which is expressed as
//			a fraction of 2*PI
// 140207 - cosmetic fix to begin()

// circular addressing indices for left and right channels
short AudioEffectFlange::l_circ_idx;
short AudioEffectFlange::r_circ_idx;

short * AudioEffectFlange::l_delayline = NULL;
short * AudioEffectFlange::r_delayline = NULL;

// User-supplied offset for the delayed sample
// but start with passthru
int AudioEffectFlange::delay_offset_idx = DELAY_PASSTHRU;
int AudioEffectFlange::delay_length;

int AudioEffectFlange::delay_depth;
int AudioEffectFlange::delay_rate_incr;
unsigned int AudioEffectFlange::l_delay_rate_index;
unsigned int AudioEffectFlange::r_delay_rate_index;
// fails if the user provides unreasonable values but will
// coerce them and go ahead anyway. e.g. if the delay offset
// is >= CHORUS_DELAY_LENGTH, the code will force it to
// CHORUS_DELAY_LENGTH-1 and return false.
// delay_rate is the rate (in Hz) of the sine wave modulation
// delay_depth is the maximum variation around delay_offset
// i.e. the total offset is delay_offset + delay_depth * sin(delay_rate)
boolean AudioEffectFlange::begin(short *delayline,int d_length,int delay_offset,int d_depth,float delay_rate)
{
  boolean all_ok = true;

if(0) {
  Serial.print("AudioEffectFlange.begin(offset = ");
  Serial.print(delay_offset);
  Serial.print(", depth = ");
  Serial.print(d_depth);
  Serial.print(", rate = ");
  Serial.print(delay_rate,3);
  Serial.println(")");
  Serial.print("    FLANGE_DELAY_LENGTH = ");
  Serial.println(d_length);
}
  delay_length = d_length/2;
  l_delayline = delayline;
  r_delayline = delayline + delay_length;
  
  delay_depth = d_depth;
  // initial index
  l_delay_rate_index = 0;
  r_delay_rate_index = 0;
  l_circ_idx = 0;
  r_circ_idx = 0;
  delay_rate_incr = delay_rate/44100.*2147483648.; 
//Serial.println(delay_rate_incr,HEX);

  delay_offset_idx = delay_offset;
  // Allow the passthru code to go through
  if(delay_offset_idx < -1) {
    delay_offset_idx = 0;
    all_ok = false;
  }
  if(delay_offset_idx >= delay_length) {
    delay_offset_idx = delay_length - 1;
    all_ok = false;
  }  
  return(all_ok);
}


boolean AudioEffectFlange::modify(int delay_offset,int d_depth,float delay_rate)
{
  boolean all_ok = true;
  
  delay_depth = d_depth;

  delay_rate_incr = delay_rate/44100.*2147483648.;
  
  delay_offset_idx = delay_offset;
  // Allow the passthru code to go through
  if(delay_offset_idx < -1) {
    delay_offset_idx = 0;
    all_ok = false;
  }
  if(delay_offset_idx >= delay_length) {
    delay_offset_idx = delay_length - 1;
    all_ok = false;
  }
  l_delay_rate_index = 0;
  r_delay_rate_index = 0;
  l_circ_idx = 0;
  r_circ_idx = 0;
  return(all_ok);
}

void AudioEffectFlange::update(void)
{
  audio_block_t *block;
  int idx;
  short *bp;
  short frac;
  int idx1;

  if(l_delayline == NULL)return;
  if(r_delayline == NULL)return; 

  // do passthru
  if(delay_offset_idx == DELAY_PASSTHRU) {
    // Just passthrough
    block = receiveWritable(0);
    if(block) {
      bp = block->data;
      for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
        l_circ_idx++;
        if(l_circ_idx >= delay_length) {
          l_circ_idx = 0;
        }
        l_delayline[l_circ_idx] = *bp++;
      }
      transmit(block,0);
      release(block);
    }
    block = receiveWritable(1);
    if(block) {
      bp = block->data;
      for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
        r_circ_idx++;
        if(r_circ_idx >= delay_length) {
          r_circ_idx = 0;
        }
        r_delayline[r_circ_idx] = *bp++;
      }
      transmit(block,1);
      release(block);
    }
    return;
  }

  //          L E F T  C H A N N E L

  block = receiveWritable(0);
  if(block) {
    bp = block->data;
    for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
      l_circ_idx++;
      if(l_circ_idx >= delay_length) {
        l_circ_idx = 0;
      }
      l_delayline[l_circ_idx] = *bp;
      idx = arm_sin_q15( (q15_t)((l_delay_rate_index >> 16) & 0x7fff));
      idx = (idx * delay_depth) >> 15;
//Serial.println(idx);
      idx = l_circ_idx - (delay_offset_idx + idx);
      if(idx < 0) {
        idx += delay_length;
      }
      if(idx >= delay_length) {
        idx -= delay_length;
      }

      if(frac < 0)
        idx1 = idx - 1;
      else
        idx1 = idx + 1;
      if(idx1 < 0) {
        idx1 += delay_length;
      }
      if(idx1 >= delay_length) {
        idx1 -= delay_length;
      }
      frac = (l_delay_rate_index >> 1) &0x7fff;
      frac = (( (int)(l_delayline[idx1] - l_delayline[idx])*frac) >> 15);

      *bp++ = (l_delayline[l_circ_idx]
                + l_delayline[idx] + frac               
              )/2;

      l_delay_rate_index += delay_rate_incr;
      if(l_delay_rate_index & 0x80000000) {
        l_delay_rate_index &= 0x7fffffff;
      }
    }
    // send the effect output to the left channel
    transmit(block,0);
    release(block);
  }

  //          R I G H T  C H A N N E L

  block = receiveWritable(1);
  if(block) {
    bp = block->data;
    for(int i = 0;i < AUDIO_BLOCK_SAMPLES;i++) {
      r_circ_idx++;
      if(r_circ_idx >= delay_length) {
        r_circ_idx = 0;
      }
      r_delayline[r_circ_idx] = *bp;
      idx = arm_sin_q15( (q15_t)((r_delay_rate_index >> 16)&0x7fff));
       idx = (idx * delay_depth) >> 15;

      idx = r_circ_idx - (delay_offset_idx + idx);
      if(idx < 0) {
        idx += delay_length;
      }
      if(idx >= delay_length) {
        idx -= delay_length;
      }

      if(frac < 0)
        idx1 = idx - 1;
      else
        idx1 = idx + 1;
      if(idx1 < 0) {
        idx1 += delay_length;
      }
      if(idx1 >= delay_length) {
        idx1 -= delay_length;
      }
      frac = (r_delay_rate_index >> 1) &0x7fff;
      frac = (( (int)(r_delayline[idx1] - r_delayline[idx])*frac) >> 15);

      *bp++ = (r_delayline[r_circ_idx]
                + r_delayline[idx] + frac
               )/2;

      r_delay_rate_index += delay_rate_incr;
      if(r_delay_rate_index & 0x80000000) {
        r_delay_rate_index &= 0x7fffffff;
      }

    }
    // send the effect output to the right channel
    transmit(block,1);
    release(block);
  }
}



