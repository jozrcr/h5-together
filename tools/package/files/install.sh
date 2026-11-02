#!/usr/bin/env bash
# H5 Together installer for Linux (Steam / Proton).
#
# Normal use, from the folder where you extracted the zip:   bash install.sh
# Options, for tests and odd setups:
#   --game-dir <folder>   use this game folder (or its bin folder), skip the search
#   --steam-dir <folder>  look in this Steam folder only
#
# It looks for Steam in ~/.steam/steam, ~/.local/share/Steam and the Flatpak
# folder, so a fake $HOME works for tests.
#
# It also copies the random map templates of this package (h5-together/templates/*.xdb)
# into <game bin>/h5-together/templates, and its language files (h5-together/texts/*.txt:
# the words of the add-on's panels) into <game bin>/h5-together/texts. It overwrites its
# own files there and never touches another file.
#
# An existing h5-together.ini is refreshed: it becomes this package's file with the
# player's values in it (the old one is kept as h5-together.ini.bak; see merge_ini).
# The files of the add-on's old name (open-homm5, until 0.2.7) get the new name first,
# the folder bin/open-homm5 included (see move_old_names).
#
# Exit codes: 0 done, 3 refused (a dinput8.dll that is not ours is there),
# 4 game not found, 5 copy failed, 7 package incomplete.

[ -n "${BASH_VERSION:-}" ] || { echo "Please run this with bash:  bash install.sh"; exit 2; }

MARKER='H5 Together hook DLL'
OLD_MARKER='open-homm5 hook DLL'   # the add-on's name until 0.2.7
GAME_FOLDER='Heroes of Might and Magic 5 Tribes of the East'

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
game_dir_arg=""
steam_dir_arg=""
while [ $# -gt 0 ]; do
    case "$1" in
        --game-dir)  game_dir_arg="${2:-}"; shift ;;
        --steam-dir) steam_dir_arg="${2:-}"; shift ;;
        -h|--help)   awk 'NR > 1 { if (/^#/) { sub(/^# ?/, ""); print } else exit }' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $1 (try --help)"; exit 2 ;;
    esac
    shift
done

# ---- shared with uninstall.sh and bug-report.sh: finding the game (keep all copies identical) ----

# True if the file is the dinput8.dll of H5 Together, or of open-homm5 (0.2.7 and
# older): it contains our marker text.
is_ours() { grep -q -a -F -e "$MARKER" -e "$OLD_MARKER" -- "$1" 2>/dev/null; }

# Prints the first file or folder called $2 (ignoring upper/lower case) inside $1.
find_ci() { find "$1" -mindepth 1 -maxdepth 1 -iname "$2" -print 2>/dev/null | head -n 1; }

# Takes the game folder or its bin folder. Prints the bin folder, or fails if
# neither holds MMH55_Utility_64.exe or H5_Game.exe.
bin_of() {
    local folder="$1" candidate
    [ -d "$folder" ] || return 1
    for candidate in "$(find_ci "$folder" bin)" "$folder"; do
        [ -n "$candidate" ] && [ -d "$candidate" ] || continue
        if [ -n "$(find_ci "$candidate" MMH55_Utility_64.exe)" ] || [ -n "$(find_ci "$candidate" H5_Game.exe)" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

steam_roots() {
    local root
    if [ -n "$steam_dir_arg" ]; then
        printf '%s\n' "$steam_dir_arg"
        return
    fi
    for root in "$HOME/.steam/steam" \
                "$HOME/.local/share/Steam" \
                "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
        [ -d "$root/steamapps" ] && printf '%s\n' "$root"
    done
}

# Prints a Steam folder and every library folder listed in its libraryfolders.vdf.
library_folders() {
    local root="$1" vdf="$1/steamapps/libraryfolders.vdf"
    printf '%s\n' "$root"
    [ -f "$vdf" ] || return 0
    sed -n -E \
        -e 's/^[[:space:]]*"path"[[:space:]]+"(.*)"[[:space:]]*$/\1/p' \
        -e 's/^[[:space:]]*"[0-9]+"[[:space:]]+"([/].*)"[[:space:]]*$/\1/p' "$vdf" | sed 's/\\\\/\\/g'
}

# Prints every bin folder of the game that Steam knows about (no duplicates).
find_game_bins() {
    local root lib bin real seen="|"
    while IFS= read -r root; do
        while IFS= read -r lib; do
            bin="$(bin_of "$lib/steamapps/common/$GAME_FOLDER")" || continue
            real="$(cd "$bin" && pwd -P)"
            case "$seen" in *"|$real|"*) continue ;; esac
            seen="$seen$real|"
            printf '%s\n' "$real"
        done < <(library_folders "$root")
    done < <(steam_roots)
}

# Sets $bin to the bin folder of the game to work on. Fails if cancelled or not found.
choose_game_bin() {
    local found=() line answer attempt n
    bin=""
    if [ -n "$game_dir_arg" ]; then
        bin="$(bin_of "$game_dir_arg")" && return 0
        echo "That is not the game folder: there is no MMH55_Utility_64.exe or H5_Game.exe in"
        echo "  $game_dir_arg"
        echo "  (or in its bin folder)."
        return 1
    fi
    while IFS= read -r line; do
        [ -n "$line" ] && found+=("$line")
    done < <(find_game_bins)
    if [ "${#found[@]}" -eq 1 ]; then
        echo "Found the game through Steam."
        bin="${found[0]}"
        return 0
    fi
    if [ "${#found[@]}" -gt 1 ]; then
        echo "The game is in several places:"
        n=1
        for line in "${found[@]}"; do
            echo "  $n) $line"
            n=$((n + 1))
        done
        echo "Type the number of the one to use, or paste another game folder."
    else
        echo "I could not find the game through Steam."
        echo "Paste the game folder here (the one that holds the bin folder), or its bin folder."
    fi
    echo "Press Enter alone to cancel."
    for attempt in 1 2 3 4 5; do
        read -r -p "Game folder: " answer || return 1
        answer="${answer#\"}"; answer="${answer%\"}"
        answer="${answer#\'}"; answer="${answer%\'}"
        answer="${answer/#\~/$HOME}"
        [ -n "$answer" ] || return 1
        if [ "${#found[@]}" -gt 1 ] && [[ "$answer" =~ ^[0-9]+$ ]]; then
            if [ "$answer" -ge 1 ] && [ "$answer" -le "${#found[@]}" ]; then
                bin="${found[$((answer - 1))]}"
                return 0
            fi
            echo "There is no number $answer in the list."
            continue
        fi
        bin="$(bin_of "$answer")" && return 0
        echo "No MMH55_Utility_64.exe or H5_Game.exe there. Try again, or press Enter to cancel."
    done
    return 1
}
# ---- end of the shared block ----

# ---- the settings file: refresh it from this package's h5-together.ini ----
# install.ps1 does exactly the same (Merge-Settings, where the rules are written out);
# test_scripts.py checks that both give the same bytes. In short: the result is this
# package's file; each "key=value" of the player's file fills the ";key=" line of the
# same [section]; the player's other lines with content go at the end of their section
# under "; kept from your previous file:" (a section the new file lacks: at the end).
# Plain POSIX awk (Linux often has mawk, not gawk); LC_ALL=C: one byte = one character.
# BINMODE=3: the gawk of Git Bash on Windows (used by the tests) would otherwise drop
# the CRs as it reads and writes; other awks see an unused variable.

# Writes the refreshed file to $3 from the player's file $1 and the package's file $2.
# Prints: <player's settings> <new settings> <settings not listed in the new file>.
merge_ini() {
    OH_OLD="$1" OH_TPL="$2" OH_OUT="$3" LC_ALL=C awk -v BINMODE=3 '
    function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }
    # Sets kind (blank, section, comment, setting, other), txt, name, key, val.
    function parse(line,    c, p) {
        txt = trim(line); name = ""; key = ""; val = ""
        if (txt == "") { kind = "blank"; return }
        c = substr(txt, 1, 1)
        p = index(txt, "]")
        if (c == "[" && p > 0) { kind = "section"; name = trim(substr(txt, 2, p - 2)); return }
        if (c == ";" || c == "#") {
            kind = "comment"
            if (match(txt, /^;[A-Za-z0-9_]+=/)) key = substr(txt, 2, RLENGTH - 2)
            return
        }
        p = index(txt, "=")
        if (p > 1) { kind = "setting"; key = trim(substr(txt, 1, p - 1)); val = trim(substr(txt, p + 1)); return }
        kind = "other"
    }
    # Reads a file into arr[1..n] without line ends (a UTF-8 byte-order mark at the
    # start goes, and one CR before each LF). Returns n, or -1 if it cannot be read.
    function readfile(path, arr,    n, line, r) {
        n = 0
        while ((r = (getline line < path)) > 0) {
            if (n == 0 && substr(line, 1, 3) == "\357\273\277") line = substr(line, 4)
            if (line ~ /\r$/) { line = substr(line, 1, length(line) - 1); had_cr = 1 }
            arr[++n] = line
        }
        close(path)
        return r < 0 ? -1 : n
    }
    function emit(s) { printf "%s%s", s, eol > out }
    function emit_kept(s, head,    j) {
        if (!(s in nkept)) return
        emit("")
        if (head != "") emit(head)
        emit("; kept from your previous file:")
        for (j = 1; j <= nkept[s]; j++) emit(kept[s, j])
    }
    BEGIN {
        out = ENVIRON["OH_OUT"]
        had_cr = 0
        nt = readfile(ENVIRON["OH_TPL"], tl)
        eol = had_cr ? "\r\n" : "\n"
        no = readfile(ENVIRON["OH_OLD"], ol)
        if (nt < 0 || no < 0) exit 1

        # the file of the package: the section of each line, the settings, the last line of each section
        sec = ""; known[""] = 1; nslots = 0
        for (i = 1; i <= nt; i++) {
            parse(tl[i])
            if (kind == "section") { sec = tolower(name); known[sec] = 1 }
            lsec[i] = sec
            if (kind != "blank") last[sec] = i
            if ((kind == "comment" && key != "") || (kind == "setting" && key ~ /^[A-Za-z0-9_]+$/)) {
                k = sec SUBSEP tolower(key)
                if (!(k in slot)) { slot[k] = 1; slotkey[i] = key; slotlist[++nslots] = k }
            }
        }

        # the file of the player: the values, and the lines to keep as they are
        sec = ""; nos = 0; settings = 0; unknown = 0
        for (i = 1; i <= no; i++) {
            parse(ol[i])
            if (kind == "blank") continue
            if (kind == "section") {
                sec = tolower(name)
                if (!(sec in heading)) { heading[sec] = txt; order[++nos] = sec }
                continue
            }
            if (kind == "comment") {
                if (key != "") seen[sec SUBSEP tolower(key)] = 1
                continue
            }
            if (kind == "setting") {
                settings++
                k = sec SUBSEP tolower(key)
                seen[k] = 1
                if (k in slot) {
                    if (!(k in fill)) { fill[k] = val; continue }
                } else {
                    unknown++
                }
            }
            kept[sec, ++nkept[sec]] = txt
        }
        new = 0
        for (j = 1; j <= nslots; j++) if (!(slotlist[j] in seen)) new++

        # the new file
        if (!("" in last)) emit_kept("", "")
        for (i = 1; i <= nt; i++) {
            line = tl[i]; s = lsec[i]
            if (i in slotkey) {
                k = s SUBSEP tolower(slotkey[i])
                if (k in fill) line = slotkey[i] "=" fill[k]
            }
            emit(line)
            if ((s in last) && last[s] == i) emit_kept(s, "")
        }
        for (j = 1; j <= nos; j++) if (!(order[j] in known)) emit_kept(order[j], heading[order[j]])
        close(out)
        print settings, new, unknown
        exit 0
    }'
}

# Keeps the last three copies of the settings file: .bak (the newest), .bak2, .bak3.
backup_ini() {
    if [ -e "$1.bak2" ]; then mv -f "$1.bak2" "$1.bak3" || return 1; fi
    if [ -e "$1.bak" ]; then mv -f "$1.bak" "$1.bak2" || return 1; fi
    cp -p "$1" "$1.bak"
}

# Refreshes the player's settings file $1 from the package's $2. Sets ini_was to
# refreshed, current (nothing to change), unreadable (not plain text: it holds zero
# bytes, for example a "Unicode" file) or failed, and ini_settings, ini_new, ini_unknown.
refresh_ini() {
    local tmp counts
    ini_was="failed"; ini_settings=0; ini_new=0; ini_unknown=0
    if ! tr -d '\000' < "$1" | cmp -s - "$1"; then
        ini_was="unreadable"
        return
    fi
    tmp="$(mktemp "${TMPDIR:-/tmp}/h5-together-ini.XXXXXX")" || return
    if counts="$(merge_ini "$1" "$2" "$tmp")" && read -r ini_settings ini_new ini_unknown <<< "$counts" && [ -n "$ini_unknown" ]; then
        if cmp -s "$tmp" "$1"; then
            ini_was="current"
        elif backup_ini "$1" && cat "$tmp" > "$1"; then
            ini_was="refreshed"
        fi
    fi
    rm -f "$tmp"
}

# "1 new setting", "2 new settings"
count_text() { if [ "$1" -eq 1 ]; then echo "1 $2"; else echo "$1 $3"; fi; }

# ---- the add-on's old name: until 0.2.7 it was called open-homm5 ----
# Its files in bin get the new name, with the same rules as Move-OldNames in install.ps1
# (read them there): the settings and their .bak copies, the hero looks, the last log
# (it becomes h5-together.old.log), and the folder open-homm5 (renamed, or merged into
# an h5-together folder that is already there). Names are found in any upper/lower case.
old_moved=" "      # what got the new name, for the summary: " open-homm5.ini open-homm5 ..."
old_left=()        # old files left as they are (the new name is there already)
old_folder_left=0  # files of the old folder that the new folder has too

# Gives the file $1 of bin the name $2, unless that name is there already.
move_old() {
    local old
    old="$(find_ci "$bin" "$1")"
    [ -n "$old" ] && [ -f "$old" ] || return 0
    if [ -n "$(find_ci "$bin" "$2")" ] || [ -e "$bin/$2" ]; then
        old_left+=("$1")
        return 0
    fi
    mv "$old" "$bin/$2" || return 1
    old_moved="$old_moved$1 "
}

# Moves what folder $1 holds into folder $2: what $2 lacks is moved, a folder both have is
# merged the same way, a file both have stays in $1. Then $1 goes if it is empty.
merge_folder() {
    local from="$1" to="$2" item name dst
    for item in "$from"/* "$from"/.[!.]* "$from"/..?*; do
        [ -e "$item" ] || [ -L "$item" ] || continue
        name="${item##*/}"
        dst="$(find_ci "$to" "$name")"
        [ -z "$dst" ] && [ -e "$to/$name" ] && dst="$to/$name"
        if [ -z "$dst" ]; then
            mv "$item" "$to/$name" || return 1
        elif [ -d "$item" ] && [ ! -L "$item" ] && [ -d "$dst" ]; then
            merge_folder "$item" "$dst" || return 1
        else
            old_folder_left=$((old_folder_left + 1))
        fi
    done
    rmdir "$from" 2>/dev/null   # only if nothing is left in it
    return 0
}

move_old_names() {
    local ext log old_dir new_dir old_tpl f p ini_stayed=0
    move_old open-homm5.ini h5-together.ini || return 1
    move_old open-homm5-skins.ini h5-together-skins.ini || return 1
    # the installer's copies of the settings follow the settings file (not when it stayed)
    for f in "${old_left[@]}"; do [ "$f" = open-homm5.ini ] && ini_stayed=1; done
    if [ "$ini_stayed" -eq 0 ]; then
        for ext in bak bak2 bak3; do
            f="$(find_ci "$bin" "open-homm5.ini.$ext")"
            if [ -n "$f" ] && [ -f "$f" ] && [ -z "$(find_ci "$bin" "h5-together.ini.$ext")" ]; then
                mv "$f" "$bin/h5-together.ini.$ext" || return 1
            fi
        done
    fi
    log="$(find_ci "$bin" open-homm5.log)"
    if [ -n "$log" ] && [ -f "$log" ]; then
        if [ -z "$(find_ci "$bin" h5-together.log)" ] && [ -z "$(find_ci "$bin" h5-together.old.log)" ]; then
            mv "$log" "$bin/h5-together.old.log" || return 1
            old_moved="${old_moved}open-homm5.log "
        else
            rm -f "$log" || return 1
        fi
    fi
    log="$(find_ci "$bin" open-homm5.old.log)"
    if [ -n "$log" ] && [ -f "$log" ]; then rm -f "$log" || return 1; fi
    old_dir="$(find_ci "$bin" open-homm5)"
    [ -n "$old_dir" ] && [ -d "$old_dir" ] || return 0
    new_dir="$(find_ci "$bin" h5-together)"
    if [ -z "$new_dir" ]; then
        mv "$old_dir" "$bin/h5-together" || return 1
    else
        # our templates in the old folder go: this install puts them in the new one
        old_tpl="$(find_ci "$old_dir" templates)"
        if [ -n "$old_tpl" ] && [ -d "$old_tpl" ]; then
            for f in "${tpl_files[@]}"; do
                p="$(find_ci "$old_tpl" "$(basename "$f")")"
                [ -n "$p" ] && [ -f "$p" ] && { rm -f "$p" || return 1; }
            done
        fi
        merge_folder "$old_dir" "$new_dir" || return 1
    fi
    old_moved="${old_moved}open-homm5 "
}

echo
echo "H5 Together installer"
echo "---------------------"

src_dll="$here/dinput8.dll"
src_ini="$here/h5-together.ini"
src_tpl="$here/h5-together/templates"
tpl_files=()
for f in "$src_tpl"/*.xdb; do
    [ -f "$f" ] && tpl_files+=("$f")
done
src_txt="$here/h5-together/texts"
txt_files=()
for f in "$src_txt"/*.txt; do
    [ -f "$f" ] && txt_files+=("$f")
done
if [ ! -f "$src_dll" ] || [ ! -f "$src_ini" ] || [ "${#tpl_files[@]}" -eq 0 ] || [ "${#txt_files[@]}" -eq 0 ]; then
    echo "dinput8.dll, h5-together.ini and the folders h5-together/templates and h5-together/texts"
    echo "must be next to this script."
    echo "Please extract the whole zip first, then run install.sh from the new folder."
    echo "Nothing was installed."
    exit 7
fi
if ! is_ours "$src_dll"; then
    echo "The dinput8.dll in this package is not the H5 Together one. The download may be damaged."
    echo "Nothing was installed."
    exit 7
fi

if ! choose_game_bin; then
    echo
    echo "No game folder, so nothing was installed."
    exit 4
fi
echo "Game folder: $bin"

# Never overwrite a dinput8.dll that belongs to something else.
dst_dll="$bin/dinput8.dll"
dll_was="new"
existing="$(find_ci "$bin" dinput8.dll)"
if [ -n "$existing" ]; then
    if ! is_ours "$existing"; then
        echo
        echo "Stopped. There is already a file called dinput8.dll in the game folder,"
        echo "and it is not the H5 Together one (another add-on may use it)."
        echo "I did not change anything. To use H5 Together and that add-on together,"
        echo "rename that file to dinput8_next.dll (H5 Together then loads it, and the"
        echo "log says \"chained\"), or move it away; then run this installer again."
        exit 3
    fi
    dst_dll="$existing"
    if cmp -s "$src_dll" "$dst_dll"; then dll_was="same"; else dll_was="older"; fi
fi

# The files of the old name (open-homm5, until 0.2.7) get the new name first, so that the
# settings refresh and the templates below work on them.
if ! move_old_names; then
    echo
    echo "Could not give your open-homm5 files their new name, h5-together (see the message above)."
    echo "If the game is running, close it and try again."
    exit 5
fi

dst_ini="$(find_ci "$bin" h5-together.ini)"
ini_was="old"
if ! cp -f "$src_dll" "$dst_dll"; then
    echo
    echo "Could not copy dinput8.dll into the game folder (see the message above)."
    echo "If the game is running, close it and try again."
    exit 5
fi
if [ -z "$dst_ini" ]; then
    if cp "$src_ini" "$bin/h5-together.ini"; then
        ini_was="new"
    else
        echo "Could not copy h5-together.ini (see the message above)."
        exit 5
    fi
else
    # the player's settings file: add this version's settings, keep the player's values
    refresh_ini "$dst_ini" "$src_ini"
fi

# Our random map templates: overwrite our own OH-*.xdb files, leave every other file alone.
tpl_dir="$(find_ci "$bin" h5-together)"
[ -n "$tpl_dir" ] && [ -d "$tpl_dir" ] || tpl_dir="$bin/h5-together"
tpl_sub="$(find_ci "$tpl_dir" templates)"
[ -n "$tpl_sub" ] && [ -d "$tpl_sub" ] || tpl_sub="$tpl_dir/templates"
if ! mkdir -p "$tpl_sub"; then
    echo "Could not create the folder $tpl_sub (see the message above)."
    exit 5
fi
tpl_replaced=0
for f in "${tpl_files[@]}"; do
    name="$(basename "$f")"
    dst="$(find_ci "$tpl_sub" "$name")"
    if [ -n "$dst" ]; then tpl_replaced=$((tpl_replaced + 1)); else dst="$tpl_sub/$name"; fi
    if ! cp -f "$f" "$dst"; then
        echo "Could not copy $name into $tpl_sub (see the message above)."
        exit 5
    fi
done
tpl_total="$(find "$tpl_sub" -mindepth 1 -maxdepth 1 -type f | wc -l)"
tpl_others=$((tpl_total - ${#tpl_files[@]}))

# Our language files: the same way (a translation of the player's own stays).
txt_sub="$(find_ci "$tpl_dir" texts)"
[ -n "$txt_sub" ] && [ -d "$txt_sub" ] || txt_sub="$tpl_dir/texts"
if ! mkdir -p "$txt_sub"; then
    echo "Could not create the folder $txt_sub (see the message above)."
    exit 5
fi
txt_replaced=0
txt_codes=""
for f in "${txt_files[@]}"; do
    name="$(basename "$f")"
    txt_codes="$txt_codes, ${name%.txt}"
    dst="$(find_ci "$txt_sub" "$name")"
    if [ -n "$dst" ]; then txt_replaced=$((txt_replaced + 1)); else dst="$txt_sub/$name"; fi
    if ! cp -f "$f" "$dst"; then
        echo "Could not copy $name into $txt_sub (see the message above)."
        exit 5
    fi
done
txt_total="$(find "$txt_sub" -mindepth 1 -maxdepth 1 -type f | wc -l)"
txt_others=$((txt_total - ${#txt_files[@]}))

echo
echo "Done. What I did:"
if [ "$old_moved" != " " ]; then
    what=""
    case "$old_moved" in *" open-homm5.ini "*) what="$what, the settings" ;; esac
    case "$old_moved" in *" open-homm5-skins.ini "*) what="$what, the hero looks" ;; esac
    case "$old_moved" in *" open-homm5.log "*) what="$what, the log" ;; esac
    case "$old_moved" in *" open-homm5 "*) what="$what, the folder" ;; esac
    echo "  - open-homm5 is now called H5 Together: your files got the new name, h5-together (${what#, })"
fi
for name in "${old_left[@]}"; do
    echo "  - kept $name as it is: there is already a h5-together${name#open-homm5}, which is the one used"
done
[ "$old_folder_left" -gt 0 ] && echo "  - left $old_folder_left file(s) in the folder open-homm5: the folder h5-together has them too"
case "$dll_was" in
    new)   echo "  - added dinput8.dll (the add-on)" ;;
    same)  echo "  - dinput8.dll was already installed and up to date" ;;
    older) echo "  - replaced the older H5 Together dinput8.dll with this version" ;;
esac
case "$ini_was" in
    new) echo "  - added h5-together.ini (settings, all at their defaults)" ;;
    refreshed)
        echo "  - settings file refreshed: $ini_settings of your settings kept, $(count_text "$ini_new" "new setting" "new settings") available (see the comments)"
        echo "    (your previous file is saved as h5-together.ini.bak)"
        [ "$ini_unknown" -gt 0 ] && echo "    ($(count_text "$ini_unknown" setting settings) not listed in this version: kept at the end of their section, under \"kept from your previous file\")"
        ;;
    current)
        if [ "$ini_settings" -gt 0 ]; then
            echo "  - settings file already up to date: $ini_settings of your settings kept"
        else
            echo "  - settings file already up to date (every setting at its default)"
        fi
        ;;
    unreadable) echo "  - kept h5-together.ini as it is: it is not a plain text file, so I could not add the new settings" ;;
    *)
        echo "  - could not refresh h5-together.ini (see the message above)"
        echo "    If the game is running, close it and run the installer again."
        ;;
esac
echo "  - installed ${#tpl_files[@]} random map templates in h5-together/templates (they are in the game's random map list)"
[ "$tpl_replaced" -gt 0 ] && echo "    ($tpl_replaced of them replaced our files from an earlier install)"
[ "$tpl_others" -gt 0 ] && echo "    (left the $tpl_others other file(s) in that folder alone)"
echo "    Everyone in a multiplayer game needs the same templates: install the same version."
echo "  - installed ${#txt_files[@]} language files in h5-together/texts (${txt_codes#, }): the words of the add-on's panels"
[ "$txt_replaced" -gt 0 ] && echo "    ($txt_replaced of them replaced our files from an earlier install)"
[ "$txt_others" -gt 0 ] && echo "    (left the $txt_others other file(s) in that folder alone)"
echo "  - did not change or replace any file of the game itself"

if [ -z "$(find_ci "$bin" MMH55_Utility_64.exe)" ]; then
    echo
    echo "Note: there is no MMH55_Utility_64.exe in this folder. The add-on is made for"
    echo "MMH5.5 (the co-op version of the game). Install MMH5.5 first (see docs/linux.md)."
fi

cat <<'EOF'

One more step, in Steam (I do not touch Steam's settings myself):
right-click the game > Properties > General > Launch options, and paste:

    WINEDLLOVERRIDES="dinput8=n,b" %command%

This tells Wine (Proton) to load our dinput8.dll instead of its own.

Which exe starts? Steam starts H5_Game.exe, the plain game, and the add-on only
works with MMH5.5's exe, MMH55_Utility_64.exe (the co-op one). To make Steam start
that exe, paste this line instead of the one above:

    WINEDLLOVERRIDES="dinput8=n,b" bash -c 'exec "${@/H5_Game.exe/MMH55_Utility_64.exe}"' -- %command%

If it does not work, docs/linux.md (step 3) has another way: swap the two exe
files. https://github.com/jozrcr/h5-together/blob/main/docs/linux.md

How to check: start the game from Steam. A file named h5-together.log then appears in:
EOF
echo "  $bin"
echo "If you see it, the add-on is running. Everyone in the same co-op game should install it."
echo "To remove it later, run: bash uninstall.sh"
exit 0
