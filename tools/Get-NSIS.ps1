param([string] $SevenZip)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$downloadDir = Join-Path $root 'out/downloads'
$toolDir = Join-Path $root 'out/tools/nsis'
[void] [IO.Directory]::CreateDirectory($downloadDir)
[void] [IO.Directory]::CreateDirectory($toolDir)
$archive = Join-Path $downloadDir 'nsis-3.13-setup.exe'
$url = 'https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13-setup.exe'
$hash = 'DB9A1038BBC0E53FEF24F423FD609979BD578E942F1BE1198DB186D1C42652A3'
if (!$SevenZip)
{
	$command = Get-Command 7z.exe -ErrorAction SilentlyContinue
	if ($command)
	{
		$SevenZip = $command.Source
	}
	else
	{
		$locations = @(
			'HKLM:/SOFTWARE/Microsoft/Windows/CurrentVersion/Uninstall/7-Zip',
			'HKLM:/SOFTWARE/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall/7-Zip'
		)
		foreach ($key in $locations)
		{
			$installed = Get-ItemPropertyValue -LiteralPath $key -Name InstallLocation -ErrorAction SilentlyContinue
			if ($installed -and (Test-Path -LiteralPath (Join-Path $installed '7z.exe')))
			{
				$SevenZip = Join-Path $installed '7z.exe'
				break
			}
		}
	}
}
if (!$SevenZip -or !(Test-Path -LiteralPath $SevenZip -PathType Leaf))
{
	throw 'Provide -SevenZip with the path to 7z.exe or use an installed NSIS compiler.'
}

if (!(Test-Path -LiteralPath $archive))
{
	& curl.exe --location --fail --silent --show-error --output $archive $url
	if ($LASTEXITCODE -ne 0)
	{
		throw 'Could not download NSIS.'
	}
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $hash)
{
	throw 'NSIS SHA-256 mismatch. Remove the damaged download and retry.'
}
& $SevenZip x -y "-o$toolDir" $archive | Out-Null
if ($LASTEXITCODE -ne 0)
{
	throw '7-Zip could not extract the NSIS compiler.'
}
$compiler = Join-Path $toolDir 'makensis.exe'
& $compiler /VERSION
if ($LASTEXITCODE -ne 0)
{
	throw 'The NSIS compiler could not run.'
}
Write-Output $compiler
