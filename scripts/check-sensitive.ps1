$ErrorActionPreference = 'Stop'
$patterns = @(
  'sk-[A-Za-z0-9]{16,}',
  'sk-ant-[A-Za-z0-9_-]{16,}',
  'Bearer\s+[A-Za-z0-9._=-]{16,}',
  '(?i)(api[_-]?key|password|proxyPassword)\s*[:=]\s*["''][^"'']{12,}["'']'
)
$files = git ls-files | Where-Object { $_ -notmatch '^(build|build-|dist|tests)/' -and $_ -notmatch '^\.git/' }
$violations = @()
foreach ($file in $files) {
  $text = Get-Content -Raw -LiteralPath $file
  foreach ($pattern in $patterns) {
    if ($text -match $pattern) { $violations += "$file matches $pattern" }
  }
}
if ($violations.Count -gt 0) {
  $violations | ForEach-Object { Write-Error $_ }
  exit 1
}
Write-Host 'Sensitive material scan passed.'
