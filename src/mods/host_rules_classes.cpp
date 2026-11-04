// Host rules, step 3: the class skill lock (class_skills; docs/plans/host-rules.md,
// "Step 3"). MMH5.5 gives each of its 24 classes a list of 12 skills in
// GameMechanics/RefTables/HeroClass.xdb, and a class is never offered a skill
// outside its list. With class_skills=1 ("unlocked") every class keeps its list
// and chances, and each other skill that MMH5.5 offers to some class gets a
// small chance (class_chance, 2 = vanilla ToE's rare skills).
//
// Why in memory: the game reads HeroClass.xdb once, at start-up, with all its
// reference tables (0x00836b80 -> 0x00cccd60: every table registered by the
// exe's start-up code; HeroClass.xdb's holder is made by 0x00ae2980, called
// from the static initialisers at 0x00dd2d15). Serving another file per game
// cannot work. The table stays in memory: [0x01024c54] + 8 + class * 0x44 is a
// class (0x00ad5af0); its SkillsProbs vector (begin, end, capacity) is at +0x10,
// 12-byte items: a word, the skill (+4), the chance (+8) (0x00875ba0, the only
// reader of the chances: the level-up's "new skill" weights, 0x00b50be0).
//
// How: when a game starts with the rule (every PC, from the host's start
// message, like the other rules), each class's vector is pointed at a new array
// of ours (the game's own allocator, 0x00878fd0, so the game may free it), and
// put back when the game ends: the next game's start, a lobby, or a save loaded
// without the rule. The structure is checked first (the code that reads it, the
// count, every vector), and before putting a vector back, that it is still ours.
//
// Saves: the world's serializer (CWorld's CObjectBase vtable 0x00e625d8, slot 3
// 0x009a2340 -> 0x009a2350; it writes the chunks 1 to 0x3e) gets one more chunk,
// 0x4f, while this game's class skills are unlocked: "OHW1;class_skills=1;...",
// only for the game's file saver (CStructureSaver: saves and loads). The
// checksum savers and the other serializers are left exactly as the game runs
// them (see WorldSerializeHook). The game's reader skips a chunk it does not
// know (a PC without the add-on loads the save on MMH5.5's lock), and zero-fills
// a missing one (a save without it: MMH5.5's lock). When a save is read, the
// rule comes from it.
//
// Testing ([rules] in h5-together.ini):
//   trace_skills=1         log every "new skill" a level-up offers (0x00b50db0, called at
//                          0x00b52885 and 0x00b52909), with the hero's class and whether the
//                          skill is on the class's MMH5.5 list; a summary every 20 offers
//   class_skills_single=1  a single-player game uses this PC's class_skills and class_chance
//                          (to check saves; the other rules stay the file's)
#include "host_rules_internal.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../hook/log.h"
#include "../hook/patch.h"

namespace mods {
namespace rules {
namespace {

// --- the game (vanilla 3.1 layout, also MMH55_Utility) ----------------------------------------
const DWORD kTableAt = 0x01024c54;  // the classes (set by the database load at start-up)
const DWORD kClassOfAt = 0x00ad5af0;
// __fastcall(class): lea eax, [[0x01024c54] + class * 0x44 + 8]
const BYTE kClassOf[] = {0x8b, 0xc1, 0xc1, 0xe0, 0x04, 0x03, 0xc1, 0x8b, 0x0d, 0x54, 0x4c, 0x02, 0x01, 0x8d, 0x44, 0x81, 0x08, 0xc3};
const DWORD kStride = 0x44, kFirst = 8, kVector = 0x10, kItemSize = 12;
// The table's holder is made with its size: push <count> (vanilla 9; MMH5.5's exe 74)
const DWORD kCountAt = 0x00ae29df;
const BYTE kCountTail[] = {0x8d, 0x44, 0x24, 0x24};  // lea eax, [esp+0x24] after it
// The chance reader 0x00875ba0: the vector at +0x10/+0x14, items of 12 bytes (/ 12),
// the skill at +4 (add ebx, 4; cmp [ebx], ebp), the chance at +8
const DWORD kProbOfAt = 0x00875bb0;
const BYTE kProbOf[] = {0x8b, 0x4e, 0x14, 0x2b, 0x4e, 0x10, 0xb8, 0xab, 0xaa, 0xaa, 0x2a};
const DWORD kProbSkillAt = 0x00875bd3;
const BYTE kProbSkill[] = {0x83, 0xc3, 0x04, 0x39, 0x2b};
const DWORD kProbValueAt = 0x00875c09;
const BYTE kProbValue[] = {0x8b, 0x44, 0x90, 0x08};
const DWORD kGameNew = 0x00878fd0;     // __cdecl(size)
const DWORD kGameDelete = 0x00879240;  // __cdecl(pointer)
// The level-up's "new skill" pick: __thiscall(hero, int excluded), ret 4; the class at +0xa4
const DWORD kNewSkill = 0x00b50db0;
const DWORD kNewSkillCalls[2] = {0x00b52885, 0x00b52909};
const DWORD kHeroClass = 0xa4;
// The world's serializer
const DWORD kWorldSerializeSlot = 0x00e625e4;
const DWORD kWorldSerialize = 0x009a2340;  // sub ecx, [ecx-4]; jmp 0x009a2350 (ret 4)
const BYTE kWorldSerializeCode[] = {0x2b, 0x49, 0xfc, 0xe9, 0x08, 0x00, 0x00, 0x00};
const int kWorldChunk = 0x4f;     // the world's own are 1..0x3e
// CStructureSaver (saves, loads, network messages): slot 10 DataChunk, 15 IsReading (mov al, [ecx+0x4c])
const DWORD kStructureSaverVftable = 0x00e93dfc;
const int kWorldChunkSize = 64;   // fixed: a reader copies this many bytes

typedef void *(__cdecl *NewFn)(size_t size);
typedef void(__cdecl *DeleteFn)(void *p);
typedef int(__fastcall *NewSkillFn)(void *hero, void *edx, int excluded);
typedef int(__fastcall *SerializeFn)(void *self, void *edx, void *saver);
typedef char(__fastcall *IsReadingFn)(void *saver, void *edx);
typedef void(__fastcall *DataChunkFn)(void *saver, void *edx, int id, void *data, int size, int number);

NewSkillFn g_new_skill_next = nullptr;
SerializeFn g_serialize_next = nullptr;
bool g_trace = false;

// --- state (under g_lock) ---------------------------------------------------------------------
CRITICAL_SECTION g_lock;
struct Lock {
	Lock() { EnterCriticalSection(&g_lock); }
	~Lock() { LeaveCriticalSection(&g_lock); }
};

ClassInfo g_info;
ClassTable g_mmh55;               // this PC's table as loaded (MMH5.5's lists)
std::vector<BYTE *> g_descs;      // each class's place in memory, as g_mmh55
std::vector<int> g_skills;        // the skills some class is offered
struct Swap {
	DWORD *vec;                    // the class's vector (begin, end, capacity)
	DWORD orig[3];
	DWORD ours[3];
};
std::vector<Swap> g_swaps;        // while unlocked
ClassState g_state;               // this game's
DWORD g_read_at = 0;              // when a save's world was last read
LONG g_offers = 0, g_off_list = 0;

bool Readable(const void *p, size_t n)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (!p || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
		return false;
	return static_cast<const BYTE *>(p) + n <= static_cast<const BYTE *>(mbi.BaseAddress) + mbi.RegionSize;
}

bool CodeIs(DWORD at, const BYTE *bytes, size_t n)
{
	return Readable(reinterpret_cast<const void *>(at), n) && memcmp(reinterpret_cast<const void *>(at), bytes, n) == 0;
}

std::string Hex(DWORD v)
{
	char t[16];
	_snprintf_s(t, sizeof(t), _TRUNCATE, "0x%08x", v);
	return t;
}

// This PC's table from memory, with its structure checked (under the lock, once).
void ReadTable()
{
	if (g_info.read)
		return;
	g_info.read = true;
	std::string why;
	const BYTE *count_code = reinterpret_cast<const BYTE *>(kCountAt);
	if (!g_serialize_next)
		why = "the world's serializer is not as expected (saves could not keep the rule)";
	else if (!CodeIs(kClassOfAt, kClassOf, sizeof(kClassOf)))
		why = "the class lookup (" + Hex(kClassOfAt) + ") is not as expected";
	else if (!CodeIs(kProbOfAt, kProbOf, sizeof(kProbOf)) || !CodeIs(kProbSkillAt, kProbSkill, sizeof(kProbSkill)) ||
	         !CodeIs(kProbValueAt, kProbValue, sizeof(kProbValue)))
		why = "the chance reader (0x00875ba0) is not as expected";
	else if (!Readable(count_code, 6) || count_code[0] != 0x6a || !CodeIs(kCountAt + 2, kCountTail, sizeof(kCountTail)))
		why = "the table's size (" + Hex(kCountAt) + ") is not as expected";
	const int count = why.empty() ? count_code[1] : 0;
	const BYTE *base = nullptr;
	if (why.empty()) {
		base = Readable(reinterpret_cast<const void *>(kTableAt), 4) ? *reinterpret_cast<BYTE *const *>(kTableAt) : nullptr;
		if (count < 2 || count > 120)
			why = "the table's size is " + std::to_string(count);
		else if (!base || !Readable(base, kFirst + count * kStride))
			why = "the table is not in memory (" + Hex(reinterpret_cast<DWORD>(base)) + ")";
	}
	ClassTable table;
	std::vector<BYTE *> descs;
	uint32_t heads = 0;
	bool heads_set = false;
	for (int c = 0; why.empty() && c < count; ++c) {
		BYTE *desc = const_cast<BYTE *>(base) + kFirst + c * kStride;
		const DWORD *vec = reinterpret_cast<const DWORD *>(desc + kVector);
		const DWORD b = vec[0], e = vec[1], cap = vec[2];
		if (b == 0 && e == 0)
			continue;  // a class without a list (HERO_CLASS_NONE, the free places)
		if (e < b || cap < e || (e - b) % kItemSize != 0 || (e - b) / kItemSize > 64 || !Readable(reinterpret_cast<void *>(b), e - b)) {
			why = "class " + std::to_string(c) + ": its skill list is not a plain vector (" + Hex(b) + " " + Hex(e) + " " + Hex(cap) + ")";
			break;
		}
		if (b == e)
			continue;
		ClassSkillList list;
		list.id = c;
		for (DWORD p = b; p < e; p += kItemSize) {
			const DWORD *item = reinterpret_cast<const DWORD *>(p);
			if (!heads_set) {
				heads = item[0];
				heads_set = true;
			}
			else if (item[0] != heads) {
				why = "class " + std::to_string(c) + ": the items' first words differ (" + Hex(item[0]) + ", " + Hex(heads) + ")";
				break;
			}
			list.probs.push_back(SkillProb{item[0], static_cast<int32_t>(item[1]), static_cast<int32_t>(item[2])});
		}
		table.push_back(list);
		descs.push_back(desc);
	}
	if (why.empty())
		why = CheckClassTable(table);
	g_info.classes = static_cast<int>(table.size());
	if (!why.empty()) {
		g_info.why = why;
		hooklog::Write("rules: class skills: this PC's class table can NOT be edited (class_skills stays MMH5.5's lock): %s",
		               why.c_str());
		return;
	}
	g_mmh55 = table;
	g_descs = descs;
	g_skills = OfferedSkills(table);
	g_info.editable = true;
	g_info.hash = ClassTableHash(table);
	g_info.skills = static_cast<int>(g_skills.size());
	std::string skills;
	for (int s : g_skills)
		skills += (skills.empty() ? "" : " ") + std::to_string(s);
	size_t items = 0;
	for (const ClassSkillList &l : table)
		items += l.probs.size();
	hooklog::Write("rules: class skills: this PC's class table (%s, %d places): %d classes with a list, %u skills in all, "
	               "items' first word %s; the skills some class is offered (%d): %s; hash %08x",
	               Hex(reinterpret_cast<DWORD>(base)).c_str(), count, g_info.classes, static_cast<unsigned>(items),
	               Hex(heads).c_str(), g_info.skills, skills.c_str(), g_info.hash);
	for (const ClassSkillList &l : table) {
		std::string list;
		int sum = 0;
		for (const SkillProb &p : l.probs) {
			list += (list.empty() ? "" : " ") + std::to_string(p.skill) + ":" + std::to_string(p.prob);
			sum += p.prob;
		}
		hooklog::Write("rules: class skills: class %d: %s (sum %d)", l.id, list.c_str(), sum);
	}
}

// MMH5.5's lists back (under the lock).
void Restore(const char *why)
{
	if (g_swaps.empty())
		return;
	int back = 0, changed = 0;
	for (const Swap &s : g_swaps) {
		if (Readable(s.vec, 12) && s.vec[0] == s.ours[0] && s.vec[1] == s.ours[1] && s.vec[2] == s.ours[2]) {
			s.vec[0] = s.orig[0];
			s.vec[1] = s.orig[1];
			s.vec[2] = s.orig[2];
			reinterpret_cast<DeleteFn>(kGameDelete)(reinterpret_cast<void *>(s.ours[0]));
			++back;
		}
		else
			++changed;  // not ours any more: the game has changed its table (left alone)
	}
	g_swaps.clear();
	hooklog::Write("rules: class skills: MMH5.5's lock again (%s): %d classes put back%s", why, back,
	               changed ? (", " + std::to_string(changed) + " NOT (the game changed them: left as they are)").c_str() : "");
}

// The lists for this mode (under the lock); false when it is MMH5.5's lock.
bool Unlock(const std::string &mode, const std::string &chance, const char *why)
{
	ClassTable t;
	std::string problem;
	if (!ClassTableFor(g_mmh55, mode, chance, &t, &problem)) {
		if (!problem.empty())
			hooklog::Write("rules: class skills: class_skills=%s NOT applied (%s): MMH5.5's lock", mode.c_str(), problem.c_str());
		return false;
	}
	for (size_t i = 0; i < g_descs.size(); ++i) {
		DWORD *vec = reinterpret_cast<DWORD *>(g_descs[i] + kVector);
		const ClassSkillList &mine = g_mmh55[i];
		const DWORD size = static_cast<DWORD>(mine.probs.size()) * kItemSize;
		if (!Readable(vec, 12) || vec[1] - vec[0] != size) {
			hooklog::Write("rules: class skills: class %d's list changed since start-up: NOT applied (MMH5.5's lock)", mine.id);
			Restore("a list changed");
			return false;
		}
		const std::vector<SkillProb> &list = t[i].probs;
		const DWORD bytes = static_cast<DWORD>(list.size()) * kItemSize;
		BYTE *ours = static_cast<BYTE *>(reinterpret_cast<NewFn>(kGameNew)(bytes));
		if (!ours) {
			hooklog::Write("rules: class skills: no memory: NOT applied (MMH5.5's lock)");
			Restore("no memory");
			return false;
		}
		for (size_t k = 0; k < list.size(); ++k) {
			DWORD *item = reinterpret_cast<DWORD *>(ours + k * kItemSize);
			item[0] = list[k].head;
			item[1] = static_cast<DWORD>(list[k].skill);
			item[2] = static_cast<DWORD>(list[k].prob);
		}
		Swap s;
		s.vec = vec;
		memcpy(s.orig, vec, sizeof(s.orig));
		s.ours[0] = reinterpret_cast<DWORD>(ours);
		s.ours[1] = s.ours[2] = reinterpret_cast<DWORD>(ours) + bytes;
		memcpy(vec, s.ours, sizeof(s.ours));
		g_swaps.push_back(s);
	}
	size_t added = 0;
	for (size_t i = 0; i < t.size(); ++i)
		added += t[i].probs.size() - g_mmh55[i].probs.size();
	const bool by_faction = ClassTableHash(g_mmh55) == kMmh55ClassTableHash;
	hooklog::Write("rules: class skills: UNLOCKED for this game (%s): %u classes, %u skills added at chance %d (MMH5.5's "
	               "lists: each class's chances add up to 100), %s; table hash %08x",
	               why, static_cast<unsigned>(t.size()), static_cast<unsigned>(added), ClassChance(chance),
	               by_faction ? "each class gets its own faction's other skills (MMH5.5's table: the factions are known)"
	                          : "each class gets the skills shared by several factions (an unknown table: never a faction's own)",
	               ClassTableHash(t));
	return true;
}

// --- the trace of the level-up's "new skill" offers ----------------------------------------------
bool OnMmh55List(int cls, int skill)
{
	for (const ClassSkillList &l : g_mmh55)
		if (l.id == cls) {
			for (const SkillProb &p : l.probs)
				if (p.skill == skill)
					return true;
			return false;
		}
	return false;
}

int __fastcall NewSkillHook(void *hero, void *edx, int excluded)
{
	const int skill = g_new_skill_next(hero, edx, excluded);
	const BYTE *h = static_cast<const BYTE *>(hero);
	const int cls = Readable(h + kHeroClass, 4) ? *reinterpret_cast<const int *>(h + kHeroClass) : -1;
	Lock lock;
	ReadTable();  // (read before any edit: if it was not read yet, the table is MMH5.5's)
	if (skill <= 0) {
		hooklog::Write("rules: class skills: level-up of hero %p (class %d): no new skill to offer", hero, cls);
		return skill;
	}
	const bool on = OnMmh55List(cls, skill);
	const LONG n = ++g_offers;
	if (!on)
		++g_off_list;
	if (n <= 600)
		hooklog::Write("rules: class skills: level-up of hero %p (class %d): new skill %d offered, %s (class skills %s)", hero, cls,
		               skill, on ? "on the class's MMH5.5 list" : "NOT on the class's MMH5.5 list",
		               g_swaps.empty() ? "MMH5.5's lock" : "unlocked");
	if (n % 20 == 0)
		hooklog::Write("rules: class skills: %ld new skills offered so far, %ld of them not on the class's MMH5.5 list", n, g_off_list);
	return skill;
}

// --- saves: the world's serializer -----------------------------------------------------------------
// Our chunk, for the game's file saver only (a save being written or read).
__declspec(noinline) void WorldChunk(void *saver)
{
	void **vt = *reinterpret_cast<void ***>(saver);
	const bool reading = reinterpret_cast<IsReadingFn>(vt[15])(saver, nullptr) != 0;
	const DataChunkFn chunk = reinterpret_cast<DataChunkFn>(vt[10]);
	char buf[kWorldChunkSize];
	memset(buf, 0, sizeof(buf));
	if (!reading) {
		std::string text;
		{
			Lock lock;
			if (g_swaps.empty())
				return;  // MMH5.5's lock: the world as the game writes it
			Values v = NoValues();
			v[kClassSkills] = g_state.mode;
			v[kClassChance] = g_state.chance;
			text = EncodeWorld(v);
		}
		if (text.size() >= sizeof(buf))
			return;
		memcpy(buf, text.c_str(), text.size());
		chunk(saver, nullptr, kWorldChunk, buf, kWorldChunkSize, 1);
		static LONG written = 0;
		const LONG n = InterlockedIncrement(&written);
		if (n <= 3 || n % 100 == 0)
			hooklog::Write("rules: class skills: a save of the world is written with \"%s\" (%ld so far)", text.c_str(), n);
		return;
	}
	chunk(saver, nullptr, kWorldChunk, buf, kWorldChunkSize, 1);  // all zeros when the save has none
	buf[kWorldChunkSize - 1] = 0;
	Values v;
	const bool ours = buf[0] && DecodeWorld(buf, &v);
	if (buf[0] && !ours)
		hooklog::Write("rules: class skills: a world is read with rules this PC cannot read (\"%.60s\"): MMH5.5's lock", buf);
	SetClassSkills(ours ? v[kClassSkills] : std::string(kClassLocked), ours ? v[kClassChance] : std::string(), true,
	               ours ? "a saved game is read: its rules" : "a saved game is read: it has no class rules");
	Lock lock;
	g_read_at = GetTickCount();
	if (!g_read_at)
		g_read_at = 1;
}

// The other savers that serialize the world (the multiplayer checksums: CWorldCRCSaver,
// CCheckSumSaver...): each logged once, never touched.
__declspec(noinline) void OtherSaver(DWORD vftable)
{
	static volatile LONG seen[8];
	for (int i = 0; i < 8; ++i) {
		const LONG was = InterlockedCompareExchange(&seen[i], static_cast<LONG>(vftable), 0);
		if (was == static_cast<LONG>(vftable))
			return;
		if (was == 0) {
			hooklog::Write("rules: class skills: the world is also serialized by another saver (vftable 0x%08x): left alone",
			               vftable);
			return;
		}
	}
}

// The wrap stays as small as it can: the game's serializer, then one compare. Only the
// game's file saver (CStructureSaver: saves and loads) gets our chunk. (A first version
// also looked at the checksum savers, calling their IsReading; on RC19k the multiplayer
// checksums then differed now and then between the PCs, even with the rule off: robot
// runs 20261102-012505, -013519, -015203.)
int __fastcall WorldSerializeHook(void *self, void *edx, void *saver)
{
	const int r = g_serialize_next(self, edx, saver);
	if (saver) {
		const DWORD vftable = *static_cast<const DWORD *>(saver);
		if (vftable == kStructureSaverVftable)
			WorldChunk(saver);
		else
			OtherSaver(vftable);
	}
	return r;
}

}  // namespace

ClassInfo ClassTableInfo()
{
	Lock lock;
	ReadTable();
	return g_info;
}

ClassState ClassSkillsNow()
{
	Lock lock;
	return g_state;
}

void SetClassSkills(const std::string &mode, const std::string &chance, bool from_save, const char *why)
{
	Lock lock;
	const std::string m = mode.empty() ? std::string(kClassLocked) : mode;
	const std::string c = chance.empty() ? std::string(kClassChanceVanilla) : chance;
	const bool unlocked_now = !g_swaps.empty();
	if (m == kClassLocked) {
		if (unlocked_now)
			Restore(why);
		g_state = ClassState();
		g_state.from_save = from_save;
		return;
	}
	if (unlocked_now && g_state.mode == m && g_state.chance == c) {
		g_state.from_save = from_save;
		return;  // already so
	}
	Restore("before a new choice");
	ReadTable();
	g_state = ClassState();
	g_state.from_save = from_save;
	if (!g_info.editable) {
		hooklog::Write("rules: class skills: class_skills=%s NOT applied (%s): this PC's class table cannot be edited (%s)",
		               m.c_str(), why, g_info.why.c_str());
		return;
	}
	if (Unlock(m, c, why)) {
		g_state.mode = m;
		g_state.chance = c;
	}
}

bool WorldReadRecently(unsigned long within_ms)
{
	Lock lock;
	return g_read_at && GetTickCount() - g_read_at < within_ms;
}

void ForgetWorldRead()
{
	Lock lock;
	g_read_at = 0;
}

bool InstallClassSkills(bool trace)
{
	InitializeCriticalSection(&g_lock);
	g_trace = trace;
	bool ok = true;
	if (CodeIs(kWorldSerialize, kWorldSerializeCode, sizeof(kWorldSerializeCode)))
		g_serialize_next = reinterpret_cast<SerializeFn>(
			hookpatch::PatchPointer(kWorldSerializeSlot, kWorldSerialize, reinterpret_cast<const void *>(&WorldSerializeHook)));
	if (!g_serialize_next) {
		hooklog::Write("rules: class skills: the world's serializer (slot 0x%08x) is not as expected: saves will not keep the rule",
		               kWorldSerializeSlot);
		ok = false;
	}
	if (trace) {
		g_new_skill_next = reinterpret_cast<NewSkillFn>(
			hookpatch::RedirectCalls(kNewSkillCalls, 2, kNewSkill, reinterpret_cast<const void *>(&NewSkillHook)));
		hooklog::Write(g_new_skill_next ? "rules: class skills: trace_skills=1: every new skill a level-up offers is logged"
		                                : "rules: class skills: trace_skills=1, but the level-up's calls are not as expected: no trace");
	}
	return ok;
}

}  // namespace rules
}  // namespace mods
