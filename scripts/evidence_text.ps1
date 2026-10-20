# Convert error messages before PowerShell adds call sites or wraps display text.
function ConvertTo-EvidenceText {
    param(
        [Parameter(ValueFromPipeline = $true)]
        [AllowNull()]
        [object]$InputObject,
        [string[]]$PrivatePaths = @()
    )
    process {
        $text = if ($InputObject -is [System.Management.Automation.ErrorRecord]) {
            $InputObject.Exception.Message
        } else { [string]$InputObject }
        $paths = @($PrivatePaths) + @($env:TEMP, $env:TMP, $env:TMPDIR, $env:USERPROFILE, $env:HOME, $env:APPDATA, $env:LOCALAPPDATA)
        foreach ($privatePath in ($paths | Where-Object { $_ -and [System.IO.Path]::IsPathRooted($_) } | Sort-Object Length -Descending)) {
            foreach ($variant in @($privatePath.Replace("\", "/"), $privatePath.Replace("/", "\"))) {
                $prefix = $variant.TrimEnd("/", "\")
                if ($prefix) { $text = [regex]::Replace($text, [regex]::Escape($prefix), "<local>", "IgnoreCase") }
            }
        }
        $text = [regex]::Replace($text, '(?i)[a-z]:[\\/][^\r\n"''<>|]*', '<path>')
        $text = [regex]::Replace($text, '\\\\[^\r\n"''<>|]+', '<path>')
        $text = [regex]::Replace($text, '(?<![\w/>:])/(?!/)[^\s"''<>|]+', '<path>')
        $text
    }
}
