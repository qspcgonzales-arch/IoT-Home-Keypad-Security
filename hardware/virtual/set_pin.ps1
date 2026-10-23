# Dot-source this script in the simulator terminal. The PIN is read without echo,
# hashed in memory and not written to disk or exported to the Flutter app.
$securePin = Read-Host 'Enter a four-digit local keypad PIN' -AsSecureString
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePin)
try {
    $plainPin = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    if ($plainPin -notmatch '^\d{4}$') { throw 'PIN must contain exactly four digits.' }
    $saltBytes = New-Object byte[] 16
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($saltBytes)
    $salt = [BitConverter]::ToString($saltBytes).Replace('-', '').ToLowerInvariant()
    $inputBytes = [Text.Encoding]::UTF8.GetBytes("${salt}:$plainPin")
    $sha = [Security.Cryptography.SHA256]::Create()
    $hash = [BitConverter]::ToString($sha.ComputeHash($inputBytes)).Replace('-', '').ToLowerInvariant()
    $env:KEYPAD_PIN_SALT = $salt
    $env:KEYPAD_PIN_SHA256 = $hash
    Write-Host 'Keypad PIN hash is loaded in this PowerShell session only.'
} finally {
    if ($bstr -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
    $plainPin = $null
    $securePin.Dispose()
    if ($rng) { $rng.Dispose() }
    if ($sha) { $sha.Dispose() }
}
