# H5 Together bug report helper for Windows. Runs on Windows PowerShell 5.1.
#
# Makes h5-together-bug-report-<date>.zip on your Desktop, to attach to a bug report on
# GitHub. It holds what we need to look into a problem and nothing else:
#   h5-together.log and h5-together.old.log   what the add-on wrote
#   h5-together.ini, h5-together-skins.ini    your settings
#   the same four under the add-on's old name (open-homm5, until 0.2.7), if they are there
#   system.txt                                the add-on, MMH5.5 and Windows versions, a few file
#                                             sizes and hashes, the other DLLs in the game folder
# No saves, no maps, no game files. It sends nothing anywhere: the zip is yours to look at
# (it is a normal zip) and to attach yourself. The log lists player names and LAN addresses.
#
# It finds the game the way install.ps1 does. It works when the game does not start, and
# when the add-on is not installed (then there is no log, and the report says so).
#
# Keep this file plain ASCII (see install.ps1 for why).
#
# Normal use: double-click bug-report.bat. Options, for tests and odd setups:
#   -GameDir <folder>   use this game folder (or its bin folder), skip the search
#   -SteamDir <folder>  look in this Steam folder instead of the one in the registry
#   -OutDir <folder>    put the zip here instead of on the Desktop
#   -NoPause            do not wait for Enter at the end, and do not open the folder
#
# Exit codes: 0 done, 4 game not found, 5 could not write the zip, 6 unexpected error.
param(
    [string]$GameDir = "",
    [string]$SteamDir = "",
    [string]$OutDir = "",
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

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)

# The path with your home folder written as ~, so the report does not carry your user name.
function Get-ShortPath([string]$Path) {
    $profile_ = $env:USERPROFILE
    if ($profile_ -and $Path.StartsWith($profile_, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "~" + $Path.Substring($profile_.Length)
    }
    return $Path
}

# SHA-256 of a file as lower case hex, read so that a file in use is fine.
function Get-Sha256Hex([string]$Path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $fs = $null
    try {
        $fs = [System.IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        return ([System.BitConverter]::ToString($sha.ComputeHash($fs))).Replace("-", "").ToLowerInvariant()
    } catch {
        return "(could not be read)"
    } finally {
        if ($fs) { $fs.Dispose() }
        $sha.Dispose()
    }
}

# The name and version a dinput8.dll of ours says in its text, or $null: "H5 Together 0.3.0"
# from "H5 Together hook DLL 0.3.0 loaded" ("open-homm5 0.2.7": the add-on's old name).
function Get-AddonVersion([string]$Path) {
    try {
        $text = $Latin1.GetString([System.IO.File]::ReadAllBytes($Path))
        $m = [regex]::Match($text, '(H5 Together|open-homm5) hook DLL (\S+) loaded')
        if ($m.Success) { return ($m.Groups[1].Value + " " + $m.Groups[2].Value) }
    } catch { }
    return $null
}

# The text of scripts/H55-Version.lua in MMH55-Index.pak (a zip file), or $null.
function Get-Mmh55VersionScript([string]$Pak) {
    $fs = $null
    $zip = $null
    try {
        $fs = [System.IO.File]::Open($Pak, 'Open', 'Read', 'ReadWrite')
        $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Read)
        foreach ($e in $zip.Entries) {
            if ($e.FullName -ieq "scripts/H55-Version.lua") {
                $reader = New-Object System.IO.StreamReader($e.Open())
                try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
            }
        }
    } catch {
    } finally {
        if ($zip) { $zip.Dispose() } elseif ($fs) { $fs.Dispose() }
    }
    return $null
}

# 55u.dll is MMH5.5's runtime patcher, and it changes with every release; its hash tells
# the versions apart (docs/plans/mmh55-rc19k.md). H55_Version says "19e" in RC19k too.
$Known55u = @{
    "a6de006f9492f1e8af6eae19619b5a345536c443a5f2e4bb5e1e26947988085b" = "RC19d"
    "a28663da081b6e23f0c2fa4000117d0b5d1457ca3314e3735bca215c1d5d304d" = "RC19k"
}

function Format-Size([long]$Bytes) {
    if ($Bytes -ge 1048576) { return ("{0:N1} MB" -f ($Bytes / 1048576.0)) }
    if ($Bytes -ge 1024) { return ("{0:N0} KB" -f ($Bytes / 1024.0)) }
    return "$Bytes bytes"
}

# Where the zip goes: -OutDir, else the Desktop (also when OneDrive moved it), else your home folder.
function Get-OutFolder {
    if ($OutDir) {
        [void](New-Item -ItemType Directory -Path $OutDir -Force)
        return (Resolve-Path -LiteralPath $OutDir).Path
    }
    foreach ($special in @("Desktop", "UserProfile")) {
        $p = [Environment]::GetFolderPath($special)
        if ($p -and (Test-Path -LiteralPath $p -PathType Container)) { return $p }
    }
    return $PSScriptRoot
}

try {
    Write-Host ""
    Write-Host "H5 Together bug report"
    Write-Host "----------------------"

    $bin = Select-GameBin
    if (-not $bin) {
        Write-Host ""
        Write-Host "No game folder, so no bug report was made."
        Wait-AndExit 4
    }
    Write-Host "Game folder: $bin"
    $root = Split-Path -Parent $bin

    # The files that go in the zip: only these, and only the ones that exist (the last four:
    # the add-on's old name, until 0.2.7, for an install that was not updated yet).
    $pack = New-Object System.Collections.ArrayList
    foreach ($name in @("h5-together.log", "h5-together.old.log", "h5-together.ini", "h5-together-skins.ini",
                        "open-homm5.log", "open-homm5.old.log", "open-homm5.ini", "open-homm5-skins.ini")) {
        $p = Join-Path $bin $name
        if (Test-Path -LiteralPath $p -PathType Leaf) { [void]$pack.Add(@($name, $p)) }
    }
    $hasLog = $false
    foreach ($item in $pack) { if ($item[0] -eq "h5-together.log" -or $item[0] -eq "open-homm5.log") { $hasLog = $true } }

    # system.txt
    $t = New-Object System.Collections.Generic.List[string]
    $t.Add("H5 Together bug report")
    $t.Add("Made by bug-report.ps1 on " + [DateTime]::Now.ToString("yyyy-MM-dd HH:mm:ss", [Globalization.CultureInfo]::InvariantCulture) + ". Nothing was sent anywhere.")
    $t.Add("")
    $t.Add("Add-on")
    $dll = Join-Path $bin "dinput8.dll"
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
        $t.Add("  dinput8.dll in the game folder: none (the add-on is not installed here)")
    } elseif (-not (Test-IsOurs $dll)) {
        $t.Add("  dinput8.dll in the game folder: NOT the H5 Together one (another add-on uses that name)")
    } else {
        $v = Get-AddonVersion $dll
        if (-not $v) { $v = "an older build of the add-on, version not written in it" }
        $props = (Get-Item -LiteralPath $dll).VersionInfo
        $t.Add("  dinput8.dll in the game folder: " + $v + " (file properties: " + $(if ($props.ProductVersion) { $props.ProductVersion } else { "no version information" }) + ")")
    }
    $mine = Join-Path $PSScriptRoot "dinput8.dll"
    if (Test-Path -LiteralPath $mine -PathType Leaf) {
        $v = Get-AddonVersion $mine
        if (-not $v) { $v = "unknown" }
        $t.Add("  the package this script came from: " + $v)
    }
    foreach ($name in @("h5-together.nohooks", "open-homm5.nohooks")) {
        $t.Add("  " + $name + " (switches the hooks off): " + $(if (Test-Path -LiteralPath (Join-Path $bin $name)) { "present" } else { "not there" }))
    }
    $t.Add("")
    $t.Add("System")
    try {
        $cv = Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion" -ErrorAction Stop
        # (Windows 11 still says "Windows 10" in this registry value; its builds start at 22000)
        $winName = $cv.ProductName
        if ([int]$cv.CurrentBuild -ge 22000) { $winName = $winName -replace '^Windows 10', 'Windows 11' }
        $t.Add("  Windows: " + $winName + " " + $cv.DisplayVersion + ", build " + $cv.CurrentBuild + "." + $cv.UBR)
    } catch {
        $t.Add("  Windows: " + [Environment]::OSVersion.VersionString)
    }
    $t.Add("  64-bit Windows: " + [Environment]::Is64BitOperatingSystem)
    $t.Add("  language: " + (Get-UICulture).Name + " (formats: " + (Get-Culture).Name + ")")
    $t.Add("  PowerShell: " + $PSVersionTable.PSVersion)
    $t.Add("  game folder: " + (Get-ShortPath $bin))
    $t.Add("")
    $t.Add("MMH5.5")
    $pak = Join-Path $root "data\MMH55-Index.pak"
    if (Test-Path -LiteralPath $pak -PathType Leaf) {
        $lua = Get-Mmh55VersionScript $pak
        $m = $null
        if ($lua) { $m = [regex]::Match($lua, 'H55_Version\s*=\s*"([^"]*)"') }
        if ($m -and $m.Success) {
            $t.Add("  H55_Version in data\MMH55-Index.pak: " + $m.Groups[1].Value)
        } else {
            $t.Add("  data\MMH55-Index.pak is there, but I could not read the version from it")
        }
    } else {
        $t.Add("  data\MMH55-Index.pak: not found (MMH5.5 is not installed here, or the game folder is not the usual one)")
    }
    $u55 = Join-Path $bin "55u.dll"
    if (Test-Path -LiteralPath $u55 -PathType Leaf) {
        $h = Get-Sha256Hex $u55
        $label = "a version we have not tested"
        if ($Known55u.ContainsKey($h)) { $label = $Known55u[$h] }
        $t.Add("  55u.dll: " + $label + " (sha256 " + $h + ")")
    } else {
        $t.Add("  55u.dll: not found")
    }
    $t.Add("")
    $t.Add("Game exes in the bin folder (size, file version, sha256)")
    $anyExe = $false
    foreach ($name in @("H5_Game.exe", "H5_GameOLD.exe", "MMH55_Utility_64.exe", "MMH55_Utility.exe", "MMH55_64.exe", "MMH55.exe")) {
        $p = Join-Path $bin $name
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { continue }
        $anyExe = $true
        $fv = (Get-Item -LiteralPath $p).VersionInfo.FileVersion
        if (-not $fv) { $fv = "no version" }
        $t.Add("  " + $name + "  " + (Get-Item -LiteralPath $p).Length + " bytes  " + $fv + "  " + (Get-Sha256Hex $p))
    }
    if (-not $anyExe) { $t.Add("  none found") }
    $t.Add("")
    $t.Add("Other .dll and .asi files in the bin folder (name, size)")
    $others = @(Get-ChildItem -LiteralPath $bin -File | Where-Object { $_.Extension -ieq ".dll" -or $_.Extension -ieq ".asi" } | Sort-Object Name)
    foreach ($f in $others) { $t.Add("  " + $f.Name + "  " + $f.Length) }
    if ($others.Count -eq 0) { $t.Add("  none") }
    $t.Add("")
    $t.Add("Files in this zip")
    foreach ($item in $pack) { $t.Add("  " + $item[0] + "  " + (Get-Item -LiteralPath $item[1]).Length + " bytes") }
    if (-not $hasLog) { $t.Add("  (there is no h5-together.log in the game folder: the game was not started since the add-on was installed, or the add-on is not installed)") }
    $t.Add("  system.txt  (this file)")
    $systemText = ($t -join "`r`n") + "`r`n"

    # The zip.
    $stamp = [DateTime]::Now.ToString("yyyy-MM-dd-HHmm", [Globalization.CultureInfo]::InvariantCulture)
    try {
        $outFolder = Get-OutFolder
    } catch {
        Write-Host ""
        Write-Host "Could not use the folder for the bug report:" -ForegroundColor Red
        Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
        Write-Host "You can run it again with -OutDir <a folder you can write in>." -ForegroundColor Red
        Wait-AndExit 5
    }
    $zipPath = Join-Path $outFolder ("h5-together-bug-report-" + $stamp + ".zip")
    $zipFile = $null
    $zip = $null
    try {
        $zipFile = New-Object System.IO.FileStream($zipPath, [System.IO.FileMode]::Create)
        $zip = New-Object System.IO.Compression.ZipArchive($zipFile, [System.IO.Compression.ZipArchiveMode]::Create)
        foreach ($item in $pack) {
            $entry = $zip.CreateEntry($item[0], [System.IO.Compression.CompressionLevel]::Optimal)
            # shared read: the game may be running and writing its log
            $in = [System.IO.File]::Open($item[1], 'Open', 'Read', 'ReadWrite')
            try {
                $out = $entry.Open()
                try { $in.CopyTo($out) } finally { $out.Dispose() }
            } finally { $in.Dispose() }
        }
        $entry = $zip.CreateEntry("system.txt", [System.IO.Compression.CompressionLevel]::Optimal)
        $out = $entry.Open()
        try {
            $bytes = (New-Object System.Text.UTF8Encoding($true)).GetPreamble() + (New-Object System.Text.UTF8Encoding($false)).GetBytes($systemText)
            $out.Write($bytes, 0, $bytes.Length)
        } finally { $out.Dispose() }
        $zip.Dispose()
        $zip = $null
    } catch {
        $problem = $_.Exception.Message
        if ($zip) { try { $zip.Dispose() } catch { } } elseif ($zipFile) { try { $zipFile.Dispose() } catch { } }
        try { Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue } catch { }
        Write-Host ""
        Write-Host "Could not write the bug report:" -ForegroundColor Red
        Write-Host ("  " + $problem) -ForegroundColor Red
        Write-Host "You can run it again with -OutDir <folder>, or just attach h5-together.log from the bin folder." -ForegroundColor Red
        Wait-AndExit 5
    }

    Write-Host ""
    Write-Host "Done. The bug report is this file:" -ForegroundColor Green
    Write-Host "  $zipPath"
    Write-Host ("  ({0}; it holds: {1}, system.txt)" -f (Format-Size (Get-Item -LiteralPath $zipPath).Length), (($pack | ForEach-Object { $_[0] }) -join ", "))
    if (-not $hasLog) {
        Write-Host ""
        Write-Host "There is no h5-together.log in the game folder, so the report only tells about your installation." -ForegroundColor Yellow
        Write-Host "(Either the add-on is not installed here, or the game was not started since.)" -ForegroundColor Yellow
    }
    Write-Host ""
    Write-Host "Attach that file to your report on GitHub: https://github.com/jozrcr/h5-together/issues/new/choose"
    Write-Host "Nothing was sent anywhere. The log lists player names and LAN addresses; the zip is a normal"
    Write-Host "zip file, so you can open it and look before you send it."
    if (-not $NoPause) {
        try { Start-Process -FilePath "explorer.exe" -ArgumentList ("/select,`"" + $zipPath + "`"") } catch { }
    }

    Wait-AndExit 0
} catch {
    Write-Host ""
    Write-Host "Something unexpected went wrong:" -ForegroundColor Red
    Write-Host ("  " + $_.Exception.Message) -ForegroundColor Red
    Write-Host "You can send this text to the project page (see README.txt)." -ForegroundColor Red
    Wait-AndExit 6
}
