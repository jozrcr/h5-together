// The template builder's data (see rmg_builder_data.h).
#include "rmg_builder_data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "rmg_recipe.h"

#include "../hook/texts_logic.h"

namespace mods {
namespace rmg {
namespace builder {

// The 24 ready templates, in make_templates.js's order. The recipe of each is
// the ready file's choices (tools/rmg/README.md, "The ready templates"); the
// test checks that each builds that name.
const Preset kPresets[] = {
	{"OH-X1-221110nnxxxxxxxa", "OH-2v2-Lu-Normal"},
	{"OH-X1-221111nnxxxxxxxa", "OH-2v2-Lu-Normal-Towns"},
	{"OH-X1-221220rnxxxxxxxa", "OH-2v2-Lu-Rich"},
	{"OH-X1-221221rnxxxxxxxa", "OH-2v2-Lu-Rich-Towns"},
	{"OH-X1-321110nnxxxxxxxa", "OH-2v2v2-Lu-Normal"},
	{"OH-X1-321111nnxxxxxxxa", "OH-2v2v2-Lu-Normal-Towns"},
	{"OH-X1-321210rnxxxxxxxa", "OH-2v2v2-XLu-Rich"},
	{"OH-X1-321211rnxxxxxxxa", "OH-2v2v2-XLu-Rich-Towns"},
	{"OH-X1-231110nnxxxxxxxa", "OH-3v3-Lu-Normal"},
	{"OH-X1-231220rnxxxxxxxa", "OH-3v3-XLu-Rich"},
	{"OH-X1-231221vnxxxxxxxa", "OH-3v3-XLu-Veryrich-Towns"},
	{"OH-X1-241110nnxxxxxxxa", "OH-4v4-Lu-Normal"},
	{"OH-X1-241111nnxxxxxxxa", "OH-4v4-Lu-Normal-Towns"},
	{"OH-X1-242220rnxxxxxxxa", "OH-4v4-XLu-Rich"},
	{"OH-X1-242221rnxxxxxxxa", "OH-4v4-XLu-Rich-Towns"},
	{"OH-X1-311110nnxxxxxxxa", "OH-3P-FFA-Lu-Normal"},
	{"OH-X1-311110rnxxxxxxxa", "OH-3P-FFA-Lu-Rich"},
	{"OH-X1-411210nnxxxxxxxa", "OH-4P-FFA-XLu-Normal"},
	{"OH-X1-411210rnxxxxxxxa", "OH-4P-FFA-XLu-Rich"},
	{"OH-X1-611110nnxxxxxxxa", "OH-6P-FFA-XLu-Normal"},
	{"OH-X1-611111nnxxxxxxxa", "OH-6P-FFA-XLu-Normal-Towns"},
	{"OH-X1-611110rnxxxxxxxa", "OH-6P-FFA-XLu-Rich"},
	{"OH-X1-811110nnxxxxxxxa", "OH-8P-FFA-Hu-Normal"},
	{"OH-X1-811110rnxxxxxxxa", "OH-8P-FFA-Hu-Rich"},
};
const int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

int PresetOf(const Choices &choices)
{
	std::string recipe;
	if (!EncodeRecipe(choices, &recipe))
		return -1;
	for (int i = 0; i < kPresetCount; ++i)
		if (recipe == kPresets[i].recipe)
			return i;
	return -1;
}

const char *const kLootKeys[4] = {"poor", "normal", "rich", "veryrich"};
const char *const kGuardKeys[3] = {"weak", "normal", "strong"};

// Short words: they must fit the game's 1024x768 layout (the panel measures
// them and cuts with "..." only as a last resort).
const Words kKeys = {
	"builder.title",
	"builder.start_from", "builder.custom",
	"builder.section.teams", "builder.teams", "builder.per_team", "builder.ally_path", {"builder.ally.weak", "builder.ally.open"},
	"builder.mines", {"builder.mines.all", "builder.mines.woodore"},
	"builder.section.neutral", "builder.team_zones", "builder.border_zones", "builder.centre_zones", "builder.border_towns",
	"builder.team_towns",
	"builder.section.loot", "builder.loot", {"builder.loot.poor", "builder.loot.normal", "builder.loot.rich", "builder.loot.veryrich"},
	"builder.guards", {"builder.guards.weak", "builder.guards.normal", "builder.guards.strong"},
	"builder.by_distance", "builder.same", {"builder.ring.start", "builder.ring.next", "builder.ring.two", "builder.ring.further"},
	"builder.section.map", "builder.size", "builder.auto_size",
	{"builder.size.tiny", "builder.size.small", "builder.size.medium", "builder.size.large", "builder.size.extra_large",
	 "builder.size.huge", "builder.size.impossible"},
	"builder.underground",
	"builder.legend",
	"builder.stats",
	"builder.size_line",
	"builder.size_line_ug",
	"builder.use", "builder.save", "builder.cancel",
	"builder.saved", "builder.not_saved", "builder.use_failed",
	"builder.native_button", "builder.native_tooltip",
};

const char *MessageKey(MessageId id)
{
	switch (id) {
	case MessageId::kTooFewTeams:
		return "builder.msg.too_few_teams";
	case MessageId::kTooManyPlayers:
		return "builder.msg.too_many_players";
	case MessageId::kSmallZones:
		return "builder.msg.small_zones";
	case MessageId::kBigZones:
		return "builder.msg.big_zones";
	case MessageId::kRichZones:
		return "builder.msg.rich_zones";
	default:
		return nullptr;
	}
}

std::string MessageText(const Message &m, const char *text, const std::string &english)
{
	if (!MessageKey(m.id) || !text)
		return english;
	const std::string a = std::to_string(m.a), b = std::to_string(m.b);
	switch (m.id) {
	case MessageId::kTooManyPlayers:
		return texts::Fill(text, {{"players", a}});
	case MessageId::kSmallZones:
	case MessageId::kBigZones:
		return texts::Fill(text, {{"tiles", a}});
	case MessageId::kRichZones:
		return texts::Fill(text, {{"value", a}, {"shipped", b}});
	default:
		return text;
	}
}

// positions(g, o) of template-maker.html, line for line.
std::vector<Point> ZonePositions(const Template &t, const Choices &o)
{
	const double kPi = 3.14159265358979323846;
	const double cx = kMapCentreX, cy = kMapCentreY, R = kMapRadius;
	const int T = o.teams;
	const double step = 2 * kPi / (T > 0 ? T : 1);
	const int sides = T == 2 ? 2 : T;
	std::map<std::pair<int, int>, std::vector<int>> by_team;  // (kind, team) -> zone numbers
	const int n = static_cast<int>(t.zones.size());
	int centres = 0;
	for (int i = 0; i < n; ++i) {
		by_team[std::make_pair(static_cast<int>(t.zones[i].kind), t.zones[i].team)].push_back(i);
		centres += t.zones[i].kind == ZoneKind::kCentre;
	}
	int border_idx = 0, centre_idx = 0;
	std::vector<Point> pos;
	for (int i = 0; i < n; ++i) {
		const Zone &z = t.zones[i];
		double a, r;
		if (z.kind == ZoneKind::kStart || z.kind == ZoneKind::kTeam) {
			const std::vector<int> &list = by_team[std::make_pair(static_cast<int>(z.kind), z.team)];
			int k = 0;
			while (k < static_cast<int>(list.size()) && list[k] != i)
				++k;
			const int count = static_cast<int>(list.size());
			const double spread = (step * 0.5 < 0.42 ? step * 0.5 : 0.42) * (z.kind == ZoneKind::kTeam ? 0.7 : 1);
			a = -kPi / 2 + z.team * step + (count > 1 ? (static_cast<double>(k) / (count - 1) - 0.5) * spread : 0);
			r = z.kind == ZoneKind::kStart ? R : R * 0.58;
		}
		else if (z.kind == ZoneKind::kBorder) {
			const int per = o.border_zones > 0 ? o.border_zones : 1;
			const int side = border_idx / per, k2 = border_idx % per;
			border_idx++;
			const double side_step = 2 * kPi / sides;
			a = -kPi / 2 + side * side_step + side_step / 2 +
			    (per > 1 ? (static_cast<double>(k2) / (per - 1) - 0.5) * side_step * 0.35 : 0);
			r = R * (T == 2 ? 0.95 : 0.98);
		}
		else {
			a = -kPi / 2 + centre_idx * (2 * kPi / (centres > 1 ? centres : 1)) + kPi / 4;
			r = centres > 1 ? 34 : 0;
			centre_idx++;
		}
		pos.push_back({static_cast<float>(cx + r * cos(a)), static_cast<float>(cy + r * sin(a))});
	}
	return pos;
}

// --- the native button --------------------------------------------------------------------
const char kMainPanelPath[] = "UI/RMGScreen/MainPanel.(WindowSimpleShared).xdb";
const char kSaveButtonPath[] = "UI/RMGScreen/Save.(WindowMSButton).xdb";
const char kSaveReactionPath[] = "UI/RMGScreen/Save.(UISRunReaction).xdb";
const char kButtonPath[] = "UI/RMGScreen/OpenHomm5Builder.(WindowMSButton).xdb";
const char kReactionPath[] = "UI/RMGScreen/OpenHomm5Builder.(UISRunReaction).xdb";
const char kLabelPath[] = "UI/RMGScreen/OpenHomm5Builder.txt";
const char kTooltipPath[] = "UI/RMGScreen/TTOpenHomm5Builder.txt";
const char kCommand[] = "oh_builder";

namespace {

// Replaces `from` by `to`, which must be in `text` exactly once.
bool ReplaceOnce(std::string *text, const char *from, const char *to)
{
	const size_t at = text->find(from);
	if (at == std::string::npos || text->find(from, at + 1) != std::string::npos)
		return false;
	text->replace(at, strlen(from), to);
	return true;
}

bool Known(const std::string &bytes, unsigned long long fingerprint)
{
	return Fingerprint(bytes) == fingerprint;
}

}  // namespace

bool MakeMainPanel(const std::string &original, std::string *out)
{
	if (!Known(original, kMainPanelFingerprint))
		return false;
	std::string s = original;
	// One more child, right after the Save button.
	if (!ReplaceOnce(&s, "\t\t<Item href=\"Save.(WindowMSButton).xdb#xpointer(/WindowMSButton)\"/>\r\n",
	                 "\t\t<Item href=\"Save.(WindowMSButton).xdb#xpointer(/WindowMSButton)\"/>\r\n"
	                 "\t\t<Item href=\"OpenHomm5Builder.(WindowMSButton).xdb#xpointer(/WindowMSButton)\"/>\r\n"))
		return false;
	*out = s;
	return true;
}

bool MakeButton(const std::string &save_button, std::string *out)
{
	if (!Known(save_button, kSaveButtonFingerprint))
		return false;
	std::string s = save_button;
	char position[64];
	_snprintf_s(position, sizeof(position), _TRUNCATE, "<x>%d</x>\r\n\t\t\t\t<y>%d</y>", kButtonX, kButtonY);
	const bool ok = ReplaceOnce(&s, "<Name>SaveBtn</Name>", "<Name>OpenHomm5BuilderBtn</Name>") &&
	                ReplaceOnce(&s, "<TooltipFileRef href=\"TTSaveBtn.txt\"/>",
	                            "<TooltipFileRef href=\"TTOpenHomm5Builder.txt\"/>") &&
	                ReplaceOnce(&s, "<x>188</x>\r\n\t\t\t\t<y>430</y>", position) &&
	                ReplaceOnce(&s, "<Enabled>false</Enabled>", "<Enabled>true</Enabled>") &&
	                ReplaceOnce(&s, "\"Save.(UISRunReaction).xdb#xpointer(/UISRunReaction)\"",
	                            "\"OpenHomm5Builder.(UISRunReaction).xdb#xpointer(/UISRunReaction)\"") &&
	                ReplaceOnce(&s, "<TextFileRef href=\"SaveBtn.txt\"/>", "<TextFileRef href=\"OpenHomm5Builder.txt\"/>");
	if (!ok)
		return false;
	*out = s;
	return true;
}

bool MakeReaction(const std::string &save_reaction, std::string *out)
{
	if (!Known(save_reaction, kSaveReactionFingerprint))
		return false;
	std::string s = save_reaction;
	if (!ReplaceOnce(&s, "<First>save</First>", "<First>oh_builder</First>"))
		return false;
	*out = s;
	return true;
}

std::string GameText(const char *utf8)
{
	std::string out("\xff\xfe", 2);
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(utf8); *p;) {
		unsigned code = *p++;
		if (code >= 0xc0 && code < 0xe0 && (*p & 0xc0) == 0x80)
			code = ((code & 0x1f) << 6) | (*p++ & 0x3f);
		else if (code >= 0xe0 && code < 0xf0 && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80) {
			code = ((code & 0x0f) << 12) | ((p[0] & 0x3f) << 6) | (p[1] & 0x3f);
			p += 2;
		}
		out += static_cast<char>(code & 0xff);
		out += static_cast<char>(code >> 8);
	}
	return out;
}

}  // namespace builder
}  // namespace rmg
}  // namespace mods
