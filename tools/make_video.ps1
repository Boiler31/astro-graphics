# Optional: turn a PNG sequence (BlackHole.exe --recordpng DIR ...) into an mp4 with ffmpeg.
# The app can also write an mp4 directly (--record FILE.mp4, uses Windows' built-in encoder), so you
# only need this if you want to re-encode or edit the lossless frames yourself.
#   .\tools\make_video.ps1 -InputDir renders\frames -Output renders\tour.mp4 -Fps 60
param(
	[Parameter(Mandatory = $true)] [string] $InputDir,
	[string] $Output = "renders\blackhole.mp4",
	[int] $Fps = 60,
	[int] $Crf = 14
)
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
	Write-Error "ffmpeg not found on PATH (e.g. winget install Gyan.FFmpeg)"; exit 1
}
ffmpeg -y -framerate $Fps -i (Join-Path $InputDir "frame_%05d.png") -c:v libx264 -preset slow -crf $Crf -pix_fmt yuv420p -movflags +faststart $Output
