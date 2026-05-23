/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef AudioStream_h
#define AudioStream_h

#ifndef __ASSEMBLER__
#include <Arduino.h>
#include <stdio.h>  // for NULL
#include <string.h> // for memcpy
#include "enums.h"
#endif

#if defined(ARDUINO_RASPBERRY_PI_PICO_2) || defined(ARDUINO_RASPBERRY_PI_PICO_2W)
#define __IMXRT1062__  // pretend this is a Teensy 4.x
#define FLASHMEM
#define ARM_DWT_CYCCNT (m33_hw->dwt_cyccnt)
#define __disable_irq(...) noInterrupts()
#define __enable_irq(...)    interrupts()
#define F_CPU_ACTUAL (rp2040.f_cpu())
#endif // defined(ARDUINO_RASPBERRY_PI_PICO_2)


// AUDIO_BLOCK_SAMPLES determines how many samples the audio library processes
// per update.  It may be reduced to achieve lower latency response to events,
// at the expense of higher interrupt and DMA setup overhead.
//
// Less than 32 may not work with some input & output objects.  Multiples of 16
// should be used, since some synthesis objects generate 16 samples per loop.
//
// Some parts of the audio library may have hard-coded dependency on 128 samples.
// Please report these on the forum with reproducible test cases.  The following
// audio classes are known to have problems with smaller block sizes:
//   AudioInputUSB, AudioOutputUSB, AudioPlaySdWav, AudioAnalyzeFFT256,
//   AudioAnalyzeFFT1024


#define noAUDIO_DEBUG_CLASS // disable this class by default

#if defined(__IMXRT1062__)
#define MAX_AUDIO_MEMORY 229376
#endif

#define NUM_MASKS (((MAX_AUDIO_MEMORY / AUDIO_BLOCK_SAMPLES / 2) + 31) / 32)

#ifndef __ASSEMBLER__
class AudioStream;
class AudioConnection;
#if defined(AUDIO_DEBUG_CLASS)
class AudioDebug;  // for testing only, never for public release
#endif // defined(AUDIO_DEBUG_CLASS)

typedef struct audio_block_struct {
	uint8_t  ref_count;
	uint8_t  reserved1;
	uint16_t memory_pool_index;
	int16_t  data[AUDIO_BLOCK_SAMPLES];
} audio_block_t;

inline void software_isr(void);

class AudioConnection{
public:
	AudioConnection();
	AudioConnection(AudioStream &source, AudioStream &destination)
		: AudioConnection() { connect(source,destination); }
	AudioConnection(AudioStream &source, unsigned char sourceOutput,
		AudioStream &destination, unsigned char destinationInput)
		: AudioConnection() { connect(source,sourceOutput, destination,destinationInput); }
	friend class AudioStream;
	~AudioConnection(); 
	int disconnect(void);
	int connect(void);
	int connect(AudioStream &source, AudioStream &destination) {return connect(source,0,destination,0);};
	int connect(AudioStream &source, unsigned char sourceOutput,
		AudioStream &destination, unsigned char destinationInput);
protected:
	AudioStream* src;	// can't use references as... 
	AudioStream* dst;	// ...they can't be re-assigned!
	unsigned char src_index;
	unsigned char dest_index;
	AudioConnection *next_dest; // linked list of connections from one source
	bool isConnected;
#if defined(AUDIO_DEBUG_CLASS)
	friend class AudioDebug;
#endif // defined(AUDIO_DEBUG_CLASS)
};


#define AudioMemory(num) ({ \
	static audio_block_t data[num]; \
	AudioStream::initialize_memory(data, num); \
})

#define CYCLE_COUNTER_APPROX_PERCENT(n) (((float)((uint32_t)(n) * 6400u) * (float)(AUDIO_SAMPLE_RATE_EXACT / AUDIO_BLOCK_SAMPLES)) / (float)(F_CPU_ACTUAL))

#define AudioProcessorUsage() (CYCLE_COUNTER_APPROX_PERCENT(AudioStream::cpu_cycles_total))
#define AudioProcessorUsageMax() (CYCLE_COUNTER_APPROX_PERCENT(AudioStream::cpu_cycles_total_max))
#define AudioProcessorUsageMaxReset() (AudioStream::cpu_cycles_total_max = AudioStream::cpu_cycles_total)
#define AudioMemoryUsage() (AudioStream::memory_used)
#define AudioMemoryUsageMax() (AudioStream::memory_used_max)
#define AudioMemoryUsageMaxReset() (AudioStream::memory_used_max = AudioStream::memory_used)

class AudioStream
{
public:
	AudioStream(unsigned char ninput, audio_block_t **iqueue) :
		num_inputs(ninput), inputQueue(iqueue)
		{
			active = false;
			destination_list = NULL;
			for (int i=0; i < num_inputs; i++) {
				inputQueue[i] = NULL;
			}
			// add to a simple list, for update_all
			// TODO: replace with a proper data flow analysis in update_all
			if (first_update == NULL) {
				first_update = this;
			} else {
				AudioStream *p;
				for (p=first_update; p->next_update; p = p->next_update) ;
				p->next_update = this;
			}
			next_update = NULL;
			cpu_cycles = 0;
			cpu_cycles_max = 0;
			numConnections = 0;
		}
	static void initialize_memory(audio_block_t *data, unsigned int num);
	float processorUsage(void) { return CYCLE_COUNTER_APPROX_PERCENT(cpu_cycles); }
	float processorUsageMax(void) { return CYCLE_COUNTER_APPROX_PERCENT(cpu_cycles_max); }
	void processorUsageMaxReset(void) { cpu_cycles_max = cpu_cycles; }
	bool isActive(void) { return active; }
	uint16_t cpu_cycles;
	uint16_t cpu_cycles_max;
	static uint16_t cpu_cycles_total;
	static uint16_t cpu_cycles_total_max;
	static uint16_t memory_used;
	static uint16_t memory_used_max;
protected:
	bool active;
	unsigned char num_inputs;
	static audio_block_t * allocate(void);
	static void release(audio_block_t * block);
	void transmit(audio_block_t *block, unsigned char index = 0);
	audio_block_t * receiveReadOnly(unsigned int index = 0);
	audio_block_t * receiveWritable(unsigned int index = 0);
	static bool update_setup(void);
	static void update_stop(void);
	static void update_all(void);
	friend void I2S_Transmitted(void); // used to trigger update_all()
	friend void software_isr(void);
	static int user_irq_num;
	friend class AudioConnection;
#if defined(AUDIO_DEBUG_CLASS)
	friend class AudioDebug;
#endif // defined(AUDIO_DEBUG_CLASS)
	uint8_t numConnections;
private:
	static AudioConnection* unused; // linked list of unused but not destructed connections
	AudioConnection *destination_list;
	audio_block_t **inputQueue;
	static bool update_scheduled;
	virtual void update(void) = 0;
	static AudioStream *first_update; // for update_all
	AudioStream *next_update; // for update_all
	static audio_block_t *memory_pool;
	static uint32_t memory_pool_available_mask[];
	static uint16_t memory_pool_first_mask;
};

// Static member variable definitions
inline audio_block_t *AudioStream::memory_pool;
inline uint32_t AudioStream::memory_pool_available_mask[NUM_MASKS];
inline uint16_t AudioStream::memory_pool_first_mask;

inline uint16_t AudioStream::cpu_cycles_total = 0;
inline uint16_t AudioStream::cpu_cycles_total_max = 0;
inline uint16_t AudioStream::memory_used = 0;
inline uint16_t AudioStream::memory_used_max = 0;
inline AudioConnection *AudioStream::unused = NULL;
inline int AudioStream::user_irq_num = -1;
inline bool AudioStream::update_scheduled = false;
inline AudioStream *AudioStream::first_update = NULL;

// AudioStream implementations
FLASHMEM inline void AudioStream::initialize_memory(audio_block_t *data, unsigned int num) 
{
  unsigned int i;
  unsigned int maxnum = MAX_AUDIO_MEMORY / AUDIO_BLOCK_SAMPLES / 2;

  //Serial.println("AudioStream initialize_memory");
  //delay(10);
  if (num > maxnum) num = maxnum;
//  __disable_irq();
  noInterrupts();
  memory_pool = data;
  memory_pool_first_mask = 0;
  for (i = 0; i < NUM_MASKS; i++) {
    memory_pool_available_mask[i] = 0;
  }
  for (i = 0; i < num; i++) {
    memory_pool_available_mask[i >> 5] |= (1 << (i & 0x1F));
  }
  for (i = 0; i < num; i++) {
    data[i].memory_pool_index = i;
  }
 __enable_irq();
 
  // Pico doesn't enable cycle counter by default, but
  // we need it to compute rough CPU usage:
  m33_hw->dwt_ctrl |= M33_DWT_CTRL_CYCCNTENA_BITS;
  m33_hw->demcr    |= M33_DEMCR_TRCENA_BITS;
}

inline audio_block_t * AudioStream::allocate(void)
{
  uint32_t n, index, avail;
  uint32_t *p, *end;
  audio_block_t *block;
  uint32_t used;

  p = memory_pool_available_mask;
  end = p + NUM_MASKS;
	__disable_irq();
  index = memory_pool_first_mask;
  p += index;
  while (1) {
    if (p >= end) {
			__enable_irq();
      //Serial.println("alloc:null");
      return NULL;
    }
    avail = *p;
    if (avail) break;
    index++;
    p++;
  }
  n = __builtin_clz(avail);
  avail &= ~(0x80000000 >> n);
  *p = avail;
  if (!avail) index++;
  memory_pool_first_mask = index;
  used = memory_used + 1;
  memory_used = used;
	__enable_irq();
  index = p - memory_pool_available_mask;
  block = memory_pool + ((index << 5) + (31 - n));
  block->ref_count = 1;
  if (used > memory_used_max) memory_used_max = used;
  //Serial.print("alloc:");
  //Serial.println((uint32_t)block, HEX);
  return block;
}

inline void AudioStream::release(audio_block_t *block)
{
  //if (block == NULL) return;
  uint32_t mask = (0x80000000 >> (31 - (block->memory_pool_index & 0x1F)));
  uint32_t index = block->memory_pool_index >> 5;

	__disable_irq();
  if (block->ref_count > 1) {
    block->ref_count--;
  } else {
    //Serial.print("reles:");
    //Serial.println((uint32_t)block, HEX);
    memory_pool_available_mask[index] |= mask;
    if (index < memory_pool_first_mask) memory_pool_first_mask = index;
    memory_used--;
  }
	__enable_irq();
}

inline void AudioStream::transmit(audio_block_t *block, unsigned char index)
{
  for (AudioConnection *c = destination_list; c != NULL; c = c->next_dest) {
    if (c->src_index == index) {
      if (c->dst->inputQueue[c->dest_index] == NULL) {
        c->dst->inputQueue[c->dest_index] = block;
        block->ref_count++;
      }
    }
  }
}

inline audio_block_t * AudioStream::receiveReadOnly(unsigned int index)
{
  audio_block_t *in;

  if (index >= num_inputs) return NULL;
  in = inputQueue[index];
  inputQueue[index] = NULL;
  return in;
}

inline audio_block_t * AudioStream::receiveWritable(unsigned int index)
{
  audio_block_t *in, *p;

  if (index >= num_inputs) return NULL;
  in = inputQueue[index];
  inputQueue[index] = NULL;
  if (in && in->ref_count > 1) {
    p = allocate();
    if (p) memcpy(p->data, in->data, sizeof(p->data));
    in->ref_count--;
    in = p;
  }
  return in;
}

inline bool AudioStream::update_setup(void) {
  if (update_scheduled) return false;
  
#define AUDIO_IRQ_PRIORITY 0xD0 // only 4 bits significant on RP2350
	int tmp = user_irq_claim_unused(false);
	if (tmp >= 0)
	{
		irq_set_priority(tmp,AUDIO_IRQ_PRIORITY);
		irq_set_exclusive_handler(tmp,software_isr);
		irq_set_enabled(tmp,true);
		user_irq_num = tmp; // do last in case buffer empties during setup!
	}

// #define UPDATE_PIN 14
// pinMode(UPDATE_PIN,OUTPUT);

  update_scheduled = true;
  return true;
}

inline void AudioStream::update_stop(void) 
{
	int tmp = user_irq_num;

	user_irq_num = -1; // do first in case buffer empties during teardown!
	irq_set_enabled(tmp,false);
	irq_remove_handler(tmp,software_isr);
	user_irq_unclaim(tmp);
	
	update_scheduled = false;
}

inline void AudioStream::update_all()
{
	AudioStream *p;

	uint32_t totalcycles = ARM_DWT_CYCCNT;
	//digitalWriteFast(2, HIGH);
	for (p = AudioStream::first_update; p; p = p->next_update) {
		if (p->active) {
			uint32_t cycles = ARM_DWT_CYCCNT;
			p->update();
			// TODO: traverse inputQueueArray and release
			// any input blocks that weren't consumed?
			cycles = (ARM_DWT_CYCCNT - cycles) >> 6;
			p->cpu_cycles = cycles;
			if (cycles > p->cpu_cycles_max) p->cpu_cycles_max = cycles;
		}
	}
	//digitalWriteFast(2, LOW);
	totalcycles = (ARM_DWT_CYCCNT - totalcycles) >> 6;
	AudioStream::cpu_cycles_total = totalcycles;
	if (totalcycles > AudioStream::cpu_cycles_total_max)
		AudioStream::cpu_cycles_total_max = totalcycles;

  // asm("DSB");
// digitalWrite(UPDATE_PIN,0);	  
// digitalWrite(UPDATE_PIN,0);	  
}

inline void software_isr(void)
{
	AudioStream::update_all(); // sync updates with buffer transfer
}

inline void I2S_Transmitted(void)
{
/*
#define UPDATE_I2S 15
	static bool ps = false;
	digitalWrite(UPDATE_I2S, ps);
	ps = !ps;
*/
	
	if (AudioStream::user_irq_num >= 0)
		irq_set_pending(AudioStream::user_irq_num);
}

// AudioConnection implementations
inline AudioConnection::AudioConnection()
  : src(NULL), dst(NULL),
    src_index(0), dest_index(0),
    isConnected(false)

{
  // we are unused right now, so
  // link ourselves at the start of the unused list
  next_dest = AudioStream::unused;
  AudioStream::unused = this;
}

inline AudioConnection::~AudioConnection()
{
  AudioConnection **pp;

  disconnect();  // disconnect ourselves: puts us on the unused list
  // Remove ourselves from the unused list
  pp = &AudioStream::unused;
  while (*pp && *pp != this)
    pp = &((*pp)->next_dest);
  if (*pp)            // found ourselves
    *pp = next_dest;  // remove ourselves from the unused list
}

inline int AudioConnection::connect(void)
{
  int result = 1;
  AudioConnection *p;
  AudioConnection **pp;
  AudioStream *s;

	do 
	{
    if (isConnected)  // already connected
    {
      break;
    }

    if (!src || !dst)  // NULL src or dst - [old] Stream object destroyed
    {
      result = 3;
      break;
    }

    if (dest_index >= dst->num_inputs)  // input number too high
    {
      result = 2;
      break;
    }

	//	__disable_irq();
	  noInterrupts();

    // First check the destination's input isn't already in use
    s = AudioStream::first_update;  // first AudioStream in the stream list
    while (s)                       // go through all AudioStream objects
    {
      p = s->destination_list;  // first patchCord in this stream's list
			while (p)
			{
				if (p->dst == dst && p->dest_index == dest_index) // same destination - it's in use!
				{
					__enable_irq();
          return 4;
        }
        p = p->next_dest;
      }
      s = s->next_update;
    }

    // Check we're on the unused list
    pp = &AudioStream::unused;
		while (*pp && *pp != this)
		{
      pp = &((*pp)->next_dest);
    }
    if (!*pp)  // never found ourselves - fail
    {
      result = 5;
      break;
    }

    // Now try to add this connection to the source's destination list
    p = src->destination_list;  // first AudioConnection
		if (p == NULL) 
		{
			src->destination_list = this;
		} 
		else 
		{
      while (p->next_dest)  // scan source Stream's connection list for duplicates
      {

        if (&p->src == &this->src && &p->dst == &this->dst
					&& p->src_index == this->src_index && p->dest_index == this->dest_index) 
				{
					//Source and destination already connected through another connection, abort
					__enable_irq();
          return 6;
        }
        p = p->next_dest;
      }

      p->next_dest = this;  // end of list, can link ourselves in
    }

    *pp = next_dest;   // remove ourselves from the unused list
    next_dest = NULL;  // we're last in the source's destination list

    src->numConnections++;
    src->active = true;

    dst->numConnections++;
    dst->active = true;

    isConnected = true;

    result = 0;
  } while (0);

	__enable_irq();

  return result;
}

inline int AudioConnection::connect(AudioStream &source, unsigned char sourceOutput,
		AudioStream &destination, unsigned char destinationInput)
{
	int result = 1;
	
	if (!isConnected)
	{
    src = &source;
    dst = &destination;
    src_index = sourceOutput;
    dest_index = destinationInput;

    result = connect();
  }
  return result;
}

inline int AudioConnection::disconnect(void)
{
  AudioConnection *p;

  if (!isConnected) return 1;
  if (dest_index >= dst->num_inputs) return 2;  // should never happen!
	__disable_irq();

  // Remove destination from source list
  p = src->destination_list;
  if (p == NULL) {
    //>>> PAH re-enable the IRQ
		__enable_irq();
    return 3;
  } else if (p == this) {
    if (p->next_dest) {
      src->destination_list = next_dest;
    } else {
      src->destination_list = NULL;
    }
  } else {
		while (p)
		{
      if (p->next_dest == this)  // found the parent of the disconnecting object
      {
        p->next_dest = this->next_dest;  // skip parent's link past us
        break;
			}
			else
        p = p->next_dest;  // carry on down the list
    }
  }
  //>>> PAH release the audio buffer properly
  //Remove possible pending src block from destination
  if (dst->inputQueue[dest_index] != NULL) {
    AudioStream::release(dst->inputQueue[dest_index]);
    // release() re-enables the IRQ. Need it to be disabled a little longer
		__disable_irq();
    dst->inputQueue[dest_index] = NULL;
  }

  //Check if the disconnected AudioStream objects should still be active
  src->numConnections--;
  if (src->numConnections == 0) {
    src->active = false;
  }

  dst->numConnections--;
  if (dst->numConnections == 0) {
    dst->active = false;
  }

  isConnected = false;
  next_dest = dst->unused;
  dst->unused = this;

	__enable_irq();

  return 0;
}

#if defined(AUDIO_DEBUG_CLASS)
class AudioDebug
{
	public:
		// info on connections
		AudioStream* getSrc(AudioConnection& c) { return c.src;};
		AudioStream* getDst(AudioConnection& c) { return c.dst;};
		unsigned char getSrcN(AudioConnection& c) { return c.src_index;};
		unsigned char getDstN(AudioConnection& c) { return c.dest_index;};
		AudioConnection* getNext(AudioConnection& c) { return c.next_dest;};
		bool isConnected(AudioConnection& c) { return c.isConnected;};
		AudioConnection* unusedList() { return AudioStream::unused;};
		
		// info on streams
		AudioConnection* dstList(AudioStream& s) { return s.destination_list;};
		audio_block_t ** inqList(AudioStream& s) { return s.inputQueue;};
		uint8_t 	 	 getNumInputs(AudioStream& s) { return s.num_inputs;};
		AudioStream*     firstUpdate(AudioStream& s) { return s.first_update;};
		AudioStream* 	 nextUpdate(AudioStream& s) { return s.next_update;};
		uint8_t 	 	 getNumConnections(AudioStream& s) { return s.numConnections;};
		bool 	 	 	 isActive(AudioStream& s) { return s.active;};
		 
		
};
#endif // defined(AUDIO_DEBUG_CLASS)

#endif // __ASSEMBLER__
#endif // AudioStream_h