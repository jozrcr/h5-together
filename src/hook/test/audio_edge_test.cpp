// Test of two small presentation mods' pure work (no game needed):
//   - the volume curve of src/mods/audio_volume.h ([audio] volume_curve=wide);
//   - the light file edit of src/mods/edge_sky.h ([camera] edge_sky=none).
#include <math.h>
#include <stdio.h>

#include <string>

#include "../../mods/audio_volume.h"
#include "../../mods/edge_sky.h"

using mods::audiovol::Wide;
using mods::edgesky::NormalizePath;
using mods::edgesky::WithoutSkyDome;

namespace {

int g_failures = 0;

void Check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		++g_failures;
}

bool Near(float a, float b)
{
	return fabsf(a - b) < 1e-5f;
}

// What FMOD gets: the game truncates v * 255 (0x00b26e80).
int Fmod(float v)
{
	return static_cast<int>(v * 255.0f);
}

void TestVolume()
{
	// the game's lowest step: the slider stores (int)(0.1 * 255) = 25
	Check(Near(Wide(25 / 255.0f, 25), (25 / 255.0f) * (25 / 255.0f)), "lowest step: the slider counts twice");
	Check(Fmod(Wide(25 / 255.0f, 25)) == 2, "lowest step: FMOD gets 2 instead of 25 (-42 dB instead of -20 dB)");
	Check(Near(Wide(0.5f, 255), 0.5f), "full slider: unchanged");
	Check(Near(Wide(0.8f, 300), 0.8f), "a slider above 255 counts as 255");
	Check(Wide(0.0f, 25) == 0.0f, "silent stays silent");
	Check(Near(Wide(0.3f, 0), 0.3f), "slider 0: left to the game (the volume is 0 then anyway)");
	Check(Near(Wide(0.3f, -5), 0.3f), "a broken slider value: left to the game");
	// a quiet sound at the lowest step stays audible
	const float quiet = 0.05f * 25 / 255.0f;  // own volume 5%: FMOD 1 with the game's curve
	Check(Fmod(quiet) == 1 && Fmod(Wide(quiet, 25)) == 1, "an audible sound stays audible (FMOD 1)");
	const float faint = 0.5f / 255.0f;  // FMOD 0 with the game's curve
	Check(Near(Wide(faint, 25), faint) && Fmod(Wide(faint, 25)) == 0, "an inaudible sound stays as it was");
	Check(Near(Wide(0.2f, 128), 0.2f * 128 / 255.0f), "middle: the volume times the slider");
	// every step is quieter or equal, never louder
	bool never_louder = true;
	for (int master = 0; master <= 255; ++master)
		for (int own = 0; own <= 100; own += 5) {
			const float v = (master / 255.0f) * (own / 100.0f);
			never_louder = never_louder && Wide(v, master) <= v + 1e-7f;
		}
	Check(never_louder, "never louder than the game");
}

void TestSky()
{
	const std::string mmh55 =
		"<AmbientLight ObjectRecordID=\"27\">\r\n"
		"\t<HeightFog/>\r\n"
		"\t<SkyDome href=\"/_(Model)/SkyDomes/SkyDome_Ramp_SphereCS.xdb#xpointer(/Model)\"/>\r\n"
		"\t<DymanicLightsModifications>\r\n";
	std::string out = "untouched";
	Check(WithoutSkyDome(mmh55, &out), "a light with a sky dome: changed");
	Check(out ==
	          "<AmbientLight ObjectRecordID=\"27\">\r\n"
	          "\t<HeightFog/>\r\n"
	          "\t<SkyDome/>\r\n"
	          "\t<DymanicLightsModifications>\r\n",
	      "only the sky dome element changes, to <SkyDome/>");
	out = "untouched";
	Check(!WithoutSkyDome("<AmbientLight>\r\n\t<SkyDome/>\r\n</AmbientLight>", &out) && out == "untouched",
	      "no sky dome (the original light): nothing to do");
	Check(!WithoutSkyDome("<SkyDomeColor href=\"x\"/><SkyDome>", &out), "longer names and an empty start tag: left alone");
	Check(WithoutSkyDome("a<SkyDome href=\"x\">\n<z/>\n</SkyDome>b", &out) && out == "a<SkyDome/>b",
	      "the long form with a closing tag");
	Check(!WithoutSkyDome("<SkyDome href=\"x\">no end", &out), "not well formed: left alone");
	Check(NormalizePath("/Lights\\_(AmbientLight)/Arena/Dusk_light_BoatArena01(VicTest).xdb#xpointer(/AmbientLight)") ==
	          "lights/_(ambientlight)/arena/dusk_light_boatarena01(victest).xdb",
	      "paths: slashes, case and the #xpointer part");
	Check(NormalizePath("") == "" && NormalizePath("#x") == "", "paths: empty");
}

}  // namespace

int main()
{
	TestVolume();
	TestSky();
	printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
	return g_failures ? 1 : 0;
}
