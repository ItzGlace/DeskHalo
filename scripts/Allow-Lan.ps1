# Run from an Administrator PowerShell. The rule is limited to the local subnet on Private networks.
# Undo: Remove-NetFirewallRule -DisplayName 'DeskHalo local streaming'
#Requires -RunAsAdministrator
if (-not (Get-NetFirewallRule -DisplayName 'DeskHalo local streaming' -ErrorAction SilentlyContinue)) {
    New-NetFirewallRule -DisplayName 'DeskHalo local streaming' -Direction Inbound -Action Allow -Protocol TCP -LocalPort 47654 -RemoteAddress LocalSubnet -Profile Private
}
