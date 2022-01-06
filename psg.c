#include "psg.h"

#include <math.h> /* TODO - Temporary */

#include "clowncommon.h"

void PSG_Init(PSG_State *state)
{
	unsigned int i;

	/* TODO - Temporary */
	for (i = 0; i < 0xF; ++i)
	{
		const int volume = (0x7FFF / 4) * powf(10.0f, -2.0f * i / 20.0f);

		state->volumes[i][0] = volume;
		state->volumes[i][1] = -volume;
	}

	state->volumes[0xF][0] = 0;
	state->volumes[0xF][1] = 0;

	for (i = 0; i < CC_COUNT_OF(state->tones); ++i)
	{
		state->tones[i].countdown = 0;
		state->tones[i].countdown_master = 0;
		state->tones[i].attenuation = 0xF;
		state->tones[i].output_bit = 0;
	}

	state->noise.countdown = 0;
	state->noise.attenuation = 0xF;
	state->noise.fake_output_bit = 0;
	state->noise.real_output_bit = 0;
	state->noise.frequency_mode = 0;
	state->noise.periodic_mode = 0;
	state->noise.shift_register = 0xFFFF;
}

void PSG_Update(PSG_State *state, short *sample_buffer, size_t total_samples)
{
	unsigned int i;
	size_t j;
	short *sample_buffer_pointer;

	for (i = 0; i < CC_COUNT_OF(state->tones); ++i)
	{
		PSG_ToneState *tone = &state->tones[i];

		sample_buffer_pointer = sample_buffer;

		for (j = 0; j < total_samples; ++j)
		{
			if (tone->countdown-- == 0)
			{
				tone->countdown = tone->countdown_master;

				tone->output_bit = !tone->output_bit;
			}

			*sample_buffer_pointer++ += state->volumes[tone->attenuation][tone->output_bit];
		}
	}

	sample_buffer_pointer = sample_buffer;

	for (j = 0; j < total_samples; ++j)
	{
		/* TODO - Periodic mode */
		if (state->noise.countdown-- == 0)
		{
			switch (state->noise.frequency_mode)
			{
				case 0:
					state->noise.countdown = 0x10;
					break;

				case 1:
					state->noise.countdown = 0x20;
					break;

				case 2:
					state->noise.countdown = 0x40;
					break;

				case 3:
					state->noise.countdown = state->tones[CC_COUNT_OF(state->tones) - 1].countdown_master;
					break;
			}

			state->noise.fake_output_bit = !state->noise.fake_output_bit;

			if (state->noise.fake_output_bit)
			{
				state->noise.real_output_bit = !!(state->noise.shift_register & 0x8000);

				state->noise.shift_register <<= 1;
				state->noise.shift_register |= state->noise.real_output_bit != !!(state->noise.shift_register & 0x2000);
			}
		}

		*sample_buffer_pointer++ += state->volumes[state->noise.attenuation][state->noise.real_output_bit];
	}
}

void PSG_DoCommand(PSG_State *state, unsigned int command)
{
	const cc_bool latch = !!(command & 0x80);

	if (latch)
	{
		/* Latch command */
		state->latched_command.channel = (command >> 5) & 3;
		state->latched_command.mode = command & 0x10;
	}

	if (state->latched_command.channel == CC_COUNT_OF(state->tones))
	{
		/* Noise channel */
		if (state->latched_command.mode)
		{
			/* Volume command */
			state->noise.attenuation = command & 0xF;
			/*state->noise.fake_output_bit = 0;*/
		}
		else
		{
			/* Frequency command */
			state->noise.frequency_mode = command & 3;
		}
	}
	else
	{
		/* Tone channel */
		PSG_ToneState *tone = &state->tones[state->latched_command.channel];

		if (state->latched_command.mode)
		{
			/* Volume command */
			tone->attenuation = command & 0xF;
			/* According to http://md.railgun.works/index.php?title=PSG, this should happen, but when I test it, I get crackly audio, so I've disabled it for now */
			/*tone->output_bit = 0;*/
		}
		else
		{
			/* Frequency command */
			if (latch)
			{
				/* Low frequency bits */
				tone->countdown_master &= ~0xF;
				tone->countdown_master |= command & 0xF;
			}
			else
			{
				/* High frequency bits */
				tone->countdown_master &= 0xF;
				tone->countdown_master |= (command & 0x3F) << 4;
			}
		}
	}
}