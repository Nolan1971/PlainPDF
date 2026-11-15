$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs)
{
	throw 'The x64 MSVC tools are required.'
}
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$drop = Join-Path $root 'tests/DropPdf.cpp'
$printer = Join-Path $root 'tests/SelectPrinter.cpp'
$output = Join-Path $root 'out/test/tools'
[void] [IO.Directory]::CreateDirectory($output)
Push-Location -LiteralPath $output
try
{
	$batch = Join-Path $output 'compile.cmd'
	$lines = @(
		'@echo off',
		('call "' + $vcvars.Replace('%','%%') + '" >nul'),
		'if errorlevel 1 exit /b 1',
		('cl /nologo /W4 /EHsc /MT /std:c++20 /DUNICODE /D_UNICODE "' + $drop.Replace('%','%%') + '" /Fe:DropPdf.exe /link shell32.lib ole32.lib user32.lib uuid.lib'),
		'if errorlevel 1 exit /b 1',
		('cl /nologo /W4 /EHsc /MT /std:c++20 /DUNICODE /D_UNICODE "' + $printer.Replace('%','%%') + '" /Fe:SelectPrinter.exe /link oleacc.lib oleaut32.lib ole32.lib user32.lib uuid.lib')
	)
	[IO.File]::WriteAllLines($batch, $lines, [Text.Encoding]::Default)
	& $batch
	if ($LASTEXITCODE -ne 0)
	{
		throw 'Could not build the native UI test drivers.'
	}
}
finally
{
	Pop-Location
}
