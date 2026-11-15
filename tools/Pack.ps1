param(
	[ValidateSet('x86', 'x64', 'arm64')]
	[string[]] $Architecture = @('x86', 'x64', 'arm64'),
	[string] $Output,
	[string] $MakeNsis,
	[ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?\z')]
	[string] $Version = '1.0.0-rc.2'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$Output)
{
	$Output = Join-Path (Split-Path -Parent $root) "PlainPDF-Artifacts/$Version"
}
$outputPath = [IO.Path]::GetFullPath($Output)

if (!$MakeNsis)
{
	$tool = Get-Command makensis.exe -ErrorAction SilentlyContinue
	if ($tool)
	{
		$MakeNsis = $tool.Source
	}
	else
	{
		$MakeNsis = Join-Path $root 'out/tools/nsis/makensis.exe'
	}
}
if (!(Test-Path -LiteralPath $MakeNsis -PathType Leaf))
{
	throw 'Specify -MakeNsis or run tools/Get-NSIS.ps1.'
}
$MakeNsis = (Resolve-Path -LiteralPath $MakeNsis).Path
[void] [IO.Directory]::CreateDirectory($outputPath)
$run = Join-Path $root ('out/package/' + [guid]::NewGuid())
[void] [IO.Directory]::CreateDirectory($run)
$artifacts = [Collections.Generic.List[string]]::new()

foreach ($arch in $Architecture)
{
	$stage = Join-Path $run $arch
	$release = Join-Path $root "out/build/$arch/Release"
	$pdfium = Join-Path $root "out/pdfium/$arch"
	$pdfiumNotices = Join-Path $stage 'licenses/pdfium'
	[void] [IO.Directory]::CreateDirectory($pdfiumNotices)

	foreach ($name in 'PlainPDF.exe', 'pdfium.dll')
	{
		Copy-Item -LiteralPath (Join-Path $release $name) -Destination $stage
	}
	$expected = (Get-FileHash -LiteralPath (Join-Path $pdfium 'bin/pdfium.dll')).Hash
	$actual = (Get-FileHash -LiteralPath (Join-Path $stage 'pdfium.dll')).Hash
	if ($expected -ne $actual)
	{
		throw "The staged PDFium DLL does not match the pinned $arch dependency."
	}
	Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'LICENSE.txt')
	Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination (Join-Path $stage 'README.md')
	Copy-Item -LiteralPath (Join-Path $pdfium 'LICENSE'), (Join-Path $pdfium 'VERSION'),
		(Join-Path $pdfium 'args.gn'), (Join-Path $pdfium 'licenses') -Destination $pdfiumNotices -Recurse
	Copy-Item -LiteralPath (Join-Path $root 'third_party/pdfium/manifest.json') -Destination $pdfiumNotices

	$removeFiles = Join-Path $run "$arch-remove.nsh"
	$remove = foreach ($file in (Get-ChildItem -LiteralPath $stage -File -Recurse))
	{
		$relative = $file.FullName.Substring($stage.Length + 1)
		'Delete "$INSTDIR\' + $relative + '"'
	}
	$directories = Get-ChildItem -LiteralPath $stage -Directory -Recurse |
		Sort-Object { $_.FullName.Length } -Descending
	foreach ($directory in $directories)
	{
		$relative = $directory.FullName.Substring($stage.Length + 1)
		$remove += 'RMDir "$INSTDIR\' + $relative + '"'
	}
	[IO.File]::WriteAllLines($removeFiles, $remove, [Text.UTF8Encoding]::new($false))

	$name = "PlainPDF-$Version-$arch"
	$zip = Join-Path $outputPath "$name.zip"
	$setup = Join-Path $outputPath "$name-setup.exe"
	Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force
	& $MakeNsis /V3 "/DARCH=$arch" "/DSTAGE=$stage" "/DOUTPUT=$setup" "/DREMOVE_FILES=$removeFiles" (Join-Path $root 'installer/PlainPDF.nsi')
	if ($LASTEXITCODE -ne 0)
	{
		throw "NSIS failed for $arch."
	}
	$artifacts.Add($zip)
	$artifacts.Add($setup)
}

$hashes = foreach ($artifact in $artifacts)
{
	(Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant() +
		'  ' + [IO.Path]::GetFileName($artifact)
}
[IO.File]::WriteAllLines((Join-Path $outputPath 'SHA256SUMS.txt'), $hashes, [Text.UTF8Encoding]::new($false))
$artifacts
