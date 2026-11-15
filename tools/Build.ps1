param(
	[ValidateSet('Debug', 'Release')][string[]] $Configuration = @('Debug', 'Release'),
	[ValidateSet('x86', 'x64', 'arm64')][string[]] $Architecture = @('x86', 'x64', 'arm64'),
	[switch] $Rebuild
)

$ErrorActionPreference = 'Stop'
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
		$log = Join-Path $logs "build-$arch-$config.log"
		& $msbuild (Join-Path $root 'PlainPDF.sln') -nologo -m "-target:$target" "-p:Platform=$($platforms[$arch])" "-p:Configuration=$config" "-flp:logfile=$log;verbosity=normal" -verbosity:minimal
		if ($LASTEXITCODE -ne 0)
		{
			throw "$config $arch build failed. See $log."
		}
	}
}
