# H5 Together installer for Windows. Runs on Windows PowerShell 5.1.
#
# Keep this file plain ASCII: PowerShell 5.1 reads a file without a byte-order
# mark as ANSI, so any accent would be garbled on a French Windows.
#
# Normal use: double-click install.bat. Options, for tests and odd setups:
#   -GameDir <folder>   use this game folder (or its bin folder), skip the search
#   -SteamDir <folder>  look in this Steam folder instead of the one in the registry
#   -NoPause            do not wait for Enter at the end
#
# The add-on's language files (h5-together\texts\*.txt: its panels' words, English and
# French) go to bin\h5-together\texts: ours are overwritten, any other file (a translation
# of the player's own) stays.
#
# An existing h5-together.ini is refreshed: it becomes this package's file with the
# player's values in it (the old one is kept as h5-together.ini.bak; see Merge-Settings).
# The files of the add-on's old name (open-homm5, until 0.2.7) get the new name first,
# the folder bin\open-homm5 included (see Move-OldNames).
#
# Exit codes: 0 done, 3 refused (a dinput8.dll that is not ours is there),
# 4 game not found, 5 copy failed, 6 unexpected error, 7 package incomplete.
param(
    [string]$GameDir = "",
    [string]$SteamDir = "",
    [switch]$NoPause,
    # Tests only: pretend these are the network profiles, "name|category;name|category".
    [string]$TestProfiles = ""
)

$ErrorActionPreference = "Stop"

function Wait-AndExit([int]$Code) {
    if (-not $NoPause) {
        Write-Host ""
        try { [void](Read-Host "Press Enter to close this window") } catch { }
    }
    exit $Code
}

# ---- shared with uninstall.ps1 and bug-report.ps1: finding the game (keep all copies identical) ----
$Marker = "H5 Together hook DLL"
$OldMarker = "open-homm5 hook DLL"  # the add-on's name until 0.2.7
$GameFolderName = "Heroes of Might and Magic 5 Tribes of the East"

# True if the file is the dinput8.dll of H5 Together, or of open-homm5 (0.2.7 and
# older): it contains our marker text.
function Test-IsOurs([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $text = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    return ($text.Contains($Marker) -or $text.Contains($OldMarker))
}

# Takes the game folder or its bin folder. Returns the bin folder, or $null if
# neither holds MMH55_Utility_64.exe or H5_Game.exe.
function Get-BinFolder([string]$Folder) {
    if (-not $Folder) { return $null }
    $Folder = $Folder.Trim().Trim('"').Trim()
    if (-not $Folder) { return $null }
    try {
        foreach ($candidate in @((Join-Path $Folder "bin"), $Folder)) {
            if ((Test-Path -LiteralPath (Join-Path $candidate "MMH55_Utility_64.exe")) -or
                (Test-Path -LiteralPath (Join-Path $candidate "H5_Game.exe"))) {
                return $candidate
            }
        }
    } catch { }
    return $null
}

function Get-SteamPath {
    if ($SteamDir) { return $SteamDir }
    try {
        $p = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name "SteamPath" -ErrorAction Stop).SteamPath
        if ($p) { return ($p -replace '/', '\') }
    } catch { }
    return $null
}

# Steam's own folder plus every library folder listed in libraryfolders.vdf.
function Get-LibraryFolders([string]$Steam) {
    $libs = New-Object System.Collections.ArrayList
    $vdf = Join-Path $Steam "steamapps\libraryfolders.vdf"
    if (Test-Path -LiteralPath $vdf) {
        foreach ($line in (Get-Content -LiteralPath $vdf -Encoding UTF8)) {
            if ($line -match '^\s*"path"\s+"(.+)"\s*$') {
                [void]$libs.Add(($Matches[1] -replace '\\\\', '\'))
            } elseif ($line -match '^\s*"\d+"\s+"([^"]*[\\/:][^"]*)"\s*$') {
                # older Steam: "1"  "D:\\SteamLibrary"
                [void]$libs.Add(($Matches[1] -replace '\\\\', '\'))
            }
        }
    }
    # Steam's own folder is a library too (the list normally has it already).
    $listed = $false
    foreach ($l in $libs) { if ($l.TrimEnd('\') -ieq $Steam.TrimEnd('\')) { $listed = $true } }
    if (-not $listed) { $libs.Insert(0, $Steam) }
    return $libs
}

# Every bin folder of the game that Steam knows about (no duplicates).
function Find-GameBins {
    $found = New-Object System.Collections.ArrayList
    $steam = Get-SteamPath
    if (-not $steam) { return @() }
    foreach ($lib in (Get-LibraryFolders $steam)) {
        $bin = Get-BinFolder (Join-Path $lib ("steamapps\common\" + $GameFolderName))
        if ($bin) {
            $dup = $false
            foreach ($f in $found) { if ($f -ieq $bin) { $dup = $true } }
            if (-not $dup) { [void]$found.Add($bin) }
        }
    }
    return @($found)
}

# Returns the bin folder of the game to work on, or $null (cancelled or not found).
function Select-GameBin {
    if ($GameDir) {
        $bin = Get-BinFolder $GameDir
        if (-not $bin) {
            Write-Host "That is not the game folder: there is no MMH55_Utility_64.exe or H5_Game.exe in" -ForegroundColor Red
            Write-Host "  $GameDir" -ForegroundColor Red
            Write-Host "  (or in its bin folder)." -ForegroundColor Red
        }
        return $bin
    }
    $found = @(Find-GameBins)
    if ($found.Count -eq 1) {
        Write-Host "Found the game through Steam."
        return $found[0]
    }
    if ($found.Count -gt 1) {
        Write-Host "The game is in several places:"
        for ($i = 0; $i -lt $found.Count; $i++) { Write-Host ("  {0}) {1}" -f ($i + 1), $found[$i]) }
        Write-Host "Type the number of the one to use, or paste another game folder."
        Write-Host "Press Enter alone to cancel."
    } else {
        Write-Host "I could not find the game through Steam."
        Write-Host "Paste the game folder here (the one that holds the bin folder), or its bin folder."
        Write-Host "Press Enter alone to cancel."
    }
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        $answer = $null
        try { $answer = Read-Host "Game folder" } catch { }
        if ($null -eq $answer) { return $null }
        $answer = $answer.Trim().Trim('"').Trim("'").Trim()
        if ($answer -eq "") { return $null }
        if ($found.Count -gt 1 -and $answer -match '^\d+$') {
            $n = [int]$answer
            if ($n -ge 1 -and $n -le $found.Count) { return $found[$n - 1] }
            Write-Host "There is no number $n in the list."
            continue
        }
        $bin = Get-BinFolder $answer
        if ($bin) { return $bin }
        Write-Host "No MMH55_Utility_64.exe or H5_Game.exe there. Try again, or press Enter to cancel."
    }
    return $null
}
# ---- end of the shared block ----

# SHA-256 of a file, through .NET: Get-FileHash lives in a module that Windows
# PowerShell 5.1 sometimes cannot load (for example when started from PowerShell 7).
function Get-Sha256([string]$Path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [System.BitConverter]::ToString($sha.ComputeHash([System.IO.File]::ReadAllBytes($Path))) }
    finally { $sha.Dispose() }
}

# Names of ZeroTier networks that Windows treats as Public. Only looks, changes nothing.
function Get-PublicZeroTier {
    $names = @()
    try {
        if ($TestProfiles) {
            foreach ($item in $TestProfiles.Split(";")) {
                $parts = $item.Split("|")
                if ($parts.Count -ge 2 -and $parts[0] -like "*ZeroTier*" -and $parts[1] -eq "Public") { $names += $parts[0] }
            }
        } else {
            $adapters = @(Get-NetAdapter -ErrorAction Stop | Where-Object {
                $_.InterfaceDescription -like "*ZeroTier*" -or $_.Name -like "*ZeroTier*" })
            foreach ($p in @(Get-NetConnectionProfile -ErrorAction Stop)) {
                $isZeroTier = $p.InterfaceAlias -like "*ZeroTier*"
                foreach ($a in $adapters) { if ($a.InterfaceIndex -eq $p.InterfaceIndex) { $isZeroTier = $true } }
                if ($isZeroTier -and ([string]$p.NetworkCategory -eq "Public")) { $names += $p.InterfaceAlias }
            }
        }
    } catch { }
    return $names
}

# ---- the settings file: refresh it from this package's h5-together.ini ----
# install.sh does exactly the same (merge_ini); test_scripts.py checks that both give
# the same bytes. The rules:
# - the result is this package's h5-together.ini (every setting, commented out), with
#   its line endings;
# - each "key=value" line of the player's file fills the line ";key=..." of the same
#   [section] of the new file: it becomes "key=value". Names ignore upper/lower case
#   (as the game's ini reader does); spaces around the key and the value go;
# - the player's other lines with content (a setting this version does not list, a
#   second copy of one) go, as they were, at the end of their section under
#   "; kept from your previous file:". A section the new file lacks goes at the end;
# - the player's own comments and blank lines are not carried over (the backup,
#   h5-together.ini.bak, has them).
# Files are read and written one byte = one character (Latin-1), so nothing is
# re-encoded: an ASCII file stays ASCII.
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)
$IniBlank = [char[]]@([char]32, [char]9)
$KeptHeading = "; kept from your previous file:"

# Lower case for A to Z only (as awk does in install.sh).
function Get-AsciiLower([string]$Text) {
    $a = $Text.ToCharArray()
    for ($i = 0; $i -lt $a.Length; $i++) {
        $c = [int]$a[$i]
        if ($c -ge 65 -and $c -le 90) { $a[$i] = [char]($c + 32) }
    }
    return (-join $a)
}

function New-OrdinalTable { return (New-Object System.Collections.Hashtable ([StringComparer]::Ordinal)) }

# The lines of a file without their line ends: a UTF-8 byte-order mark at the start
# goes, and one CR before each LF. Crlf: some line ended with CR LF.
function Read-IniLines([byte[]]$Bytes) {
    $start = 0
    if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF) { $start = 3 }
    $text = $Latin1.GetString($Bytes, $start, $Bytes.Length - $start)
    $lines = New-Object System.Collections.Generic.List[string]
    $crlf = $false
    if ($text.Length -gt 0) {
        $parts = $text.Split([char]10)
        $n = $parts.Length
        if ($text[$text.Length - 1] -eq [char]10) { $n-- }
        for ($i = 0; $i -lt $n; $i++) {
            $l = $parts[$i]
            if ($l.EndsWith([string][char]13)) { $l = $l.Substring(0, $l.Length - 1); $crlf = $true }
            $lines.Add($l)
        }
    }
    return @{ Lines = $lines; Crlf = $crlf }
}

# What one line is. Kind: blank, section (Name), comment (Key if it reads ";key=..."),
# setting (Key, Value) or other. Text: the line without spaces at its ends.
function Get-IniLine([string]$Line) {
    $t = $Line.Trim($IniBlank)
    $r = @{ Kind = "other"; Text = $t; Name = ""; Key = ""; Value = "" }
    if ($t.Length -eq 0) { $r.Kind = "blank"; return $r }
    $c = $t[0]
    $p = $t.IndexOf([char]']')
    if ($c -eq [char]'[' -and $p -gt 0) {
        $r.Kind = "section"
        $r.Name = $t.Substring(1, $p - 1).Trim($IniBlank)
        return $r
    }
    if ($c -eq [char]';' -or $c -eq [char]'#') {
        $r.Kind = "comment"
        if ($t -cmatch '^;([A-Za-z0-9_]+)=') { $r.Key = $Matches[1] }
        return $r
    }
    $p = $t.IndexOf([char]'=')
    if ($p -gt 0) {
        $r.Kind = "setting"
        $r.Key = $t.Substring(0, $p).Trim($IniBlank)
        $r.Value = $t.Substring($p + 1).Trim($IniBlank)
    }
    return $r
}

function Add-KeptLines($Out, [string]$Heading, $Lines, [string]$Eol) {
    if ($null -eq $Lines -or $Lines.Count -eq 0) { return }
    [void]$Out.Append($Eol)
    if ($Heading) { [void]$Out.Append($Heading).Append($Eol) }
    [void]$Out.Append($KeptHeading).Append($Eol)
    foreach ($l in $Lines) { [void]$Out.Append($l).Append($Eol) }
}

# The refreshed settings file (Bytes) from the player's file and this package's.
# Settings: the player's "key=value" lines; New: settings of the package's file that
# the player's file does not mention; Unknown: player's settings the package's file
# does not list. $null if the player's file is not plain text (it holds zero bytes,
# for example saved as "Unicode" by Notepad): then it is left as it is.
function Merge-Settings([byte[]]$PlayerBytes, [byte[]]$PackageBytes) {
    # (PowerShell names ignore upper/lower case: $old here is not $Old)
    if ([Array]::IndexOf($PlayerBytes, [byte]0) -ge 0) { return $null }
    $tpl = Read-IniLines $PackageBytes
    $old = Read-IniLines $PlayerBytes
    $eol = "`n"
    if ($tpl.Crlf) { $eol = "`r`n" }

    # The package's file: each line's section, the settings, each section's last line.
    $lineSec = New-Object System.Collections.Generic.List[string]
    $slotKey = @{}
    $slots = New-OrdinalTable
    $known = New-OrdinalTable
    $lastLine = New-OrdinalTable
    $sec = ""
    $known[""] = $true
    for ($i = 0; $i -lt $tpl.Lines.Count; $i++) {
        $p = Get-IniLine $tpl.Lines[$i]
        if ($p.Kind -eq "section") { $sec = Get-AsciiLower $p.Name; $known[$sec] = $true }
        $lineSec.Add($sec)
        if ($p.Kind -ne "blank") { $lastLine[$sec] = $i }
        if (($p.Kind -eq "comment" -and $p.Key -ne "") -or ($p.Kind -eq "setting" -and $p.Key -cmatch '^[A-Za-z0-9_]+$')) {
            $k = $sec + "`n" + (Get-AsciiLower $p.Key)
            if (-not $slots.ContainsKey($k)) { $slots[$k] = $true; $slotKey[$i] = $p.Key }
        }
    }

    # The player's file: the values, and the lines to keep as they are.
    $fill = New-OrdinalTable
    $seen = New-OrdinalTable
    $kept = New-OrdinalTable
    $heading = New-OrdinalTable
    $order = New-Object System.Collections.Generic.List[string]
    $settings = 0
    $unknown = 0
    $sec = ""
    foreach ($line in $old.Lines) {
        $p = Get-IniLine $line
        if ($p.Kind -eq "blank") { continue }
        if ($p.Kind -eq "section") {
            $sec = Get-AsciiLower $p.Name
            if (-not $heading.ContainsKey($sec)) { $heading[$sec] = $p.Text; $order.Add($sec) }
            continue
        }
        if ($p.Kind -eq "comment") {
            if ($p.Key -ne "") { $seen[$sec + "`n" + (Get-AsciiLower $p.Key)] = $true }
            continue
        }
        if ($p.Kind -eq "setting") {
            $settings++
            $k = $sec + "`n" + (Get-AsciiLower $p.Key)
            $seen[$k] = $true
            if ($slots.ContainsKey($k)) {
                if (-not $fill.ContainsKey($k)) { $fill[$k] = $p.Value; continue }
            } else {
                $unknown++
            }
        }
        if (-not $kept.ContainsKey($sec)) { $kept[$sec] = New-Object System.Collections.Generic.List[string] }
        $kept[$sec].Add($p.Text)
    }
    $new = 0
    foreach ($k in $slots.Keys) { if (-not $seen.ContainsKey($k)) { $new++ } }

    # The new file.
    $out = New-Object System.Text.StringBuilder
    if (-not $lastLine.ContainsKey("")) { Add-KeptLines $out $null $kept[""] $eol }
    for ($i = 0; $i -lt $tpl.Lines.Count; $i++) {
        $line = $tpl.Lines[$i]
        $s = $lineSec[$i]
        if ($slotKey.ContainsKey($i)) {
            $k = $s + "`n" + (Get-AsciiLower $slotKey[$i])
            if ($fill.ContainsKey($k)) { $line = $slotKey[$i] + "=" + $fill[$k] }
        }
        [void]$out.Append($line).Append($eol)
        if ($lastLine.ContainsKey($s) -and $lastLine[$s] -eq $i) { Add-KeptLines $out $null $kept[$s] $eol }
    }
    foreach ($s in $order) {
        if (-not $known.ContainsKey($s)) { Add-KeptLines $out $heading[$s] $kept[$s] $eol }
    }
    return @{ Bytes = $Latin1.GetBytes($out.ToString()); Settings = $settings; New = $new; Unknown = $unknown }
}

# Keeps the last three copies of the settings file: .bak (the newest), .bak2, .bak3.
function Save-IniBackup([string]$Path) {
    $b1 = $Path + ".bak"
    $b2 = $Path + ".bak2"
    $b3 = $Path + ".bak3"
    if (Test-Path -LiteralPath $b2) {
        if (Test-Path -LiteralPath $b3) { Remove-Item -LiteralPath $b3 -Force }
        Move-Item -LiteralPath $b2 -Destination $b3
    }
    if (Test-Path -LiteralPath $b1) { Move-Item -LiteralPath $b1 -Destination $b2 }
    Copy-Item -LiteralPath $Path -Destination $b1
}

# "1 new setting", "2 new settings"
function Get-Count([int]$N, [string]$One, [string]$Many) {
    if ($N -eq 1) { return "1 $One" }
    return "$N $Many"
}

# ---- the add-on's old name: until 0.2.7 it was called open-homm5 ----
# Its files in bin had that name: open-homm5.ini (and its .bak copies), open-homm5-skins.ini
# (the hero looks), open-homm5.log and open-homm5.old.log, and the folder open-homm5 (our
# templates, the French packs in lang\fr, anything the player put there). They get the new
# name, so nothing the player set or added is lost. install.sh does the same (move_old_names).
#   - a file moves only if the new name is not there yet; else it stays as it is (the new
#     one is the one used), and the summary says so;
#   - the last log becomes h5-together.old.log (the older log) while there is no log of the
#     new name yet; the other old logs are removed (ours, and older than that);
#   - the folder is renamed; if h5-together is there too, what it lacks is moved into it,
#     and our templates in the old folder go (this install puts them in the new one).

# Moves what the folder $From holds into the folder $To: what $To lacks is moved, a folder
# both have is merged the same way, a file both have stays in $From. Then $From is removed
# if it is empty. Returns how many files or folders stayed in $From.
function Merge-Folder([string]$From, [string]$To) {
    $left = 0
    foreach ($item in @(Get-ChildItem -LiteralPath $From -Force)) {
        $dst = Join-Path $To $item.Name
        if (-not (Test-Path -LiteralPath $dst)) {
            Move-Item -LiteralPath $item.FullName -Destination $dst
        } elseif ($item.PSIsContainer -and (Test-Path -LiteralPath $dst -PathType Container)) {
            $left += Merge-Folder $item.FullName $dst
        } else {
            $left++
        }
    }
    if (@(Get-ChildItem -LiteralPath $From -Force).Count -eq 0) { Remove-Item -LiteralPath $From -Force }
    return $left
}

# Gives the old files in $Bin their new names. $Templates: the names of this package's
# templates. Returns Moved (what got the new name), Left (old files left as they are) and
# FolderLeft (files of the old folder that the new one also has).
function Move-OldNames([string]$Bin, $Templates) {
    $r = @{ Moved = New-Object System.Collections.ArrayList; Left = New-Object System.Collections.ArrayList; FolderLeft = 0 }
    foreach ($ext in @(".ini", "-skins.ini")) {
        $old = Join-Path $Bin ("open-homm5" + $ext)
        $new = Join-Path $Bin ("h5-together" + $ext)
        if (-not (Test-Path -LiteralPath $old -PathType Leaf)) { continue }
        if (Test-Path -LiteralPath $new) { [void]$r.Left.Add("open-homm5" + $ext); continue }
        Move-Item -LiteralPath $old -Destination $new
        [void]$r.Moved.Add("open-homm5" + $ext)
    }
    # the installer's copies of the settings follow the settings file (not when it stayed)
    if (-not $r.Left.Contains("open-homm5.ini")) {
        foreach ($ext in @(".bak", ".bak2", ".bak3")) {
            $old = Join-Path $Bin ("open-homm5.ini" + $ext)
            $new = Join-Path $Bin ("h5-together.ini" + $ext)
            if ((Test-Path -LiteralPath $old -PathType Leaf) -and -not (Test-Path -LiteralPath $new)) { Move-Item -LiteralPath $old -Destination $new }
        }
    }
    $oldLog = Join-Path $Bin "open-homm5.log"
    if (Test-Path -LiteralPath $oldLog -PathType Leaf) {
        $older = Join-Path $Bin "h5-together.old.log"
        if (-not (Test-Path -LiteralPath (Join-Path $Bin "h5-together.log")) -and -not (Test-Path -LiteralPath $older)) {
            Move-Item -LiteralPath $oldLog -Destination $older
            [void]$r.Moved.Add("open-homm5.log")
        } else {
            Remove-Item -LiteralPath $oldLog -Force
        }
    }
    $oldOldLog = Join-Path $Bin "open-homm5.old.log"
    if (Test-Path -LiteralPath $oldOldLog -PathType Leaf) { Remove-Item -LiteralPath $oldOldLog -Force }
    $oldDir = Join-Path $Bin "open-homm5"
    $newDir = Join-Path $Bin "h5-together"
    if (Test-Path -LiteralPath $oldDir -PathType Container) {
        if (-not (Test-Path -LiteralPath $newDir)) {
            Move-Item -LiteralPath $oldDir -Destination $newDir
        } else {
            foreach ($name in $Templates) {
                $p = Join-Path $oldDir ("templates\" + $name)
                if (Test-Path -LiteralPath $p -PathType Leaf) { Remove-Item -LiteralPath $p -Force }
            }
            $r.FolderLeft = Merge-Folder $oldDir $newDir
        }
        [void]$r.Moved.Add("open-homm5")
    }
    return $r
}

try {
    Write-Host ""
    Write-Host "H5 Together installer"
    Write-Host "---------------------"

    $srcDll = Join-Path $PSScriptRoot "dinput8.dll"
    $srcIni = Join-Path $PSScriptRoot "h5-together.ini"
    $srcTemplates = Join-Path $PSScriptRoot "h5-together\templates"
    $templateFiles = @()
    if (Test-Path -LiteralPath $srcTemplates) {
        $templateFiles = @(Get-ChildItem -LiteralPath $srcTemplates -File | Where-Object { $_.Extension -ieq ".xdb" })
    }
    $srcTexts = Join-Path $PSScriptRoot "h5-together\texts"
    $textFiles = @()
    if (Test-Path -LiteralPath $srcTexts) {
        $textFiles = @(Get-ChildItem -LiteralPath $srcTexts -File | Where-Object { $_.Extension -ieq ".txt" })
    }
    if (-not (Test-Path -LiteralPath $srcDll) -or -not (Test-Path -LiteralPath $srcIni) -or $templateFiles.Count -eq 0 -or $textFiles.Count -eq 0) {
        Write-Host "dinput8.dll, h5-together.ini and the folders h5-together\templates and h5-together\texts" -ForegroundColor Red
        Write-Host "must be next to this script." -ForegroundColor Red
        Write-Host "Please extract the whole zip first (right-click the zip, Extract All)," -ForegroundColor Red
        Write-Host "then run install.bat from the new folder. Nothing was installed." -ForegroundColor Red
        Wait-AndExit 7
    }
    if (-not (Test-IsOurs $srcDll)) {
        Write-Host "The dinput8.dll in this package is not the H5 Together one. The download may be damaged." -ForegroundColor Red
        Write-Host "Nothing was installed." -ForegroundColor Red
        Wait-AndExit 7
    }

    $bin = Select-GameBin
    if (-not $bin) {
        Write-Host ""
        Write-Host "No game folder, so nothing was installed."
        Wait-AndExit 4
    }
    Write-Host "Game folder: $bin"

    $dstDll = Join-Path $bin "dinput8.dll"
    $dstIni = Join-Path $bin "h5-together.ini"
    $dstTemplates = Join-Path $bin "h5-together\templates"
    $dstTexts = Join-Path $bin "h5-together\texts"

    # Never overwrite a dinput8.dll that belongs to something else.
    $dllWas = "new"
    if (Test-Path -LiteralPath $dstDll) {
        if (-not (Test-IsOurs $dstDll)) {
            Write-Host ""
            Write-Host "Stopped. There is already a file called dinput8.dll in the game folder," -ForegroundColor Red
            Write-Host "and it is not the H5 Together one (another add-on may use it)." -ForegroundColor Red
            Write-Host "I did not change anything. If you want H5 Together, move that file away" -ForegroundColor Red
            Write-Host "yourself and run this installer again." -ForegroundColor Red
            Wait-AndExit 3
        }
        $same = (Get-Sha256 $srcDll) -eq (Get-Sha256 $dstDll)
        if ($same) { $dllWas = "same" } else { $dllWas = "older" }
    }

    # The files of the old name (open-homm5, until 0.2.7) get the new name first, so that
    # the settings refresh and the templates below work on them.
    try {
        $renamed = Move-OldNames $bin @($templateFiles | ForEach-Object { $_.Name })
    } catch {
        Write-Host ""
        Write-Host "Could not give your open-homm5 files their new name (h5-together):" -ForegroundColor Red
        Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
        Write-Host "If the game is running, close it and try again. Nothing else was changed." -ForegroundColor Red
        Wait-AndExit 5
    }

    try {
        Copy-Item -LiteralPath $srcDll -Destination $dstDll -Force
        # A zip from the web marks its files as "downloaded"; take that mark off the copy.
        try { Unblock-File -LiteralPath $dstDll } catch { }
        $iniWas = "old"
        if (-not (Test-Path -LiteralPath $dstIni)) {
            Copy-Item -LiteralPath $srcIni -Destination $dstIni
            $iniWas = "new"
        }
        # Our random map templates: overwrite our own OH-*.xdb files, leave every other file alone.
        [void](New-Item -ItemType Directory -Path $dstTemplates -Force)
        $ours = @{}
        $tplReplaced = 0
        foreach ($f in $templateFiles) {
            $ours[$f.Name] = $true
            $dst = Join-Path $dstTemplates $f.Name
            if (Test-Path -LiteralPath $dst) { $tplReplaced++ }
            Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
        }
        $tplOthers = @(Get-ChildItem -LiteralPath $dstTemplates -File | Where-Object { -not $ours.ContainsKey($_.Name) }).Count
        # Our language files: the same way (a translation of the player's own stays).
        [void](New-Item -ItemType Directory -Path $dstTexts -Force)
        $oursTxt = @{}
        $txtReplaced = 0
        foreach ($f in $textFiles) {
            $oursTxt[$f.Name] = $true
            $dst = Join-Path $dstTexts $f.Name
            if (Test-Path -LiteralPath $dst) { $txtReplaced++ }
            Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
        }
        $txtOthers = @(Get-ChildItem -LiteralPath $dstTexts -File | Where-Object { -not $oursTxt.ContainsKey($_.Name) }).Count
    } catch {
        Write-Host ""
        Write-Host "Could not copy the files into the game folder:" -ForegroundColor Red
        Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
        Write-Host "If the game is running, close it and try again. If Windows says access is" -ForegroundColor Red
        Write-Host "denied, right-click install.bat and choose Run as administrator." -ForegroundColor Red
        Wait-AndExit 5
    }

    # The player's settings file: add this version's settings, keep the player's values.
    $iniError = ""
    if ($iniWas -eq "old") {
        try {
            $oldBytes = [System.IO.File]::ReadAllBytes($dstIni)
            $merged = Merge-Settings $oldBytes ([System.IO.File]::ReadAllBytes($srcIni))
            if ($null -eq $merged) {
                $iniWas = "unreadable"
            } elseif ([string]::Equals($Latin1.GetString($merged.Bytes), $Latin1.GetString($oldBytes))) {
                $iniWas = "current"
            } else {
                Save-IniBackup $dstIni
                [System.IO.File]::WriteAllBytes($dstIni, $merged.Bytes)
                $iniWas = "refreshed"
            }
        } catch {
            $iniWas = "failed"
            $iniError = $_.Exception.Message
        }
    }

    Write-Host ""
    Write-Host "Done. What I did:" -ForegroundColor Green
    if ($renamed.Moved.Count -gt 0) {
        $what = @()
        if ($renamed.Moved.Contains("open-homm5.ini")) { $what += "the settings" }
        if ($renamed.Moved.Contains("open-homm5-skins.ini")) { $what += "the hero looks" }
        if ($renamed.Moved.Contains("open-homm5.log")) { $what += "the log" }
        if ($renamed.Moved.Contains("open-homm5")) { $what += "the folder" }
        Write-Host ("  - open-homm5 is now called H5 Together: your files got the new name, h5-together (" + ($what -join ", ") + ")")
    }
    foreach ($name in $renamed.Left) {
        Write-Host ("  - kept {0} as it is: there is already a {1}, which is the one used" -f $name, ($name -replace '^open-homm5', 'h5-together')) -ForegroundColor Yellow
    }
    if ($renamed.FolderLeft -gt 0) {
        Write-Host ("  - left {0} file(s) in the folder open-homm5: the folder h5-together has them too" -f $renamed.FolderLeft) -ForegroundColor Yellow
    }
    if ($dllWas -eq "new") { Write-Host "  - added dinput8.dll (the add-on)" }
    elseif ($dllWas -eq "same") { Write-Host "  - dinput8.dll was already installed and up to date" }
    else { Write-Host "  - replaced the older H5 Together dinput8.dll with this version" }
    if ($iniWas -eq "new") { Write-Host "  - added h5-together.ini (settings, all at their defaults)" }
    elseif ($iniWas -eq "refreshed") {
        $newText = Get-Count $merged.New "new setting" "new settings"
        Write-Host ("  - settings file refreshed: {0} of your settings kept, {1} available (see the comments)" -f $merged.Settings, $newText)
        Write-Host "    (your previous file is saved as h5-together.ini.bak)"
        if ($merged.Unknown -gt 0) {
            $unknownText = Get-Count $merged.Unknown "setting" "settings"
            Write-Host ("    ({0} not listed in this version: kept at the end of their section, under ""kept from your previous file"")" -f $unknownText)
        }
    }
    elseif ($iniWas -eq "current") {
        if ($merged.Settings -gt 0) { Write-Host ("  - settings file already up to date: {0} of your settings kept" -f $merged.Settings) }
        else { Write-Host "  - settings file already up to date (every setting at its default)" }
    }
    elseif ($iniWas -eq "unreadable") {
        Write-Host "  - kept h5-together.ini as it is: it is not a plain text file, so I could not add the new settings" -ForegroundColor Yellow
    }
    else {
        Write-Host ("  - could not refresh h5-together.ini: " + $iniError) -ForegroundColor Yellow
        Write-Host "    If the game is running, close it and run the installer again." -ForegroundColor Yellow
    }
    Write-Host ("  - installed {0} random map templates in h5-together\templates (they are in the game's random map list)" -f $templateFiles.Count)
    if ($tplReplaced -gt 0) { Write-Host ("    ({0} of them replaced our files from an earlier install)" -f $tplReplaced) }
    if ($tplOthers -gt 0) { Write-Host ("    (left the {0} other file(s) in that folder alone)" -f $tplOthers) }
    Write-Host "    Everyone in a multiplayer game needs the same templates: install the same version."
    $codes = ($textFiles | ForEach-Object { $_.BaseName }) -join ", "
    Write-Host ("  - installed {0} language files in h5-together\texts ({1}): the words of the add-on's panels" -f $textFiles.Count, $codes)
    if ($txtReplaced -gt 0) { Write-Host ("    ({0} of them replaced our files from an earlier install)" -f $txtReplaced) }
    if ($txtOthers -gt 0) { Write-Host ("    (left the {0} other file(s) in that folder alone)" -f $txtOthers) }
    Write-Host "  - did not change or replace any file of the game itself"
    if (-not (Test-Path -LiteralPath (Join-Path $bin "MMH55_Utility_64.exe"))) {
        Write-Host ""
        Write-Host "Note: there is no MMH55_Utility_64.exe in this folder. The add-on is made for" -ForegroundColor Yellow
        Write-Host "MMH5.5 (the co-op version of the game). Install MMH5.5 first." -ForegroundColor Yellow
    }
    Write-Host ""
    Write-Host "How to check: start the game as you do for co-op games (GameRanger, or"
    Write-Host "MMH55_Utility_64.exe). A file named h5-together.log then appears in:"
    Write-Host "  $bin"
    Write-Host "If you see it, the add-on is running. Everyone in the same co-op game should install it."
    Write-Host "To remove it later, run uninstall.bat."

    $publicNets = @(Get-PublicZeroTier)
    if ($publicNets.Count -gt 0) {
        Write-Host ""
        Write-Host ("Heads-up: Windows treats your ZeroTier network (" + ($publicNets -join ", ") + ") as Public.") -ForegroundColor Yellow
        Write-Host "The game's firewall rule only works on Private networks, so friends may not see your game." -ForegroundColor Yellow
        Write-Host "To fix it: Settings > Network and Internet > that ZeroTier network > Network profile type: Private." -ForegroundColor Yellow
        Write-Host "(I did not change anything in Windows.)" -ForegroundColor Yellow
    }

    Wait-AndExit 0
} catch {
    Write-Host ""
    Write-Host "Something unexpected went wrong:" -ForegroundColor Red
    Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
    Write-Host "You can send this text to the project page (see README.txt)." -ForegroundColor Red
    Wait-AndExit 6
}
