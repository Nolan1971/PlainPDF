param(
	[ValidateSet('Debug', 'Release')][string[]] $Configuration = @('Debug', 'Release'),
	[ValidateSet('x86', 'x64', 'arm64')][string[]] $Architecture = @('x86', 'x64', 'arm64'),
	[switch] $Rebuild,
	[switch] $AddressSanitizer,
	[switch] $Analyze
)

$ErrorActionPreference = 'Stop'
if ($AddressSanitizer -and ($Architecture -contains 'arm64' -or $Configuration -contains 'Debug'))
{
	throw 'Select Release x86 and/or x64 for the local AddressSanitizer build.'
}
$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (!$installation)
{
	throw 'Visual Studio 2022 with MSBuild is required.'
}
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$platforms = @{ x86 = 'Win32'; x64 = 'x64'; arm64 = 'ARM64' }
$target = 'Build'
if ($Rebuild)
{
	$target = 'Rebuild'
}
$logs = Join-Path $root 'out/logs'
[void] [IO.Directory]::CreateDirectory($logs)

foreach ($arch in $Architecture)
{
	foreach ($config in $Configuration)
	{
		$options = @()
		$label = 'build'
		if ($AddressSanitizer)
		{
			$label = 'asan'
			$options += '-p:EnableASAN=true', '-p:CopyAsanBinariesToOutDir=true', '-p:LinkIncremental=false'
		}
		if ($Analyze)
		{
			$label = if ($AddressSanitizer) { 'asan-analysis' } else { 'analysis' }
			$options += '-p:RunCodeAnalysis=true'
		}
		if ($label -ne 'build')
		{
			$options += "-p:OutDir=$root/out/$label/$arch/$config/"
			$options += "-p:IntDir=$root/out/$label/obj/$arch/$config/"
		}
		$log = Join-Path $logs "$label-$arch-$config.log"
		& $msbuild (Join-Path $root 'PlainPDF.sln') -nologo -m "-target:$target" "-p:Platform=$($platforms[$arch])" "-p:Configuration=$config" "-flp:logfile=$log;verbosity=normal" -verbosity:minimal @options
		if ($LASTEXITCODE -ne 0)
		{
			throw "$config $arch build failed. See $log."
		}
	}
}
