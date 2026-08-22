$ErrorActionPreference = 'Stop'
$patterns = @(
  '(?i)sk-[A-Za-z0-9]{20,}',
  '(?i)sk-ant-[A-Za-z0-9_-]{20,}',
  '(?i)Bearer\s+[A-Za-z0-9._=-]{20,}',
  '(?i)ghp_[A-Za-z0-9]{30,}',
  '(?i)github_pat_[A-Za-z0-9_]{30,}',
  '(?i)AKIA[0-9A-Z]{16}',
  '(?i)-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
  '(?i)(api[_-]?key|password|proxyPassword|secret|token)\s*[:=]\s*["''][^"'']{16,}["'']'
)
$extensions = @('.cpp','.h','.hpp','.c','.cc','.cmake','.md','.json','.ps1','.yml','.yaml','.xml','.qrc','.txt','.ini','.toml','.py','.js','.ts','.bat','.sh')
$files = @(git ls-files --cached --others --exclude-standard | Where-Object {
  $_ -notmatch '^(build|build-|dist|\.git)/' -and
  $extensions -contains [IO.Path]::GetExtension($_).ToLowerInvariant()
}) | Sort-Object -Unique
$fixtureAllowlist = @{
  'tests/tst_core.cpp' = @('Bearer sk-super-secret-value', 'Bearer secret-token-value')
}
$violations = @()
foreach ($file in $files) {
  $normalizedFile = $file -replace '\\', '/'
  if ($normalizedFile -eq 'scripts/check-sensitive.ps1') { continue } # Pattern definitions intentionally resemble credentials.
  if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { continue }
  $text = Get-Content -Raw -LiteralPath $file -ErrorAction Stop
  if ($fixtureAllowlist.ContainsKey($normalizedFile)) {
    foreach ($allowed in $fixtureAllowlist[$normalizedFile]) { $text = $text.Replace($allowed, 'TEST_FIXTURE') }
  }
  foreach ($pattern in $patterns) {
    if ($text -match $pattern) { $violations += "$file matches $pattern" }
  }
}
if ($violations.Count -gt 0) {
  $violations | ForEach-Object { Write-Error $_ }
  exit 1
}
Write-Host "Sensitive material scan passed ($($files.Count) text files checked)."
