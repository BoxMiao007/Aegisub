#!/usr/bin/env powershell

param (
  [Parameter(Position = 0, Mandatory = $false)]
  [string]$BuildRoot = $null,
  [Parameter(Position = 1, Mandatory = $false)]
  [string]$SourceRoot = $null
)

$lastSvnRevision = 6962
$lastSvnHash = '16cd907fe7482cb54a7374cd28b8501f138116be'
# Allow the fork suffix (-fork.N[-pre.date]) in the tag's pre-release part
$semVerMatch = [regex] 'v?(\d+)\.(\d+).(\d+)(?:-([0-9A-Za-z._-]+))?'

$repositoryRootPath = Join-Path $PSScriptRoot .. | Resolve-Path
if (!(git -C $repositoryRootPath rev-parse --is-inside-work-tree 2>$null)) {
  throw "$repositoryRootPath is not a git repository"
}

if ($BuildRoot -eq $null -or $BuildRoot.Trim() -eq "")  {
  $BuildRoot = $repositoryRootPath
}

# support legacy in-tree builds
if ([System.IO.Path]::GetFullPath([System.IO.Path]::Combine((pwd).Path, $BuildRoot)) -eq
  [System.IO.Path]::GetFullPath([System.IO.Path]::Combine((pwd).Path, $repositoryRootPath))) {
    $BuildRoot = Join-Path $repositoryRootPath 'build'
}
$gitVersionHeaderPath = Join-Path $BuildRoot 'git_version.h'

$gitRevision = $lastSvnRevision + ((git -C $repositoryRootPath log --pretty=oneline "$($lastSvnHash)..HEAD" 2>$null | Measure-Object).Count)
$gitBranch = [string](git -C $repositoryRootPath symbolic-ref --short HEAD 2>$null)
if ([string]::IsNullOrEmpty($gitBranch)) {
  $gitBranch = [Environment]::GetEnvironmentVariable('AEGISUB_BUILD_BRANCH')
}
$gitBranch = (($gitBranch -replace '[^A-Za-z0-9._-]', '-') -replace '-+', '-').Trim('-')
if ([string]::IsNullOrEmpty($gitBranch)) {
  $gitBranch = 'unnamed-branch'
}
$gitHash = git -C $repositoryRootPath rev-parse --short HEAD 2>$null

# Resolve the version before consulting the cached header. A commit can be
# built before its tag exists and again after tagging; if the cache check ran
# on the pre-tag revision string it would exit early and keep the wrong
# version in the built artifacts.
$gitVersionString = $gitRevision, $gitBranch, $gitHash -join '-'
$exactGitTag = git -C $repositoryRootPath describe --exact-match --tags 2>$null

$version = @{
  TAGGED_RELEASE = $false
  RESOURCE_BASE_VERSION = @(0, 0, 0)
  INSTALLER_VERSION = '0.0.0'
}

if ($exactGitTag -match $semVerMatch) {
  $version['TAGGED_RELEASE'] = $true
  $version['RESOURCE_BASE_VERSION'] = $Matches[1..3]
  $joinedVersion = $Matches[1..3] -join '.'
  $gitVersionString = $joinedVersion + @("-$($Matches[4])",'')[!$Matches[4]]
  $version['INSTALLER_VERSION'] = $joinedVersion
} else {
  foreach ($rev in (git -C $repositoryRootPath rev-list --tags 2>$null)) {
    $tag = git -C $repositoryRootPath describe --exact-match --tags $rev 2>$null
    if ($tag -match $semVerMatch) {#
      $version['RESOURCE_BASE_VERSION'] = $Matches[1..3]
      $version['INSTALLER_VERSION'] = ($Matches[1..3] -join '.')
      break;
    }
  }
}
$version['BUILD_GIT_VERSION_NUMBER'] = $gitRevision
$version['BUILD_GIT_VERSION_STRING'] = $gitVersionString

$lines = foreach ($key in ($version.Keys | Sort-Object)) {
  $value = $version[$key]
  $fmtValue = switch ($value.GetType()) {
    ([string]) {"`"$value`""}
    ([int]) {$value.ToString()}
    ([bool]) {([int]$value).ToString()}
    ([object[]]) {$value -join ', '}
    default {
      Write-Host "no format known for type '$($value.GetType())' - trying default string conversion" -ForegroundColor Red
      {"`"$($value.ToString())`""}
    }
  }
  "#define $key $fmtValue"
}
$text = ($lines -join "`n") + "`n"

# Only rewrite when the result changed, to avoid spurious rebuilds.
$current = if (Test-Path $gitVersionHeaderPath) { [System.IO.File]::ReadAllText($gitVersionHeaderPath) } else { '' }
$current = $current.TrimStart([char]0xFEFF).Replace("`r`n", "`n")
if ($current -ceq $text) {
  exit 0
}
[System.IO.File]::WriteAllText($gitVersionHeaderPath, $text, (New-Object System.Text.UTF8Encoding($false)))
