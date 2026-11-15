param([switch] $Remove)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path $root 'out/test'
$marker = Join-Path $testDirectory 'plainpdf-print-queue.txt'
$name = 'Microsoft Print to PDF'
$driver = 'Microsoft Print To PDF'
$port = 'PORTPROMPT:'
$label = 'Temporary PlainPDF acceptance-test queue'

$queue = Get-Printer -Name $name -ErrorAction SilentlyContinue
if ($Remove)
{
	if ((Test-Path -LiteralPath $marker) -and $queue -and $queue.DriverName -eq $driver -and
		$queue.PortName -eq $port -and $queue.Comment -eq $label)
	{
		Remove-Printer -Name $name
		Remove-Item -LiteralPath $marker
	}
	return
}

if ($queue)
{
	if ($queue.DriverName -ne $driver -or $queue.PortName -ne $port)
	{
		throw 'A queue with this name already exists with an unexpected driver or port.'
	}
	return
}

[void] [IO.Directory]::CreateDirectory($testDirectory)
Add-Printer -Name $name -DriverName $driver -PortName $port -Comment $label
[IO.File]::WriteAllText($marker, $label)
