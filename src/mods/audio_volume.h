// Mod: a wider range for the game's volume sliders (see audio_volume.cpp).
#pragma once

namespace mods {

// With the settings of open-homm5.ini [audio]: volume_curve=game (the default:
// the game's own sliders, and then nothing is installed) or wide; log_volume=N
// (testing: log the first N changes of an audible channel volume). Only after DetectGameBuild() said
// the build is hookable.
void ApplyAudioVolumeFromSettings();

namespace audiovol {

// FMOD's quietest audible volume as a fraction. The game turns 0..1 into
// FMOD's 0..255 by truncating v * 255, so 1/255 could truncate to 0; 1.5/255
// gives 1.
const float kQuietest = 1.5f / 255.0f;

// The volume a channel gets with volume_curve=wide.
//   volume  what the game computed for the channel (0..1): its slider's value
//           times the channel's own factors (the sound's volume, its group, the
//           3D distance);
//   master  that slider's stored value, 0..255 (the slider's 0..1 times 255).
// The game's straight factor master/255 becomes (master/255)^2, so the result
// is volume * master / 255: every slider step is then about the same number of
// decibels (10% -> -40 dB instead of -20 dB). A channel the game makes audible
// stays audible (at FMOD's quietest step), so quiet sounds do not vanish at the
// lowest steps.
inline float Wide(float volume, int master)
{
	if (!(volume > 0.0f) || master <= 0)
		return volume;  // silent anyway (or not a number: left to the game)
	if (master > 255)
		master = 255;
	float out = volume * (static_cast<float>(master) / 255.0f);
	const float floor = volume < kQuietest ? volume : kQuietest;
	if (out < floor)
		out = floor;
	return out;
}

}  // namespace audiovol
}  // namespace mods
