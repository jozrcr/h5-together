# H5 Together uninstaller for Windows. Runs on Windows PowerShell 5.1.
#
# Removes only what install.ps1 put in the game's bin folder:
#   dinput8.dll (only if it is ours), h5-together.log (and h5-together.old.log), h5-together.ini (after asking),
#   and the random map templates of this package (the OH-*.xdb files whose names are in
#   this package's h5-together\templates folder), the language files of this package
#   (h5-together\texts, the same names as in the package), then the folders if they are empty.
# The same files under the add-on's old name (open-homm5, until 0.2.7) go too.
# Nothing else is touched: a template or a translation of your own stays.
#
# Keep this file plain ASCII (see install.ps1 for why).
#
# Options, for tests and odd setups:
#   -GameDir <folder>   use this game folder (or its bin folder), skip the search
#   -SteamDir <folder>  look in this Steam folder instead of the one in the registry
#   -NoPause            do not wait for Enter at the end
#
# Exit codes: 0 done, 4 game not found, 5 could not delete a file, 6 unexpected error.
param(
    [string]$GameDir = "",
    [string]$SteamDir = "",
    [switch]$NoPause
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

try {
    Write-Host ""
    Write-Host "H5 Together uninstaller"
    Write-Host "-----------------------"

    $bin = Select-GameBin
    if (-not $bin) {
        Write-Host ""
        Write-Host "No game folder, so nothing was removed."
        Wait-AndExit 4
    }
    Write-Host "Game folder: $bin"
    Write-Host ""

    $failed = $false

    $dll = Join-Path $bin "dinput8.dll"
    if (-not (Test-Path -LiteralPath $dll)) {
        Write-Host "  - no dinput8.dll here, nothing to remove"
    } elseif (-not (Test-IsOurs $dll)) {
        Write-Host "  - dinput8.dll is NOT the H5 Together one, so I left it alone" -ForegroundColor Yellow
    } else {
        try {
            Remove-Item -LiteralPath $dll -Force
            Write-Host "  - removed dinput8.dll"
        } catch {
            Write-Host ("  - could not remove dinput8.dll: " + $_.Exception.Message) -ForegroundColor Red
            Write-Host "    If the game is running, close it and run this again." -ForegroundColor Red
            $failed = $true
        }
    }

    # The logs, under the add-on's name and its old one (open-homm5, until 0.2.7). The log is
    # kept to 5 MB: when it got bigger, the game kept the last full one as h5-together.old.log.
    $logs = @("h5-together.log", "h5-together.old.log", "open-homm5.log", "open-homm5.old.log")
    if (-not (Test-Path -LiteralPath (Join-Path $bin "h5-together.log")) -and -not (Test-Path -LiteralPath (Join-Path $bin "open-homm5.log"))) {
        Write-Host "  - no h5-together.log here"
    }
    foreach ($name in $logs) {
        $log = Join-Path $bin $name
        if (-not (Test-Path -LiteralPath $log)) { continue }
        try {
            Remove-Item -LiteralPath $log -Force
            Write-Host "  - removed $name"
        } catch {
            Write-Host ("  - could not remove " + $name + ": " + $_.Exception.Message) -ForegroundColor Red
            $failed = $true
        }
    }

    # The settings file, and the copies of it that the installer keeps when it adds new
    # settings (.bak, .bak2, .bak3), under both names. One question for all of them.
    $inis = @(@("h5-together.ini", "open-homm5.ini") | Where-Object { Test-Path -LiteralPath (Join-Path $bin $_) -PathType Leaf })
    $backups = @()
    foreach ($base in @("h5-together.ini", "open-homm5.ini")) {
        $backups += @(@(".bak", ".bak2", ".bak3") | ForEach-Object { Join-Path $bin ($base + $_) } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    }
    if ($inis.Count -eq 0 -and $backups.Count -eq 0) {
        Write-Host "  - no h5-together.ini here"
    } else {
        $iniText = $inis -join " and "
        if ($inis.Count -eq 0) {
            $copied = (Split-Path -Leaf $backups[0]) -replace '\.bak\d?$', ''
            $question = "Also delete the old copies of your settings ($copied.bak)? [y/N]"; $what = "the old copies of $copied"
        }
        elseif ($inis.Count -gt 1) { $question = "Also delete your settings files $iniText, and their old copies (.bak)? [y/N]"; $what = "$iniText and their old copies" }
        elseif ($backups.Count -gt 0) { $question = "Also delete your settings file $iniText and its old copies (.bak)? [y/N]"; $what = "$iniText and its old copies" }
        else { $question = "Also delete your settings file $iniText? [y/N]"; $what = $iniText }
        $answer = $null
        try { $answer = Read-Host $question } catch { }
        if ($answer -and $answer.Trim() -match '^(y|yes|o|oui)$') {
            $files = @($inis | ForEach-Object { Join-Path $bin $_ }) + $backups
            foreach ($f in $files) {
                $name = Split-Path -Leaf $f
                try {
                    Remove-Item -LiteralPath $f -Force
                    Write-Host "  - removed $name"
                } catch {
                    Write-Host ("  - could not remove " + $name + ": " + $_.Exception.Message) -ForegroundColor Red
                    $failed = $true
                }
            }
        } else {
            Write-Host "  - kept $what"
        }
    }

    # The language files: only the names this package holds; a translation of the
    # player's own (another name) stays. Before the templates, so that the folder
    # h5-together can go once both are empty.
    $srcTexts = Join-Path $PSScriptRoot "h5-together\texts"
    $txtNames = @()
    if (Test-Path -LiteralPath $srcTexts) {
        $txtNames = @(Get-ChildItem -LiteralPath $srcTexts -File | Where-Object { $_.Extension -ieq ".txt" } | ForEach-Object { $_.Name })
    }
    $ourDir = Join-Path $bin "h5-together"
    $dstTexts = Join-Path $ourDir "texts"
    if (Test-Path -LiteralPath $dstTexts) {
        if ($txtNames.Count -eq 0) {
            Write-Host "  - this folder has no list of our language files, so I left h5-together\texts alone" -ForegroundColor Yellow
        } else {
            $removed = 0
            foreach ($n in $txtNames) {
                $p = Join-Path $dstTexts $n
                if (Test-Path -LiteralPath $p -PathType Leaf) {
                    try {
                        Remove-Item -LiteralPath $p -Force
                        $removed++
                    } catch {
                        Write-Host ("  - could not remove " + $n + ": " + $_.Exception.Message) -ForegroundColor Red
                        $failed = $true
                    }
                }
            }
            Write-Host ("  - removed {0} language files (h5-together\texts)" -f $removed)
            $left = @(Get-ChildItem -LiteralPath $dstTexts -Force)
            if ($left.Count -gt 0) {
                Write-Host ("  - kept {0} other file(s) in h5-together\texts (not ours)" -f $left.Count)
            } else {
                try {
                    Remove-Item -LiteralPath $dstTexts -Force
                    if (@(Get-ChildItem -LiteralPath $ourDir -Force).Count -eq 0) { Remove-Item -LiteralPath $ourDir -Force }
                    Write-Host "  - removed the empty folder h5-together\texts"
                } catch {
                    Write-Host ("  - could not remove the empty folder: " + $_.Exception.Message) -ForegroundColor Red
                    $failed = $true
                }
            }
        }
    }

    # The random map templates: only the names this package holds, never another file. In
    # our folder, and in its old name (open-homm5, until 0.2.7) if that is still there.
    $srcTemplates = Join-Path $PSScriptRoot "h5-together\templates"
    $tplNames = @()
    if (Test-Path -LiteralPath $srcTemplates) {
        $tplNames = @(Get-ChildItem -LiteralPath $srcTemplates -File | Where-Object { $_.Extension -ieq ".xdb" } | ForEach-Object { $_.Name })
    }
    $anyTemplates = $false
    foreach ($folder in @("h5-together", "open-homm5")) {
        $dstDir = Join-Path $bin $folder
        $dstTemplates = Join-Path $dstDir "templates"
        if (-not (Test-Path -LiteralPath $dstTemplates)) { continue }
        $anyTemplates = $true
        $from = ""
        if ($folder -ne "h5-together") { $from = " from $folder\templates (the folder's old name)" }
        if ($tplNames.Count -eq 0) {
            Write-Host "  - this folder has no list of our templates, so I left $folder\templates alone" -ForegroundColor Yellow
            continue
        }
        $removed = 0
        foreach ($n in $tplNames) {
            $p = Join-Path $dstTemplates $n
            if (Test-Path -LiteralPath $p -PathType Leaf) {
                try {
                    Remove-Item -LiteralPath $p -Force
                    $removed++
                } catch {
                    Write-Host ("  - could not remove " + $n + ": " + $_.Exception.Message) -ForegroundColor Red
                    $failed = $true
                }
            }
        }
        Write-Host ("  - removed {0} random map templates{1}" -f $removed, $from)
        $left = @(Get-ChildItem -LiteralPath $dstTemplates -Force)
        if ($left.Count -gt 0) {
            Write-Host ("  - kept {0} other file(s) in {1}\templates (not ours)" -f $left.Count, $folder)
        } else {
            try {
                Remove-Item -LiteralPath $dstTemplates -Force
                if (@(Get-ChildItem -LiteralPath $dstDir -Force).Count -eq 0) { Remove-Item -LiteralPath $dstDir -Force }
                Write-Host "  - removed the empty folder $folder\templates"
            } catch {
                Write-Host ("  - could not remove the empty folder: " + $_.Exception.Message) -ForegroundColor Red
                $failed = $true
            }
        }
    }
    if (-not $anyTemplates) { Write-Host "  - no random map templates here" }

    Write-Host ""
    if ($failed) {
        Write-Host "Some files could not be removed (see above)." -ForegroundColor Red
        Wait-AndExit 5
    }
    Write-Host "Done. No other file was touched." -ForegroundColor Green
    Wait-AndExit 0
} catch {
    Write-Host ""
    Write-Host "Something unexpected went wrong:" -ForegroundColor Red
    Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
    Wait-AndExit 6
}
