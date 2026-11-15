param(
	[ValidateSet('x86', 'x64', 'arm64')]
	[string[]] $Architecture = @('x86', 'x64', 'arm64')
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'third_party/pdfium/manifest.json') |
	ConvertFrom-Json
$downloads = Join-Path $root 'out/downloads'
$vendor = Join-Path $root 'out/pdfium'

function Get-VerifiedAsset($name, $sha256)
{
	$archive = Join-Path $downloads $name
	if (!(Test-Path -LiteralPath $archive))
	{
		$tempArchive = Join-Path $downloads ($name + '.' + [guid]::NewGuid() + '.part')
		try
		{
			& curl.exe --fail --location --silent --show-error --output $tempArchive "$($manifest.downloadBase)/$name"
			if ($LASTEXITCODE -ne 0)
			{
				throw "Download failed for $name"
			}
			if ((Get-FileHash -LiteralPath $tempArchive -Algorithm SHA256).Hash -ne $sha256)
			{
				throw "SHA-256 mismatch for $name"
			}
			[IO.File]::Move($tempArchive, $archive)
		}
		finally
		{
			if (Test-Path -LiteralPath $tempArchive)
			{
				[IO.File]::Delete($tempArchive)
			}
		}
	}
	if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $sha256)
	{
		throw "SHA-256 mismatch for $archive. Remove the damaged download and retry."
	}
	return $archive
}

[void] [IO.Directory]::CreateDirectory($downloads)
[void] [IO.Directory]::CreateDirectory($vendor)
$attestation = Get-VerifiedAsset 'pdfium-attestation.json' $manifest.attestation.sha256
$envelope = Get-Content -Raw -LiteralPath $attestation | ConvertFrom-Json
$statement = [Text.Encoding]::UTF8.GetString(
	[Convert]::FromBase64String($envelope.dsseEnvelope.payload)) | ConvertFrom-Json
$dependency = $statement.predicate.buildDefinition.resolvedDependencies |
	Where-Object uri -eq "git+https://github.com/$($manifest.publisher)@refs/heads/master"
if ($dependency.digest.gitCommit -ne $manifest.packagingCommit)
{
	throw 'Attestation packaging revision does not match the manifest.'
}

foreach ($arch in $Architecture)
{
	$artifact = $manifest.artifacts.$arch
	$subject = $statement.subject | Where-Object name -eq $artifact.name
	if ($subject.digest.sha256 -ne $artifact.sha256)
	{
		throw "Attestation digest does not match the manifest for $arch."
	}

	$archive = Get-VerifiedAsset $artifact.name $artifact.sha256
	if ((Get-Item -LiteralPath $archive).Length -ne $artifact.size)
	{
		throw "Unexpected archive size for $arch."
	}
	$destination = Join-Path $vendor $arch
	[void] [IO.Directory]::CreateDirectory($destination)
	& tar.exe -xzf $archive -C $destination
	if ($LASTEXITCODE -ne 0)
	{
		throw "Extraction failed for $arch."
	}

	$dll = Join-Path $destination 'bin/pdfium.dll'
	$binary = [IO.BinaryReader]::new([IO.File]::OpenRead($dll))
	try
	{
		if ($binary.ReadUInt16() -ne 0x5A4D)
		{
			throw "Invalid DOS header in $dll."
		}
		$binary.BaseStream.Position = 0x3C
		$binary.BaseStream.Position = $binary.ReadUInt32()
		if ($binary.ReadUInt32() -ne 0x4550 -or $binary.ReadUInt16() -ne $artifact.machine)
		{
			throw "Invalid PE header or wrong architecture in $dll."
		}
	}
	finally
	{
		$binary.Dispose()
	}

	foreach ($name in 'lib/pdfium.dll.lib', 'include/fpdfview.h', 'LICENSE')
	{
		if (!(Test-Path -LiteralPath (Join-Path $destination $name) -PathType Leaf))
		{
			throw "Missing $name in verified $arch payload."
		}
	}
	Write-Output "Verified PDFium $($manifest.version) $arch at $destination"
}
